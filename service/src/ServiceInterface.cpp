// SPDX-License-Identifier: MIT
// serviceInterface 实现（详细设计 §3.1 / §4.6 / D-27 / D-33）
//
// 分层边界：本类只处理「报文类型 + 纯载荷 JSON」，8 字节帧头由 UDSService 剥与加（D-27）。
// 本文件返回的四键外壳（ok / code / message / data）**不含帧头**，加头是 UDSService::route S4 的事。
//
// JSON 解析与序列化统一由 common/include/Json.h（sched::json::Value）承担——解析器与
// 序列化器全项目各只有一份实现（本文件匿名命名空间原有的私有工具集已并入删除）。
// 本文件只保留业务层拼装：四键外壳与载荷编解码。

#include "service/include/ServiceInterface.h"

#include <string>
#include <utility>

#include "agent/include/SchedManager.h"
#include "common/include/Json.h"
#include "common/include/UDSService.h"

namespace sched {
namespace {

// ---------------------------------------------------------------------------
// 四键外壳（§4.6.2）：ok 与 code 的一致性在此一处保证，别处不再拼外壳
// ---------------------------------------------------------------------------
std::string envelope(int code, const std::string& message, json::Value data) {
    json::Value root = json::Value::object();
    root.set("ok", json::Value::boolean(code == 0));
    root.set("code", json::Value::number(code));
    root.set("message", json::Value::string(message));
    root.set("data", std::move(data));
    return root.dump();
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
    case Reason::refreshInfoFalse:  return "refresh_info_false";
    case Reason::getStateFalse:     return "get_state_false";
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
    case Reason::refreshInfoFalse:  return 3004;
    case Reason::getStateFalse:     return 3004;
    }
    return 2002;                        // 兜底归入推理失败，绝不回 0 冒充成功
}

// ---------------------------------------------------------------------------
// 载荷编码（顶层键见 §4.6.4；data 内字段由实现层定义）
// ---------------------------------------------------------------------------
json::Value actionInfoToJson(const ActionInfo& ai) {
    json::Value root = json::Value::object();

    json::Value action = json::Value::object();
    action.set("device", json::Value::string(ai.action.device));
    action.set("freq_khz", json::Value::number(ai.action.freqKHz));
    root.set("action", std::move(action));

    json::Value result = json::Value::object();
    result.set("ok", json::Value::boolean(ai.result.ok));
    result.set("message", json::Value::string(ai.result.message));
    result.set("reason", json::Value::string(reasonName(ai.result.reason)));
    root.set("result", std::move(result));

    // §3.9：ok / message / reason 是 result 对应字段的只读镜像，协议层可直读
    root.set("ok", json::Value::boolean(ai.ok));
    root.set("message", json::Value::string(ai.message));
    root.set("reason", json::Value::string(reasonName(ai.reason)));
    return root;
}

json::Value snapshotToJson(const SystemStateSnapshot& snap) {
    json::Value root = json::Value::object();
    json::Value system = json::Value::object();

    system.set("u_cpu", json::Value::number(snap.system.ucpu));
    system.set("u_npu", json::Value::number(snap.system.unpu));
    system.set("cpu_freq_khz", json::Value::number(snap.system.cpuFreqKHz));
    system.set("npu_freq_khz", json::Value::number(snap.system.npuFreqKHz));

    root.set("system", std::move(system));
    root.set("model", json::Value::string(snap.model));
    return root;
}

// ---------------------------------------------------------------------------
// 载荷解码：只判「结构键」，不判契约键（契约键由 decisionMaker 判，见 D-28）
// ---------------------------------------------------------------------------
bool decodeTask(const std::string& payload, TaskInfo& out, std::string& err) {
    json::Value root;
    if (!json::Value::parse(payload, root) || !root.isObject()) {
        err = "payload is not a JSON object";
        return false;
    }

    const json::Value* id = root.find("request_id");
    std::string requestId;
    if (id == nullptr || !id->isString() || !id->asString(requestId) ||
        requestId.empty()) {
        err = "request_id missing, not a string, or empty";
        return false;
    }

    const json::Value* task = root.find("task");
    if (task == nullptr || !task->isObject()) {
        err = "task missing or not an object";
        return false;
    }

    out.requestId = std::move(requestId);
    out.task.clear();
    for (const auto& kv : task->objectItems()) {
        double value = 0.0;
        if (!kv.second.asNumber(value)) {
            err = "task value must be numeric: " + kv.first;
            return false;
        }
        out.task[kv.first] = value;              // 原始物理量，归一化归 decisionMaker
    }
    return true;
}

bool decodeModelPath(const std::string& payload, std::string& out) {
    json::Value root;
    if (!json::Value::parse(payload, root) || !root.isObject()) {
        return false;
    }
    const json::Value* path = root.find("model_path");
    if (path == nullptr || !path->isString()) {
        return false;                            // 必须是 JSON 对象，不接受裸字符串
    }
    return path->asString(out);                  // 空串合法：表示停用推理
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
        return envelope(1005, "service not running", json::Value());
    }

    // S2. TaskAsk：解码 {request_id, task} → taskAsk → ActionInfo
    if (type == "TaskAsk") {
        if (manager_ == nullptr) {
            return envelope(1006, "scheduler not attached", json::Value());
        }
        TaskInfo task;
        std::string err;
        if (!decodeTask(payload, task, err)) {
            return envelope(1004, err, json::Value());   // 结构错一律 1004，不断连
        }
        const ActionInfo info = taskAsk(task);
        if (!info.ok) {
            // 失败时 data = null，code 由 reason 映射（D-33），message 仅人读
            return envelope(reasonToCode(info.reason), info.message, json::Value());
        }
        return envelope(0, std::string(), actionInfoToJson(info));
    }

    // S3. GetState：载荷为空或 {}，出现未知键一律忽略
    if (type == "GetState") {
        if (!payload.empty()) {
            json::Value root;
            if (!json::Value::parse(payload, root) || !root.isObject()) {
                return envelope(1004, "payload is not a JSON object", json::Value());
            }
        }
        if (manager_ == nullptr) {
            return envelope(1006, "scheduler not attached", json::Value());
        }
        const SystemStateSnapshot snap = getSystemInfo();
        return envelope(0, std::string(), snapshotToJson(snap));
    }

    // S4. SetModel：解码 {model_path}（空串=停用）→ setModel → {"model": 名称}
    if (type == "SetModel") {
        if (manager_ == nullptr) {
            return envelope(1006, "scheduler not attached", json::Value());
        }
        std::string modelPath;
        if (!decodeModelPath(payload, modelPath)) {
            return envelope(1004, "model_path missing or not a string", json::Value());
        }
        if (!setModel(modelPath)) {
            // 路径非法 / 加载失败 / 契约不配对，统一 2003（Reason 枚举无对应项）
            return envelope(2003, "invalid model path or contract mismatch", json::Value());
        }
        std::string name;
        {
            // 单独取一次锁：setModel 内部已释放，此处不与之嵌套，避免自锁
            std::lock_guard<std::mutex> gate(bizMutex_);
            name = manager_->getModel();
        }
        json::Value data = json::Value::object();
        data.set("model", json::Value::string(name));
        return envelope(0, std::string(), std::move(data));
    }

    // S5. 未知类型 → 1002，不断连
    return envelope(1002, "unknown message type: " + type, json::Value());
}

} // namespace sched
