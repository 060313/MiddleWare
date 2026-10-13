// B1 批次验收测试 —— 拓扑层 L0 的叶子节点（《软件详细设计文档》6.6.4）。
//
// 覆盖 B1 验收行要求的三条硬断言：
//   ① RequestCodec：同帧解码等于原值；截断帧 / 粘包帧 / 未知类型必须返回 false；
//   ② ConfigLoader：缺键取默认；留空与非法整数一律退回默认；
//   ③ FeatureEncoder / ActionDecoder 与 model_contract.json 的 input_dim=8 / output_dim=5
//      逐项对齐（含归一化分量顺序与动作展开结果）。
//
// 运行前提：工作目录为项目根（ctest 通过 WORKING_DIRECTORY 保证），
// 因为契约与配置按相对路径读取。

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "ActionDecoder.h"
#include "ConfigLoader.h"
#include "FeatureEncoder.h"
#include "Json.h"
#include "OutputFormatter.h"
#include "RequestCodec.h"
#include "SystemFS.h"
#include "SystemdCtl.h"

namespace {

int g_failed = 0;

void check(const char* name, bool ok) {
    std::printf("[%s] %s\n", ok ? "PASS" : "FAIL", name);
    if (!ok) {
        ++g_failed;
    }
}

void checkNear(const char* name, double got, double want) {
    const bool ok = std::fabs(got - want) <= 1e-5;
    std::printf("[%s] %-34s got=%.6f want=%.6f\n", ok ? "PASS" : "FAIL", name, got, want);
    if (!ok) {
        ++g_failed;
    }
}

constexpr const char* kContractPath = "model/model_contract.json";
constexpr const char* kConfPath = "deploy/schedd.conf";

// 独立于实现重新算一遍"某个字节位置落在第几显示列"。
// 故意不复用 OutputFormatter.cpp 里的 displayWidth：测的就是列对齐，
// 若测试与被测代码共用同一份实现，对齐错了也测不出来。
std::size_t labelColumn(const std::string& line) {
    std::size_t column = 0;
    std::size_t i = 0;
    while (i < line.size() && line[i] != ':') {
        const unsigned char lead = static_cast<unsigned char>(line[i]);
        std::size_t length = 1;
        if ((lead & 0xE0u) == 0xC0u) {
            length = 2;
        } else if ((lead & 0xF0u) == 0xE0u) {
            length = 3;
        } else if ((lead & 0xF8u) == 0xF0u) {
            length = 4;
        }
        // 3 字节序列且首字节落在 U+4E00~U+9FFF 的 UTF-8 区间 → 汉字，占 2 列
        const bool wide = (length == 3 && lead >= 0xE4u && lead <= 0xE9u);
        column += wide ? 2u : 1u;
        i += length;
    }
    return column;
}

// ---------------------------------------------------------------- common::Json

void testJson() {
    std::printf("\n-- common::Json --\n");

    common::Json doc;
    check("parse nested document", common::Json::parse(R"({"a":1,"b":[true,null,"x"]})", doc));
    check("int value", doc["a"].asInt() == 1);
    check("array size", doc["b"].size() == 3);
    check("nested bool", doc["b"][0].asBool());
    check("nested null", doc["b"][1].isNull());
    check("nested string", doc["b"][2].asString() == "x");
    check("missing key -> null", doc["nope"].isNull());
    check("out of range index -> null", doc["b"][9].isNull());

    common::Json round;
    check("dump/parse roundtrip",
          common::Json::parse(doc.dump(), round) && round["a"].asInt() == 1 &&
              round["b"][2].asString() == "x");

    check("reject truncated", !common::Json::parse(R"({"a":)", doc));
    check("reject trailing garbage", !common::Json::parse(R"({"a":1} junk)", doc));
    check("reject unquoted key", !common::Json::parse("{a:1}", doc));
    check("reject lone surrogate", !common::Json::parse(R"("\uD800")", doc));

    common::Json esc;
    check("escape sequences", common::Json::parse(R"("a\nb\t\"c\"")", esc) &&
                                  esc.asString() == "a\nb\t\"c\"");
    check("unicode escape to utf8", common::Json::parse(R"("\u4e2d")", esc) &&
                                       esc.asString() == "\xE4\xB8\xAD");
    check("negative number", common::Json::parse("-0.5", esc) &&
                                 std::fabs(esc.asNumber() + 0.5) < 1e-12);

    check("dump integer without decimal point", common::Json(1800000).dump() == "1800000");
    check("dump keeps fraction", common::Json(0.25).dump() == "0.25");
    check("type mismatch -> default", common::Json(1).asString("fallback") == "fallback");
}

// ---------------------------------------------------------------- ConfigLoader

void testConfigLoader() {
    std::printf("\n-- common::ConfigLoader --\n");

    common::ConfigLoader conf;
    check("load deploy/schedd.conf", conf.load(kConfPath));
    check("string value", conf.getString("server", "socket_path", "") == "/run/schedd.sock");
    check("string value in another section",
          conf.getString("agent", "default_mode", "") == "auto");
    check("int value", conf.getInt("server", "max_conn", 0) == 16);
    check("int value with comment above", conf.getInt("system", "sample_interval_ms", 0) == 500);

    // 留空 = "未配置，启动时从硬件读取"：必须退回默认值，不能被当成 0
    check("empty value -> default", conf.getInt("system", "cpu_freq_min", -1) == -1);
    check("has() still true for empty value", conf.has("system", "cpu_freq_min"));

    check("missing key -> default", conf.getString("server", "nope", "fallback") == "fallback");
    check("missing section -> default", conf.getInt("nope", "nope", 7) == 7);
    check("has on missing", !conf.has("server", "nope"));

    // 非法字面量：写一份临时配置比对
    const std::string tmp = sched::SystemFS::tempDir() + "/b1_test.conf";
    const bool written = sched::SystemFS::write(
        tmp, "# tmp\n[s]\nnum = abc\nempty =\ntrailing = 12abc\nok = 42\nnegative = -8\n");
    check("write temp conf", written);

    common::ConfigLoader bad;
    check("load temp conf", bad.load(tmp));
    check("non numeric -> default", bad.getInt("s", "num", -7) == -7);
    check("trailing garbage -> default", bad.getInt("s", "trailing", -7) == -7);
    check("empty -> default", bad.getInt("s", "empty", -7) == -7);
    check("valid -> value", bad.getInt("s", "ok", -7) == 42);
    check("negative -> value", bad.getInt("s", "negative", -7) == -8);

    // 不可读文件：返回 false 且不清空已有内容
    check("unreadable file -> false", !bad.load("deploy/__no_such__.conf"));
    check("failed load keeps table", bad.getInt("s", "ok", -7) == 42);

    bad.clear();
    check("clear empties table", !bad.has("s", "ok"));
}

// ---------------------------------------------------------------- RequestCodec

void testRequestCodec() {
    std::printf("\n-- proto::RequestCodec --\n");

    proto::RequestCodec codec;
    const std::string payload = R"({"taskId":7,"batchSize":4})";
    const std::string frame = codec.encode(common::MessageType::AskSched, payload);

    check("frame size = header + payload",
          frame.size() == payload.size() + proto::RequestCodec::kHeaderSize);
    check("length prefix counts the whole frame",
          static_cast<unsigned char>(frame[0]) == 0 &&
              static_cast<unsigned char>(frame[3]) == frame.size());
    check("type header is big endian",
          static_cast<unsigned char>(frame[7]) ==
              static_cast<unsigned char>(common::MessageType::AskSched));

    common::MessageType type = common::MessageType::Unknown;
    std::string decoded;
    check("decode ok", codec.decode(frame, type, decoded));
    check("type roundtrip", type == common::MessageType::AskSched);
    check("payload roundtrip", decoded == payload);

    // 失败必须不动出参
    common::MessageType keepType = common::MessageType::GetState;
    std::string keepPayload = "sentinel";
    check("truncated frame -> false",
          !codec.decode(frame.substr(0, frame.size() - 1), keepType, keepPayload));
    check("truncated keeps out params",
          keepType == common::MessageType::GetState && keepPayload == "sentinel");
    check("short frame -> false", !codec.decode("abc", keepType, keepPayload));
    check("empty frame -> false", !codec.decode(std::string(), keepType, keepPayload));
    // 粘包：两帧拼一起整体当一帧读，长度前缀与实到字节数不符 → 必须拒绝
    check("concatenated frames -> false", !codec.decode(frame + frame, keepType, keepPayload));
    // 未知类型
    check("unknown type -> false",
          !codec.decode(codec.encode(static_cast<common::MessageType>(99), "x"), keepType,
                        keepPayload));
    // 空载荷合法（GetState 的载荷就是空的）
    common::MessageType emptyType = common::MessageType::Unknown;
    std::string emptyPayload = "sentinel";
    check("empty payload ok",
          codec.decode(codec.encode(common::MessageType::GetState, ""), emptyType, emptyPayload) &&
              emptyType == common::MessageType::GetState && emptyPayload.empty());

    // ---- 载荷序列化 ----
    common::ActionInfo info;
    info.taskId = 123;
    info.ok = true;
    info.actionIndex = 2;
    common::ResultInfo result;
    result.device = "npu";
    result.ok = true;
    result.adjusted = true;
    result.appliedFreqKHz = 1000000;
    info.results.push_back(result);

    common::Json doc;
    const std::string infoJson = codec.encodeActionInfo(info);
    check("encodeActionInfo is valid json", common::Json::parse(infoJson, doc));
    check("actioninfo taskId", doc["taskId"].asInt() == 123);
    check("actioninfo ok", doc["ok"].asBool());
    check("actioninfo results size", doc["results"].size() == 1);
    check("actioninfo result device", doc["results"][0]["device"].asString() == "npu");
    check("actioninfo result adjusted", doc["results"][0]["adjusted"].asBool());
    check("actioninfo result freq", doc["results"][0]["appliedFreqKHz"].asInt() == 1000000);

    common::SystemFeature feature;
    feature.ucpu = 0.5f;
    feature.unpu = {0.1f, 0.2f, 0.3f};
    feature.cpuFreqKHz = 1800000;
    feature.npuFreqKHz = 1000000;
    feature.stale = true;

    const std::string featureJson = codec.encodeSystemFeature(feature);
    check("encodeSystemFeature is valid json", common::Json::parse(featureJson, doc));
    check("sysfeat unpu is array of 3", doc["unpu"].size() == 3);
    check("sysfeat unpu[1]", std::fabs(doc["unpu"][1].asNumber() - 0.2) < 1e-5);
    check("sysfeat cpuFreqKHz", doc["cpuFreqKHz"].asInt() == 1800000);
    check("sysfeat stale", doc["stale"].asBool());
}

// ---------------------------------------------------------------- FeatureEncoder / ActionDecoder

void testAgentContract() {
    std::printf("\n-- agent::FeatureEncoder / ActionDecoder --\n");

    agent::FeatureEncoder encoder;
    check("loadContract", encoder.loadContract(kContractPath));
    check("input_dim == 8 (与契约对齐)", encoder.inputDim() == 8);

    common::TaskFeature task;
    task.batchSize = 8;
    task.seqLen = 2048;
    common::SystemFeature sys;
    sys.ucpu = 0.5f;
    sys.unpu = {0.25f, 0.5f, 1.0f};
    sys.cpuFreqKHz = 1800000;
    sys.npuFreqKHz = 1000000;

    const std::vector<float> v = encoder.encode(task, sys);
    check("encode returns input_dim elements", v.size() == 8);
    checkNear("  [0] ucpu", v[0], 0.5);
    checkNear("  [1] unpu_core0", v[1], 0.25);
    checkNear("  [2] unpu_core1", v[2], 0.5);
    checkNear("  [3] unpu_core2", v[3], 1.0);
    checkNear("  [4] cpu_freq_ratio", v[4], 1.0);
    checkNear("  [5] npu_freq_ratio", v[5], 1.0);
    checkNear("  [6] batch_size_norm", v[6], 1.0);
    checkNear("  [7] seq_len_norm", v[7], 1.0);

    // 半档频率
    sys.cpuFreqKHz = 900000;
    sys.npuFreqKHz = 500000;
    task.batchSize = 4;
    task.seqLen = 1024;
    const std::vector<float> half = encoder.encode(task, sys);
    checkNear("  cpu_freq_ratio half", half[4], 0.5);
    checkNear("  npu_freq_ratio half", half[5], 0.5);
    checkNear("  batch_size_norm half", half[6], 0.5);
    checkNear("  seq_len_norm half", half[7], 0.5);

    // 越界与负数必须被夹到 [0,1]，否则会喂给模型一个训练时没见过的输入分布
    sys.ucpu = 1.8f;
    sys.unpu = {-0.5f, 2.0f, 0.0f};
    sys.cpuFreqKHz = 3600000;
    task.batchSize = 64;
    task.seqLen = -16;
    const std::vector<float> clamped = encoder.encode(task, sys);
    checkNear("  clamp ucpu 1.8 -> 1.0", clamped[0], 1.0);
    checkNear("  clamp unpu[0] -0.5 -> 0.0", clamped[1], 0.0);
    checkNear("  clamp unpu[1] 2.0 -> 1.0", clamped[2], 1.0);
    checkNear("  clamp cpu_freq_ratio", clamped[4], 1.0);
    checkNear("  clamp batch_size_norm", clamped[6], 1.0);
    checkNear("  clamp seq_len_norm (negative)", clamped[7], 0.0);

    // 契约缺失必须返回 false，由上层进入降级
    agent::FeatureEncoder missing;
    check("loadContract on missing file -> false",
          !missing.loadContract("model/__no_such__.json"));
    check("failed loadContract keeps inputDim", missing.inputDim() == 0);

    agent::ActionDecoder decoder;
    check("loadContract", decoder.loadContract(kContractPath));
    check("output_dim == 5 (与契约对齐)", decoder.actionCount() == 5);

    // argmax：最大值在 2 号 → 契约里的 npu_heavy（batch 4 / cpu 档 1 / npu 档 2）
    const std::vector<float> q = {0.1f, 0.2f, 0.9f, 0.3f, 0.4f};
    const common::Decision decision = decoder.decode(q);
    check("actionIndex = argmax", decision.actionIndex == 2);
    checkNear("qValue", decision.qValue, 0.9);
    check("expanded to 2 device actions", decision.actions.size() == 2);
    check("npu action first", decision.actions[0].device == "npu");
    check("npu freq = level2 = 1000 MHz", decision.actions[0].freqKHz == 1000000);
    check("npu batch from contract", decision.actions[0].batchSize == 4);
    check("cpu action second", decision.actions[1].device == "cpu");
    check("cpu freq = level1 = 1200 MHz", decision.actions[1].freqKHz == 1200000);

    // 0 号动作 = low（cpu 档 0 / npu 档 0）
    const common::Decision low = decoder.decode(std::vector<float>{1.0f, 0.0f, 0.0f, 0.0f, 0.0f});
    check("action0 -> cpu 600 MHz", low.actions[1].freqKHz == 600000);
    check("action0 -> npu 600 MHz", low.actions[0].freqKHz == 600000);

    // 维度不符 / 空输入 → 无效决策，绝不能猜一个动作出来
    check("wrong q size -> invalid",
          decoder.decode(std::vector<float>{1.0f, 2.0f}).actionIndex == -1);
    check("empty q -> invalid", decoder.decode(std::vector<float>{}).actionIndex == -1);
    check("invalid decision has no actions",
          decoder.decode(std::vector<float>{1.0f, 2.0f}).actions.empty());

    // 并列时取序号小者，保证同一 Q 向量解码结果稳定
    check("tie -> lowest index",
          decoder.decode(std::vector<float>{0.5f, 0.5f, 0.1f, 0.1f, 0.1f}).actionIndex == 0);
}

// ---------------------------------------------------------------- OutputFormatter

void testOutputFormatter() {
    std::printf("\n-- cli::OutputFormatter --\n");

    common::SystemFeature sys;
    sys.ucpu = 0.427f;
    sys.unpu = {0.38f, 0.41f, 0.35f};
    sys.cpuFreqKHz = 1800000;
    sys.npuFreqKHz = 1000000;

    cli::OutputFormatter table;
    const std::string text = table.formatState(sys);
    check("table has cpu usage", text.find("42.7%") != std::string::npos);
    check("table has all three cores", text.find("core0") != std::string::npos &&
                                           text.find("core1") != std::string::npos &&
                                           text.find("core2") != std::string::npos);
    check("table has cpu freq in MHz", text.find("CPU 1800 MHz") != std::string::npos);
    check("table has npu freq in MHz", text.find("NPU 1000 MHz") != std::string::npos);
    check("table marks fresh state", text.find("正常") != std::string::npos);

    // 列对齐：四行的 ':' 必须落在同一**显示列**上。
    // 注意不能用字节下标判定 —— "CPU 利用率" 13 字节 / 10 列，与 "频率" 6 字节 / 4 列
    // 的字节下标天然不同，但显示列必须对齐，这正是 padRight 存在的意义。
    std::size_t firstColumn = 0;
    std::size_t lineCount = 0;
    bool aligned = true;
    for (std::size_t pos = 0; pos < text.size();) {
        const std::size_t newline = text.find('\n', pos);
        const std::string line =
            text.substr(pos, newline == std::string::npos ? std::string::npos : newline - pos);
        if (!line.empty()) {
            const std::size_t column = labelColumn(line);
            if (lineCount == 0) {
                firstColumn = column;
            } else if (column != firstColumn) {
                aligned = false;
            }
            ++lineCount;
        }
        if (newline == std::string::npos) {
            break;
        }
        pos = newline + 1;
    }
    // "CPU 利用率" 的显示宽度正好是 10（3 + 1 + 3×2），padRight 补到该宽度后
    // ':' 紧随其后 → 四行的 ':' 都应落在第 10 列（0 起）。
    check("label columns aligned across 4 lines",
          aligned && lineCount == 4 && firstColumn == 10);

    sys.stale = true;
    check("table highlights stale", table.formatState(sys).find("陈旧") != std::string::npos);

    cli::OutputFormatter json;
    json.setJson(true);
    common::Json doc;
    check("json output parses", common::Json::parse(json.formatState(sys), doc));
    check("json carries stale", doc["stale"].asBool());
    check("json carries unpu", doc["unpu"].size() == 3);

    check("formatMode", table.formatMode(common::SchedMode::Auto) == std::string("auto"));
    check("formatMode static", table.formatMode(common::SchedMode::Static) == std::string("static"));
    check("formatMode off", table.formatMode(common::SchedMode::Off) == std::string("off"));

    common::ActionInfo info;
    info.taskId = 5;
    info.ok = false;
    info.actionIndex = 2;
    info.message = "degraded: model unavailable";
    common::ResultInfo ok;
    ok.device = "npu";
    ok.ok = true;
    ok.appliedFreqKHz = 1000000;
    common::ResultInfo failed;
    failed.device = "cpu";
    failed.ok = false;
    failed.message = "写入失败";
    info.results.push_back(ok);
    info.results.push_back(failed);

    const std::string action = table.formatActionResult(info);
    check("action result has overall message", action.find("degraded") != std::string::npos);
    check("action result lists both devices", action.find("npu") != std::string::npos &&
                                                  action.find("cpu") != std::string::npos);
    check("action result keeps failure reason", action.find("写入失败") != std::string::npos);
    check("action result json parses",
          common::Json::parse(json.formatActionResult(info), doc) && doc["taskId"].asInt() == 5);
}

// ---------------------------------------------------------------- SystemdCtl

void testSystemdCtl() {
    std::printf("\n-- cli::SystemdCtl --\n");

    // x86 仿真环境（MinGW / WSL）没有 systemd，这里只验证"能跑到 shell 并拿回退出码"，
    // 不假设具体码值；上板后由 B8 的端到端测试覆盖真实 systemctl 行为。
    cli::SystemdCtl ctl("schedd");
    const int rc = ctl.start();
    std::printf("[INFO] SystemdCtl::start() rc=%d（仿真环境无 systemd，非 0 属预期）\n", rc);
    // -1 是本类对"连子进程都没起来"的返回值；能拿到退出码即说明 popen/pclose 链路通。
    check("start() launches a child process", rc != -1);
    check("status() outputs something", !ctl.status().empty());

    // 单元名含 shell 元字符必须被拒，不能拼进命令行
    cli::SystemdCtl unsafe("schedd; rm -rf /");
    check("reject unsafe unit name", unsafe.start() == -1);
    check("unsafe unit yields empty status", unsafe.status().empty());
}

}  // namespace

int main() {
    std::printf("=== B1 批次验收（拓扑层 L0 的 10 个叶子节点）===\n");

    testJson();
    testConfigLoader();
    testRequestCodec();
    testAgentContract();
    testOutputFormatter();
    testSystemdCtl();

    std::printf("\n=== %s（失败 %d 项）===\n", g_failed == 0 ? "ALL PASS" : "FAILURES", g_failed);
    return g_failed == 0 ? 0 : 1;
}
