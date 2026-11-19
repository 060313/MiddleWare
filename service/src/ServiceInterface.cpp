// SPDX-License-Identifier: MIT
// serviceInterface 实现（详细设计 §3.1 / §4.6 / D-27 / D-33）
//
// 分层边界：本类只处理「报文类型 + 纯载荷 JSON」，8 字节帧头由 UDSService 剥与加（D-27）。
// 本文件返回的四键外壳（ok / code / message / data）**不含帧头**，加头是 UDSService::route S4 的事。
//
// 文件内的匿名命名空间是一套**实现层私有**的 JSON 读写工具（约 200 行）：本工程不引入
// 第三方 JSON 库（少依赖、可审计），协议只用到对象 / 数组 / 字符串 / 数值 / 布尔 / null 六种形态。
// 它不是设计类，不进类清单，也不对外暴露。

#include "service/include/ServiceInterface.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

#include "agent/include/SchedManager.h"
#include "common/include/UDSService.h"

namespace sched {
namespace {

// ---------------------------------------------------------------------------
// JSON 值（保序对象：字段少，线性查找即可，换来可读的输出顺序）
// ---------------------------------------------------------------------------
struct JV {
    enum class Kind { Null, Bool, Num, Str, Obj, Arr };

    Kind kind = Kind::Null;
    bool b = false;
    double num = 0.0;
    std::string str;
    std::vector<std::pair<std::string, JV>> obj;
    std::vector<JV> arr;
};

JV jNull() { return JV{}; }

JV jBool(bool v) {
    JV j;
    j.kind = JV::Kind::Bool;
    j.b = v;
    return j;
}

JV jNum(double v) {
    JV j;
    j.kind = JV::Kind::Num;
    j.num = v;
    return j;
}

JV jStr(std::string v) {
    JV j;
    j.kind = JV::Kind::Str;
    j.str = std::move(v);
    return j;
}

JV jObj() {
    JV j;
    j.kind = JV::Kind::Obj;
    return j;
}

JV jArr() {
    JV j;
    j.kind = JV::Kind::Arr;
    return j;
}

void put(JV& object, const std::string& key, JV value) {
    object.obj.emplace_back(key, std::move(value));
}

const JV* findKey(const JV& object, const std::string& key) {
    for (const auto& kv : object.obj) {
        if (kv.first == key) {
            return &kv.second;
        }
    }
    return nullptr;
}

// ---------------------------------------------------------------------------
// 解析：递归下降，限深防嵌套炸弹；解析完必须无残留字符
// ---------------------------------------------------------------------------
constexpr int kMaxDepth = 32;

class JsonParser {
public:
    explicit JsonParser(const std::string& text) : s_(text) {}

    bool parse(JV& out) {
        skip();
        if (!parseValue(out, 0)) {
            return false;
        }
        skip();
        return i_ == s_.size();
    }

private:
    void skip() {
        while (i_ < s_.size()) {
            const char c = s_[i_];
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
                ++i_;
            } else {
                break;
            }
        }
    }

    bool parseValue(JV& v, int depth) {
        if (depth > kMaxDepth || i_ >= s_.size()) {
            return false;
        }
        switch (s_[i_]) {
        case '{':
            return parseObject(v, depth);
        case '[':
            return parseArray(v, depth);
        case '"':
            v.kind = JV::Kind::Str;
            return parseString(v.str);
        case 't':
            return parseLiteral("true", v, true);
        case 'f':
            return parseLiteral("false", v, false);
        case 'n':
            return parseNull(v);
        default:
            return parseNumber(v);
        }
    }

    bool parseObject(JV& v, int depth) {
        ++i_;                                   // '{'
        v.kind = JV::Kind::Obj;
        v.obj.clear();
        skip();
        if (i_ < s_.size() && s_[i_] == '}') {
            ++i_;
            return true;
        }
        while (true) {
            skip();
            std::string key;
            if (!parseString(key)) {
                return false;
            }
            skip();
            if (i_ >= s_.size() || s_[i_] != ':') {
                return false;
            }
            ++i_;                               // ':'
            JV child;
            if (!parseValue(child, depth + 1)) {
                return false;
            }
            v.obj.emplace_back(std::move(key), std::move(child));
            skip();
            if (i_ >= s_.size()) {
                return false;
            }
            if (s_[i_] == ',') {
                ++i_;
                continue;
            }
            if (s_[i_] == '}') {
                ++i_;
                return true;
            }
            return false;
        }
    }

    bool parseArray(JV& v, int depth) {
        ++i_;                                   // '['
        v.kind = JV::Kind::Arr;
        v.arr.clear();
        skip();
        if (i_ < s_.size() && s_[i_] == ']') {
            ++i_;
            return true;
        }
        while (true) {
            JV child;
            if (!parseValue(child, depth + 1)) {
                return false;
            }
            v.arr.push_back(std::move(child));
            skip();
            if (i_ >= s_.size()) {
                return false;
            }
            if (s_[i_] == ',') {
                ++i_;
                continue;
            }
            if (s_[i_] == ']') {
                ++i_;
                return true;
            }
            return false;
        }
    }

    bool parseString(std::string& out) {
        if (i_ >= s_.size() || s_[i_] != '"') {
            return false;
        }
        ++i_;
        out.clear();
        while (i_ < s_.size()) {
            const char c = s_[i_++];
            if (c == '"') {
                return true;
            }
            if (c != '\\') {
                out.push_back(c);
                continue;
            }
            if (i_ >= s_.size()) {
                return false;
            }
            const char e = s_[i_++];
            switch (e) {
            case '"':  out.push_back('"');  break;
            case '\\': out.push_back('\\'); break;
            case '/':  out.push_back('/');  break;
            case 'b':  out.push_back('\b'); break;
            case 'f':  out.push_back('\f'); break;
            case 'n':  out.push_back('\n'); break;
            case 'r':  out.push_back('\r'); break;
            case 't':  out.push_back('\t'); break;
            case 'u': {
                unsigned cp = 0;
                if (!parseHex4(cp)) {
                    return false;
                }
                if (cp >= 0xD800 && cp <= 0xDBFF) {         // 高代理，须跟低代理
                    if (i_ + 1 >= s_.size() || s_[i_] != '\\' || s_[i_ + 1] != 'u') {
                        return false;
                    }
                    i_ += 2;
                    unsigned lo = 0;
                    if (!parseHex4(lo) || lo < 0xDC00 || lo > 0xDFFF) {
                        return false;
                    }
                    cp = 0x10000u + ((cp - 0xD800u) << 10) + (lo - 0xDC00u);
                } else if (cp >= 0xDC00 && cp <= 0xDFFF) {  // 孤立低代理
                    return false;
                }
                appendUtf8(out, cp);
                break;
            }
            default:
                return false;
            }
        }
        return false;
    }

    bool parseHex4(unsigned& cp) {
        if (i_ + 4 > s_.size()) {
            return false;
        }
        cp = 0;
        for (int k = 0; k < 4; ++k) {
            const char c = s_[i_++];
            unsigned d = 0;
            if (c >= '0' && c <= '9') {
                d = static_cast<unsigned>(c - '0');
            } else if (c >= 'a' && c <= 'f') {
                d = static_cast<unsigned>(c - 'a' + 10);
            } else if (c >= 'A' && c <= 'F') {
                d = static_cast<unsigned>(c - 'A' + 10);
            } else {
                return false;
            }
            cp = (cp << 4) | d;
        }
        return true;
    }

    static void appendUtf8(std::string& out, unsigned cp) {
        if (cp < 0x80) {
            out.push_back(static_cast<char>(cp));
        } else if (cp < 0x800) {
            out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else if (cp < 0x10000) {
            out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else {
            out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
    }

    bool parseNumber(JV& v) {
        const char* begin = s_.c_str() + i_;
        char* end = nullptr;
        const double d = std::strtod(begin, &end);
        if (end == begin) {
            return false;
        }
        i_ += static_cast<std::size_t>(end - begin);
        v.kind = JV::Kind::Num;
        v.num = d;
        return true;
    }

    bool parseLiteral(const char* lit, JV& v, bool value) {
        const std::size_t n = std::char_traits<char>::length(lit);
        if (s_.compare(i_, n, lit) != 0) {
            return false;
        }
        i_ += n;
        v.kind = JV::Kind::Bool;
        v.b = value;
        return true;
    }

    bool parseNull(JV& v) {
        if (s_.compare(i_, 4, "null") != 0) {
            return false;
        }
        i_ += 4;
        v = JV{};
        return true;
    }

    const std::string& s_;
    std::size_t i_ = 0;
};

// ---------------------------------------------------------------------------
// 序列化
// ---------------------------------------------------------------------------
void appendEscaped(const std::string& in, std::string& out) {
    out.push_back('"');
    for (const unsigned char c : in) {
        switch (c) {
        case '"':  out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\b': out += "\\b";  break;
        case '\f': out += "\\f";  break;
        case '\n': out += "\\n";  break;
        case '\r': out += "\\r";  break;
        case '\t': out += "\\t";  break;
        default:
            if (c < 0x20) {
                char buf[8];
                std::snprintf(buf, sizeof buf, "\\u%04x", c);
                out += buf;
            } else {
                out.push_back(static_cast<char>(c));
            }
        }
    }
    out.push_back('"');
}

void appendNumber(double d, std::string& out) {
    if (!std::isfinite(d)) {            // JSON 无 NaN / Inf，落 0 保持可解析
        out += '0';
        return;
    }
    char buf[40];
    if (d == std::floor(d) && std::fabs(d) < 1e15) {
        std::snprintf(buf, sizeof buf, "%lld", static_cast<long long>(d));
    } else {
        std::snprintf(buf, sizeof buf, "%.15g", d);
    }
    out += buf;
}

void dumpJson(const JV& v, std::string& out) {
    switch (v.kind) {
    case JV::Kind::Null:
        out += "null";
        break;
    case JV::Kind::Bool:
        out += v.b ? "true" : "false";
        break;
    case JV::Kind::Num:
        appendNumber(v.num, out);
        break;
    case JV::Kind::Str:
        appendEscaped(v.str, out);
        break;
    case JV::Kind::Obj: {
        out.push_back('{');
        bool first = true;
        for (const auto& kv : v.obj) {
            if (!first) {
                out.push_back(',');
            }
            first = false;
            appendEscaped(kv.first, out);
            out.push_back(':');
            dumpJson(kv.second, out);
        }
        out.push_back('}');
        break;
    }
    case JV::Kind::Arr: {
        out.push_back('[');
        bool first = true;
        for (const auto& e : v.arr) {
            if (!first) {
                out.push_back(',');
            }
            first = false;
            dumpJson(e, out);
        }
        out.push_back(']');
        break;
    }
    }
}

// ---------------------------------------------------------------------------
// 四键外壳（§4.6.2）：ok 与 code 的一致性在此一处保证，别处不再拼外壳
// ---------------------------------------------------------------------------
std::string envelope(int code, const std::string& message, JV data) {
    JV root = jObj();
    put(root, "ok", jBool(code == 0));
    put(root, "code", jNum(code));
    put(root, "message", jStr(message));
    put(root, "data", std::move(data));
    std::string out;
    dumpJson(root, out);
    return out;
}

// ---------------------------------------------------------------------------
// Reason -> code：§4.6.3 的唯一映射表（D-33）。业务侧只给枚举，不产错误码。
// ---------------------------------------------------------------------------
const char* reasonName(Reason r) {
    switch (r) {
    case Reason::none:              return "none";
    case Reason::serviceNotRunning: return "service_not_running";
    case Reason::schedulerAbsent:   return "scheduler_absent";
    case Reason::modelNotReady:     return "model_not_ready";
    case Reason::inferenceFailed:   return "inference_failed";
    case Reason::deviceUnknown:     return "unknown_device";
    case Reason::freqUnavailable:   return "freq_unavailable";
    case Reason::writeFailed:       return "write_failed";
    }
    return "none";
}

int reasonToCode(Reason r) {
    switch (r) {
    case Reason::none:              return 0;
    case Reason::serviceNotRunning: return 1005;
    case Reason::schedulerAbsent:   return 1006;
    case Reason::modelNotReady:     return 2001;
    case Reason::inferenceFailed:   return 2002;
    case Reason::deviceUnknown:     return 3001;
    case Reason::freqUnavailable:   return 3002;
    case Reason::writeFailed:       return 3003;
    }
    return 2002;                        // 兜底归入推理失败，绝不回 0 冒充成功
}

// ---------------------------------------------------------------------------
// 载荷编码（顶层键见 §4.6.4；data 内字段由实现层定义）
// ---------------------------------------------------------------------------
JV freqTableToJson(const std::vector<int>& table) {
    JV arr = jArr();
    arr.arr.reserve(table.size());
    for (const int f : table) {
        arr.arr.push_back(jNum(f));
    }
    return arr;
}

JV actionInfoToJson(const ActionInfo& ai) {
    JV root = jObj();

    JV action = jObj();
    put(action, "device", jStr(ai.action.device));
    put(action, "freq_khz", jNum(ai.action.freqKHz));
    put(root, "action", std::move(action));

    JV result = jObj();
    put(result, "ok", jBool(ai.result.ok));
    put(result, "message", jStr(ai.result.message));
    put(result, "adjusted", jBool(ai.result.adjusted));
    put(result, "reason", jStr(reasonName(ai.result.reason)));
    put(root, "result", std::move(result));

    // §3.9：ok / message / reason 是 result 对应字段的只读镜像，协议层可直读
    put(root, "ok", jBool(ai.ok));
    put(root, "message", jStr(ai.message));
    put(root, "reason", jStr(reasonName(ai.reason)));
    return root;
}

JV snapshotToJson(const SystemStateSnapshot& snap) {
    JV root = jObj();
    JV system = jObj();

    JV cpu = jObj();
    put(cpu, "util", jNum(snap.system.cpuInfo.util));
    put(cpu, "cur_freq_khz", jNum(snap.system.cpuInfo.curFreqKHz));
    put(cpu, "freq_table", freqTableToJson(snap.system.cpuInfo.freqTable));
    put(system, "cpu", std::move(cpu));

    JV npu = jObj();
    put(npu, "util", jNum(snap.system.npuInfo.util));
    put(npu, "cur_freq_khz", jNum(snap.system.npuInfo.curFreqKHz));
    put(npu, "freq_table", freqTableToJson(snap.system.npuInfo.freqTable));
    put(system, "npu", std::move(npu));

    put(system, "u_cpu", jNum(snap.system.ucpu));
    put(system, "u_npu", jNum(snap.system.unpu));
    put(system, "cpu_freq_khz", jNum(snap.system.cpuFreqKHz));
    put(system, "npu_freq_khz", jNum(snap.system.npuFreqKHz));

    put(root, "system", std::move(system));
    put(root, "model", jStr(snap.model));
    return root;
}

// ---------------------------------------------------------------------------
// 载荷解码：只判「结构键」，不判契约键（契约键由 decisionMaker 判，见 D-28）
// ---------------------------------------------------------------------------
bool decodeTask(const std::string& payload, TaskInfo& out, std::string& err) {
    JV root;
    JsonParser parser(payload);
    if (!parser.parse(root) || root.kind != JV::Kind::Obj) {
        err = "payload is not a JSON object";
        return false;
    }

    const JV* id = findKey(root, "request_id");
    if (id == nullptr || id->kind != JV::Kind::Str || id->str.empty()) {
        err = "request_id missing, not a string, or empty";
        return false;
    }

    const JV* task = findKey(root, "task");
    if (task == nullptr || task->kind != JV::Kind::Obj) {
        err = "task missing or not an object";
        return false;
    }

    out.requestId = id->str;
    out.task.clear();
    for (const auto& kv : task->obj) {
        if (kv.second.kind != JV::Kind::Num) {
            err = "task value must be numeric: " + kv.first;
            return false;
        }
        out.task[kv.first] = kv.second.num;      // 原始物理量，归一化归 decisionMaker
    }
    return true;
}

bool decodeModelPath(const std::string& payload, std::string& out) {
    JV root;
    JsonParser parser(payload);
    if (!parser.parse(root) || root.kind != JV::Kind::Obj) {
        return false;
    }
    const JV* path = findKey(root, "model_path");
    if (path == nullptr || path->kind != JV::Kind::Str) {
        return false;                            // 必须是 JSON 对象，不接受裸字符串
    }
    out = path->str;                             // 空串合法：表示停用推理
    return true;
}

} // namespace

// ===========================================================================
// serviceInterface
// ===========================================================================
serviceInterface::serviceInterface(const std::string& socketPath,
                                   schedManager* mgr,
                                   int maxConn)
    : manager_(mgr),
      running_(false),
      socketPath_(socketPath),
      maxConn_((maxConn > 0) ? maxConn : 16) {
    // 只做赋值、不做 I/O：UDSService 是全静态类，监听由 start() 间接建立
}

serviceInterface::~serviceInterface() {
    // 兜底：析构不保证被调用，正常清理一律走显式 stop()
    if (running_) {
        running_ = false;   // 先置标志再拆资源
        UDSService::stop();
    }
    // 不删除 manager_（不拥有）
}

ActionInfo serviceInterface::taskAsk(const TaskInfo& taskInfo) {
    // 串行化：并发请求在此排队，不进入 Agent 链路（§3.1 并发语义裁决）
    std::lock_guard<std::mutex> gate(bizMutex_);

    // S1. 前置：服务已启动
    if (!running_) {
        return makeFailure(Reason::serviceNotRunning, "service not running");
    }

    // S2. 调度链未装配 → 1006（与「服务没起」区分开）
    if (manager_ == nullptr) {
        return makeFailure(Reason::schedulerAbsent, "scheduler not attached");
    }

    // S3. 转交总控
    return manager_->taskAsk(taskInfo);
}

SystemStateSnapshot serviceInterface::getSystemInfo() {
    std::lock_guard<std::mutex> gate(bizMutex_);

    // S1. 前置不满足 → 回空快照（system 零值、model 空串），
    //     成败由 handleFrame 按 1005 / 1006 表达——快照自身**不设 ok**（D-15）
    if (!running_ || manager_ == nullptr) {
        return SystemStateSnapshot{};
    }

    // S2. 原样转交
    return manager_->getSystemInfo();
}

bool serviceInterface::setModel(const std::string& modelPath) {
    std::lock_guard<std::mutex> gate(bizMutex_);

    // S1. 前置：服务已启动（装配期首次加载**不走门面**，见 D-11）
    if (!running_) {
        return false;
    }
    if (manager_ == nullptr) {
        return false;
    }

    // S2. 转交总控：由其解析模型名、先建后换
    return manager_->setModel(modelPath);
}

bool serviceInterface::start() {
    // S1. 幂等保护：重复启动视为成功（§3.1 对前置条件的宽容式放宽）
    if (running_) {
        return true;
    }
    if (socketPath_.empty()) {
        return false;
    }

    // S2. 以绑定 this 的回调启动 UDS 监听（静态调用，不创建实例）
    const bool ok = UDSService::start(
        socketPath_, maxConn_,
        [this](const std::string& type, const std::string& payload) {
            return handleFrame(type, payload);
        });
    if (!ok) {
        return false;
    }

    // S3. 监听就绪才置标志
    running_ = true;
    return true;
}

void serviceInterface::stop() {
    // S1. 幂等保护：未运行则无事可做
    if (!running_) {
        return;
    }

    // S2. 先置标志，再拆资源（避免新请求进入）
    running_ = false;

    // S3. 关端点并唤醒服务循环（不 join）
    UDSService::stop();
}

void serviceInterface::serve() {
    // S1. 前置校验：服务未启动 → 无循环可跑，直接返回（§3.1 serve S1）
    if (!running_) {
        return;
    }

    // S2. 服务线程入口：阻塞至事件循环退出（由 stop() 置标志并经 wakeFd 唤醒后返回）
    UDSService::serve();
}

std::string serviceInterface::handleFrame(const std::string& type, const std::string& payload) {
    // S1. 服务未启动 → 1005（route 正常路径不会走到这里；作为状态漂移的兜底）
    if (!running_) {
        return envelope(1005, "service not running", jNull());
    }

    // S2. TaskAsk：解码 {request_id, task} → taskAsk → ActionInfo
    if (type == "TaskAsk") {
        if (manager_ == nullptr) {
            return envelope(1006, "scheduler not attached", jNull());
        }
        TaskInfo task;
        std::string err;
        if (!decodeTask(payload, task, err)) {
            return envelope(1004, err, jNull());        // 结构错一律 1004，不断连
        }
        const ActionInfo info = taskAsk(task);
        if (!info.ok) {
            // 失败时 data = null，code 由 reason 映射（D-33），message 仅人读
            return envelope(reasonToCode(info.reason), info.message, jNull());
        }
        return envelope(0, std::string(), actionInfoToJson(info));
    }

    // S3. GetState：载荷为空或 {}，出现未知键一律忽略
    if (type == "GetState") {
        if (!payload.empty()) {
            JV root;
            JsonParser parser(payload);
            if (!parser.parse(root) || root.kind != JV::Kind::Obj) {
                return envelope(1004, "payload is not a JSON object", jNull());
            }
        }
        if (manager_ == nullptr) {
            return envelope(1006, "scheduler not attached", jNull());
        }
        const SystemStateSnapshot snap = getSystemInfo();
        return envelope(0, std::string(), snapshotToJson(snap));
    }

    // S4. SetModel：解码 {model_path}（空串=停用）→ setModel → {"model": 名称}
    if (type == "SetModel") {
        if (manager_ == nullptr) {
            return envelope(1006, "scheduler not attached", jNull());
        }
        std::string modelPath;
        if (!decodeModelPath(payload, modelPath)) {
            return envelope(1004, "model_path missing or not a string", jNull());
        }
        if (!setModel(modelPath)) {
            // 路径非法 / 加载失败 / 契约不配对，统一 2003（Reason 枚举无对应项）
            return envelope(2003, "invalid model path or contract mismatch", jNull());
        }
        std::string name;
        {
            // 单独取一次锁：setModel 内部已释放，此处不与之嵌套，避免自锁
            std::lock_guard<std::mutex> gate(bizMutex_);
            name = manager_->getModel();
        }
        JV data = jObj();
        put(data, "model", jStr(name));
        return envelope(0, std::string(), std::move(data));
    }

    // S5. 未知类型 → 1002，不断连
    return envelope(1002, "unknown message type: " + type, jNull());
}

} // namespace sched
