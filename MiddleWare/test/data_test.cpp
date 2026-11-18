// SPDX-License-Identifier: MIT
// 数据层单元测试（详细设计 §4.3 / §4.4 / §4.5）
//
// 覆盖：Json 解析器的正/负样例、ContractFile 的契约解析与两道校验、ConfigFile 的容错口径。
// 不依赖测试框架：`make test` 编译后直接运行，退出码非 0 即失败。
//
// 说明：每个用例都用临时文件走「真实路径」调用，不做纯内存注入——
// 这样 ConfigFile / ContractFile 的 read 分支（文件缺失、空文件、BOM）也被覆盖到。

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

#include "common/include/Json.h"
#include "data/include/ConfigFile.h"
#include "data/include/ContractFile.h"
#include "agent/include/FeatureEncoder.h"
#include "service/include/MessageCodec.h"

namespace {

int gPass = 0;
int gFail = 0;

void check(bool ok, const std::string& name) {
    if (ok) {
        ++gPass;
        return;
    }
    ++gFail;
    std::printf("  FAIL  %s\n", name.c_str());
}

// 临时文件目录：默认取系统临时目录；可用 SCHEDD_TEST_TMP 覆盖。
// 覆盖的用途：Windows 上若用户目录含非 ASCII 字符，MinGW 的窄字符 ifstream
// 打不开绝对路径（Linux 无此问题），此时指向一个相对目录即可照常跑。
std::filesystem::path scratchDir() {
    const char* override = std::getenv("SCHEDD_TEST_TMP");
    if (override != nullptr && *override != '\0') {
        return std::filesystem::path(override);
    }
    return std::filesystem::temp_directory_path();
}

// 每次生成一个不重名的临时文件路径（同一次运行内不冲突即可）
std::string tempPath(const std::string& tag) {
    static int counter = 0;
    const std::filesystem::path dir = scratchDir();
    return (dir / ("schedd_test_" + std::to_string(++counter) + "_" + tag)).string();
}

std::string writeTemp(const std::string& tag, const std::string& content) {
    const std::string path = tempPath(tag);
    std::ofstream out(path, std::ios::binary);
    out.write(content.data(), static_cast<std::streamsize>(content.size()));
    return path;
}

void removePath(const std::string& path) {
    std::error_code ec;
    std::filesystem::remove(path, ec);
}

// ---------------------------------------------------------------------------
// Json：正例
// ---------------------------------------------------------------------------
void testJsonPositive() {
    std::printf("[json] 正例\n");
    using sched::json::Value;

    Value root;
    check(Value::parse(R"({"a":1,"b":[true,null,"x"],"c":{"d":1.5}})", root), "parse nested");

    const Value* a = root.find("a");
    double number = 0.0;
    check(a != nullptr && a->asNumber(number) && number == 1.0, "a == 1");

    const Value* b = root.find("b");
    check(b != nullptr && b->isArray() && b->size() == 3, "b size 3");

    const Value* nested = root.find("c");
    const Value* d = (nested != nullptr) ? nested->find("d") : nullptr;
    check(d != nullptr && d->asNumber(number) && number == 1.5, "c.d == 1.5");

    // 整数值按整数输出（频率不能被写成 408000.0）
    check(Value::number(408000.0).dump() == "408000", "dump integer");
    // 字符串转义往返
    Value quoted = Value::string("a\"b\\c\n中");
    Value back;
    check(Value::parse(quoted.dump(), back), "escaped roundtrip parse");
    std::string text;
    check(back.asString(text) && text == "a\"b\\c\n中", "escaped roundtrip value");

    // BOM 允许（Windows 记事本另存的契约）
    Value bom;
    check(Value::parse("\xEF\xBB\xBF{\"a\":1}", bom), "utf-8 bom accepted");

    // 9.0 视为 9（asInt 放宽整数值）
    Value nine;
    check(Value::parse(R"({"v":9.0})", nine), "parse 9.0");
    int iv = 0;
    const Value* vv = nine.find("v");
    check(vv != nullptr && vv->asInt(iv) && iv == 9, "asInt(9.0) == 9");

    // 空对象 / 空数组
    Value empty;
    check(Value::parse("{}", empty) && empty.isObject() && empty.size() == 0, "empty object");
}

// ---------------------------------------------------------------------------
// Json：负例（坏 JSON）
// ---------------------------------------------------------------------------
void testJsonNegative() {
    std::printf("[json] 负例\n");
    using sched::json::Value;

    std::vector<std::pair<std::string, std::string>> cases;
    cases.emplace_back("trailing comma", R"({"a":1,})");
    cases.emplace_back("single quotes", R"({'a':1})");
    cases.emplace_back("NaN", R"({"a":NaN})");
    cases.emplace_back("Infinity", R"({"a":Infinity})");
    cases.emplace_back("leading zero", R"({"a":01})");
    cases.emplace_back("bare fraction", R"({"a":.5})");
    cases.emplace_back("plus sign", R"({"a":+1})");
    cases.emplace_back("truncated object", R"({"a":1)");
    cases.emplace_back("truncated string", R"({"a":"x)");
    cases.emplace_back("trailing garbage", R"({} x)");
    cases.emplace_back("bad escape", R"({"a":"\q"})");
    cases.emplace_back("short unicode escape", R"({"a":"\u12"})");
    cases.emplace_back("lone high surrogate", R"({"a":"\ud800"})");
    cases.emplace_back("missing colon", R"({"a" 1})");
    cases.emplace_back("empty text", "");
    cases.emplace_back("unclosed array", R"([1,2)");

    // 裸控制字符（不是 \u0001 转义，而是字节 0x01 直接放在字符串里）
    std::string control = "{\"a\":\"";
    control.push_back('\x01');
    control += "\"}";
    cases.emplace_back("raw control char", control);

    // 深嵌套：超过 16 层上限，必须被拒绝而不是栈溢出
    std::string deep;
    for (int i = 0; i < 40; ++i) {
        deep.push_back('[');
    }
    deep += "1";
    for (int i = 0; i < 40; ++i) {
        deep.push_back(']');
    }
    cases.emplace_back("nesting too deep", deep);

    for (const auto& item : cases) {
        Value parsed;
        std::string error;
        const bool ok = Value::parse(item.second, parsed, &error);
        check(!ok, "reject: " + item.first);
        if (ok) {
            std::printf("        (accepted unexpectedly: %s)\n", item.first.c_str());
        }
    }

    // 失败时不得污染出参
    Value keep = Value::string("keep");
    Value out = keep;
    check(!Value::parse("{\"a\":}", out), "reject malformed");
    std::string kept;
    check(out.asString(kept) && kept == "keep", "out untouched on failure");
}

// ---------------------------------------------------------------------------
// ContractFile：解析 + 两道校验
// ---------------------------------------------------------------------------
const char* const kGoodContract = R"({
  "input_dim": 2,
  "output_dim": 2,
  "input_fields": [
    {"name": "phase", "source": "task", "raw_min": 0, "raw_max": 1, "min": 0.0, "max": 1.0},
    {"name": "ucpu",  "source": "system", "raw_min": 0, "raw_max": 1}
  ],
  "actions": [
    {"device": "cpu", "freq_khz": 408000},
    {"device": "npu", "freq_khz": 300000}
  ]
})";

void testContract() {
    std::printf("[contract]\n");
    sched::ContractFile file;

    const std::string good = writeTemp("good.json", kGoodContract);
    {
        sched::Contract contract;
        check(file.loadContract(good, contract), "load good contract");
        check(contract.inputDim == 2 && contract.outputDim == 2, "dims parsed");
        check(contract.inputFields.size() == 2 && contract.actions.size() == 2, "sizes parsed");
        check(!contract.inputFields.empty() && contract.inputFields[0].name == "phase", "field order kept");
        // min / max 缺省 [0,1]
        check(contract.inputFields.size() == 2 && contract.inputFields[1].min == 0.0 &&
                  contract.inputFields[1].max == 1.0,
              "min/max default");
        check(contract.actions.size() == 2 && contract.actions[0].device == "cpu" &&
                  contract.actions[0].freqKHz == 408000,
              "action 0");
        check(file.contractValidate(good), "validate good contract");
    }
    removePath(good);

    // 维度不自洽：解析成功、校验失败（第 1、2 道校验的职责边界）
    {
        const std::string path = writeTemp("dim_mismatch.json",
            R"({"input_dim":3,"output_dim":1,"input_fields":[{"name":"a","source":"task","raw_min":0,"raw_max":1}],"actions":[{"device":"cpu","freq_khz":1}]})");
        sched::Contract contract;
        check(file.loadContract(path, contract), "dim mismatch still parses");
        check(!file.contractValidate(path), "dim mismatch rejected by validate");
        removePath(path);
    }

    // 各类坏契约
    std::vector<std::pair<std::string, std::string>> bad;
    bad.emplace_back("missing actions",
        R"({"input_dim":0,"output_dim":0,"input_fields":[]})");
    bad.emplace_back("missing input_fields",
        R"({"input_dim":0,"output_dim":0,"actions":[]})");
    bad.emplace_back("input_dim is string",
        R"({"input_dim":"2","output_dim":0,"input_fields":[],"actions":[]})");
    bad.emplace_back("input_fields is object",
        R"({"input_dim":0,"output_dim":0,"input_fields":{},"actions":[]})");
    bad.emplace_back("field missing raw_max",
        R"({"input_dim":1,"output_dim":0,"input_fields":[{"name":"a","source":"task","raw_min":0}],"actions":[]})");
    bad.emplace_back("field reversed range",
        R"({"input_dim":1,"output_dim":0,"input_fields":[{"name":"a","source":"task","raw_min":9,"raw_max":1}],"actions":[]})");
    bad.emplace_back("unknown source",
        R"({"input_dim":1,"output_dim":0,"input_fields":[{"name":"a","source":"gpu","raw_min":0,"raw_max":1}],"actions":[]})");
    bad.emplace_back("action freq is string",
        R"({"input_dim":0,"output_dim":1,"input_fields":[],"actions":[{"device":"cpu","freq_khz":"408000"}]})");
    bad.emplace_back("action freq negative",
        R"({"input_dim":0,"output_dim":1,"input_fields":[],"actions":[{"device":"cpu","freq_khz":-1}]})");
    bad.emplace_back("action device missing",
        R"({"input_dim":0,"output_dim":1,"input_fields":[],"actions":[{"freq_khz":408000}]})");
    bad.emplace_back("truncated json",
        R"({"input_dim":0,"output_dim":0,"input_fields":[],"actions":[)");

    for (const auto& item : bad) {
        const std::string path = writeTemp("bad.json", item.second);
        sched::Contract contract;
        check(!file.loadContract(path, contract), "reject contract: " + item.first);
        removePath(path);
    }

    // 不存在的文件 / 空路径
    {
        sched::Contract contract;
        check(!file.loadContract("", contract), "reject empty path");
        check(!file.loadContract(
                  (std::filesystem::temp_directory_path() / "schedd_no_such_file.json").string(),
                  contract),
              "reject missing file");
    }
}

// ---------------------------------------------------------------------------
// ConfigFile：容错口径（§4.3.4）
// ---------------------------------------------------------------------------
void testConfig() {
    std::printf("[config]\n");
    sched::ConfigFile config;

    // 注意：故意混用 CRLF 与注释，验证行尾与注释处理
    const std::string text =
        "# 注释行\r\n"
        "; 另一种注释\r\n"
        "[server]\r\n"
        "socket_path = /run/schedd.sock\r\n"
        "max_conn    = 16\r\n"
        "\r\n"
        "[agent]\r\n"
        "model_file  = dqn.onnx\r\n"
        "\r\n"
        "[system]\r\n"
        "cpu_freq_min = 0\r\n"
        "cpu_freq_max = 0\r\n"
        "npu_freq_min = 0\r\n"
        "npu_freq_max = 0\r\n"
        "cpu_freq_path =\r\n"
        "npu_freq_path =\r\n"
        "proc_stat_path =\r\n"
        "sample_interval_ms = abc\r\n";

    const std::string path = writeTemp("schedd.conf", text);
    check(config.getConfig(path), "getConfig ok");
    check(config.configValidate(path), "validate ok (bad int is not an error)");

    check(config.getString("server", "socket_path", "") == "/run/schedd.sock", "crlf trimmed");
    check(config.getInt("server", "max_conn", 0) == 16, "int value");
    check(config.getInt("system", "sample_interval_ms", 500) == 500, "illegal int falls back");
    check(config.getString("system", "cpu_freq_path", "/sys/default") == "/sys/default",
          "empty string falls back");
    check(config.has("system", "cpu_freq_path"), "has() true for empty value");
    check(!config.has("system", "no_such_key"), "has() false for missing key");
    check(config.getString("nope", "x", "def") == "def", "missing section falls back");

    // 缺 [agent] 节：校验失败，但解析本身成功（取值容错）
    const std::string missing = writeTemp("missing.conf",
        "[server]\nsocket_path=/run/schedd.sock\nmax_conn=16\n"
        "[system]\n"
        "cpu_freq_min=0\ncpu_freq_max=0\nnpu_freq_min=0\nnpu_freq_max=0\n"
        "cpu_freq_path=\nnpu_freq_path=\nproc_stat_path=\nsample_interval_ms=500\n");
    sched::ConfigFile another;
    check(another.getConfig(missing), "getConfig ok without [agent]");
    check(!another.configValidate(missing), "validate fails without [agent]");
    check(another.getString("agent", "model_file", "") == "", "missing agent key falls back");

    removePath(path);
    removePath(missing);
}

// ---------------------------------------------------------------------------
// 可选：校验仓库自带的示例契约（argv[1]）
// ---------------------------------------------------------------------------
void testSample(const char* samplePath) {
    if (samplePath == nullptr) {
        return;
    }
    std::printf("[sample] %s\n", samplePath);
    sched::ContractFile file;
    sched::Contract contract;
    check(file.loadContract(samplePath, contract), "sample parses");
    check(file.contractValidate(samplePath), "sample self-consistent");
    check(contract.inputDim == static_cast<int>(contract.inputFields.size()), "sample dim matches fields");
    check(contract.outputDim == static_cast<int>(contract.actions.size()), "sample dim matches actions");
}

// ---------------------------------------------------------------------------
// MessageCodec：四键外壳 / Reason→code / 报文编解码（§4.6）
// ---------------------------------------------------------------------------
void testCodec() {
    std::printf("[codec]\n");
    using sched::json::Value;

    // Reason → code（D-33 唯一映射点）
    check(sched::reasonToCode(sched::Reason::none) == 0, "none -> 0");
    check(sched::reasonToCode(sched::Reason::serviceNotRunning) == 1005, "-> 1005");
    check(sched::reasonToCode(sched::Reason::schedulerAbsent) == 1006, "-> 1006");
    check(sched::reasonToCode(sched::Reason::modelNotReady) == 2001, "-> 2001");
    check(sched::reasonToCode(sched::Reason::inferenceFailed) == 2002, "-> 2002");
    check(sched::reasonToCode(sched::Reason::deviceUnknown) == 3001, "-> 3001");
    check(sched::reasonToCode(sched::Reason::freqUnavailable) == 3002, "-> 3002");
    check(sched::reasonToCode(sched::Reason::writeFailed) == 3003, "-> 3003");

    // 四键外壳：失败 data=null，四键恒在
    {
        const std::string frame = sched::makeEnvelope(false, 2001, "model not ready", nullptr);
        Value root;
        check(Value::parse(frame, root) && root.isObject(), "envelope parses");
        check(root.find("ok") && root.find("code") && root.find("message") && root.find("data"),
              "four keys present");
        bool ok = false;
        const Value* v = root.find("ok");
        check(v != nullptr && v->asBool(ok) && !ok, "ok == false");
        const Value* d = root.find("data");
        check(d != nullptr && d->isNull(), "data null on failure");
    }

    // 四键外壳：成功带 data
    {
        Value data = Value::object();
        data.set("model", Value::string("dqn"));
        const std::string frame = sched::makeEnvelope(true, 0, "", &data);
        Value root;
        Value::parse(frame, root);
        const Value* d = root.find("data");
        check(d != nullptr && d->isObject() && d->find("model") != nullptr, "data present on success");
    }

    // decodeTaskAsk
    {
        sched::TaskInfo task;
        check(sched::decodeTaskAsk(R"({"request_id":"r1","task":{"phase":0,"prompt_len":512}})", task),
              "decode task ok");
        check(task.requestId == "r1" && task.task.size() == 2 && task.task["prompt_len"] == 512.0,
              "task fields parsed");
        check(!sched::decodeTaskAsk("", task), "reject empty payload");
        check(!sched::decodeTaskAsk(R"({"request_id":"r1"})", task), "reject missing task");
        check(!sched::decodeTaskAsk(R"({"request_id":"","task":{}})", task), "reject empty request_id");
        check(!sched::decodeTaskAsk(R"({"request_id":"r1","task":{"x":"str"}})", task),
              "reject non-numeric task value");
        check(!sched::decodeTaskAsk("not json", task), "reject non-json");
        check(!sched::decodeTaskAsk(R"({"request_id":"r1","task":[]})", task),
              "reject task not object");
    }

    // decodeSetModel
    {
        std::string path;
        check(sched::decodeSetModel(R"({"model_path":"/x/dqn.onnx"})", path) && path == "/x/dqn.onnx",
              "decode setmodel");
        check(sched::decodeSetModel(R"({"model_path":""})", path) && path.empty(),
              "setmodel empty path ok (disable)");
        check(!sched::decodeSetModel(R"("/x/dqn.onnx")", path), "setmodel reject bare string");
        check(!sched::decodeSetModel(R"({})", path), "setmodel reject missing key");
    }

    // encodeActionInfo
    {
        sched::ActionInfo info;
        info.action.device = "cpu";
        info.action.freqKHz = 408000;
        info.result.ok = true;
        info.result.adjusted = false;
        info.result.message = "ok";
        Value v = sched::encodeActionInfo(info);
        const Value* act = v.find("action");
        const Value* res = v.find("result");
        check(act != nullptr && res != nullptr, "actioninfo has action + result");
        const Value* dev = act != nullptr ? act->find("device") : nullptr;
        std::string s;
        check(dev != nullptr && dev->asString(s) && s == "cpu", "action device");
        const Value* fk = act != nullptr ? act->find("freq_khz") : nullptr;
        int freq = 0;
        check(fk != nullptr && fk->asInt(freq) && freq == 408000, "action freq_khz");
        const Value* okv = res != nullptr ? res->find("ok") : nullptr;
        bool b = false;
        check(okv != nullptr && okv->asBool(b) && b, "result ok");
    }

    // encodeSnapshot
    {
        sched::SystemStateSnapshot snap;
        snap.system.cpuInfo.util = 0.5;
        snap.system.cpuInfo.curFreqKHz = 1800000;
        snap.system.cpuInfo.freqTable = {408000, 1008000};
        snap.system.npuInfo.util = 0.25;
        snap.system.ucpu = 0.5;
        snap.system.unpu = 0.25;
        snap.system.cpuFreqKHz = 1800000;
        snap.system.npuFreqKHz = 600000;
        snap.model = "dqn";
        Value v = sched::encodeSnapshot(snap);
        const Value* sys = v.find("system");
        const Value* cpu = sys != nullptr ? sys->find("cpu") : nullptr;
        const Value* ft = cpu != nullptr ? cpu->find("freq_table") : nullptr;
        check(sys != nullptr && cpu != nullptr && ft != nullptr, "snapshot structure");
        check(ft != nullptr && ft->isArray() && ft->size() == 2, "freq_table len");
        const Value* model = v.find("model");
        std::string m;
        check(model != nullptr && model->asString(m) && m == "dqn", "snapshot model");
    }
}

// ---------------------------------------------------------------------------
// FeatureEncoder：契约驱动的特征编码 + 归一化（§4.4 D-28 / D-30）
// ---------------------------------------------------------------------------
void testFeatureEncoding() {
    std::printf("[feature]\n");

    sched::Contract contract;
    contract.inputDim = 2;
    contract.outputDim = 1;
    contract.inputFields = {
        {"phase", "task", 0.0, 1.0, 0.0, 1.0},
        {"ucpu", "system", 0.0, 1.0, 0.0, 1.0},
    };
    contract.actions = {{"cpu", 408000}};

    sched::TaskInfo task;
    task.requestId = "r";
    task.task["phase"] = 0.5;
    sched::SystemInfo sysInfo;
    sysInfo.ucpu = 0.75;

    std::vector<float> out;
    check(sched::encodeFeatures(contract, task, sysInfo, out), "encode ok");
    check(out.size() == 2, "two features");
    check(out.size() == 2 && out[0] > 0.49f && out[0] < 0.51f, "phase -> 0.5");
    check(out.size() == 2 && out[1] > 0.74f && out[1] < 0.76f, "ucpu -> 0.75");

    // 契约要求而载荷未提供 → false（映射 2002）
    sched::TaskInfo missing;
    missing.requestId = "r";
    check(!sched::encodeFeatures(contract, missing, sysInfo, out), "missing task key rejected");

    // 未知系统分量名 → false
    sched::Contract bad;
    bad.inputDim = 1;
    bad.outputDim = 1;
    bad.inputFields = {{"gpu_util", "system", 0.0, 1.0, 0.0, 1.0}};
    bad.actions = {{"cpu", 408000}};
    check(!sched::encodeFeatures(bad, task, sysInfo, out), "unknown system feature rejected");

    // 量程退化 rawMin == rawMax → 中值 0.5（不除零）
    sched::Contract degenerate;
    degenerate.inputDim = 1;
    degenerate.outputDim = 1;
    degenerate.inputFields = {{"kv_ratio", "task", 0.0, 0.0, 0.0, 1.0}};
    degenerate.actions = {{"cpu", 408000}};
    sched::TaskInfo dtask;
    dtask.task["kv_ratio"] = 0.9;
    std::vector<float> dout;
    check(sched::encodeFeatures(degenerate, dtask, sysInfo, dout) &&
              dout.size() == 1 && dout[0] > 0.49f && dout[0] < 0.51f,
          "degenerate range -> 0.5");

    // 越界夹取到 [0,1]
    sched::Contract wide;
    wide.inputDim = 1;
    wide.outputDim = 1;
    wide.inputFields = {{"prompt_len", "task", 0.0, 100.0, 0.0, 1.0}};
    wide.actions = {{"cpu", 408000}};
    sched::TaskInfo wtask;
    wtask.task["prompt_len"] = 500.0;
    std::vector<float> wout;
    check(sched::encodeFeatures(wide, wtask, sysInfo, wout) &&
              wout.size() == 1 && wout[0] == 1.0f,
          "clamp high to 1.0");

    // 多余任务键忽略（不影响编码）
    sched::TaskInfo extra;
    extra.requestId = "r";
    extra.task["phase"] = 0.5;
    extra.task["junk"] = 123.0;
    std::vector<float> eout;
    check(sched::encodeFeatures(contract, extra, sysInfo, eout) && eout.size() == 2,
          "extra task key ignored");
}

} // namespace

int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);   // 崩溃时也能看到进度
    testJsonPositive();
    testJsonNegative();
    testContract();
    testConfig();
    testCodec();
    testFeatureEncoding();
    testSample(argc > 1 ? argv[1] : nullptr);

    std::printf("\n%d passed, %d failed\n", gPass, gFail);
    return (gFail == 0) ? 0 : 1;
}
