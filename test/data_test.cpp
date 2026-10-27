// SPDX-License-Identifier: MIT
// common + data 层单元测试：JsonUtil / ConfigFile / ModelFile / ContractFile
//
// 手法：全部走**临时文件桩**（build/test_fake_data/），不依赖真实 /etc/schedd 与
// /usr/lib/schedd/models，故本机与容器行为一致。

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <string>

#include "common/include/FileService.h"
#include "common/include/JsonUtil.h"
#include "data/include/ConfigFile.h"
#include "data/include/ContractFile.h"
#include "data/include/ModelFile.h"
#include "test/test_util.h"

using namespace sched;

namespace fs = std::filesystem;

namespace {

const std::string kRoot = "build/test_fake_data";

std::string put(const std::string& name, const std::string& text) {
    const std::string path = kRoot + "/" + name;
    FileService::write(path, text);
    return path;
}

void resetRoot() {
    fs::remove_all(kRoot);
    fs::create_directories(kRoot);
}

// ---- JsonUtil ----

void testJsonTypes() {
    test::section("1. JSON 解析：六类取值");
    json::Value root;
    const bool ok = json::parse(
        R"({"n":null,"t":true,"f":false,"i":42,"d":-1.5e2,"s":"ab","a":[1,2],"o":{"k":"v"}})", root);
    CHECK(ok && root.isObject());
    CHECK(root.find("n")->isNull());
    CHECK(root.find("t")->isBool() && root.find("t")->asBool());
    CHECK(root.find("f")->isBool() && !root.find("f")->asBool());
    CHECK(root.find("i")->isNumber() && root.find("i")->asNumber() == 42.0);
    CHECK(std::fabs(root.find("d")->asNumber() + 150.0) < 1e-9);
    CHECK(root.find("s")->asString() == "ab");
    CHECK(root.find("a")->isArray() && root.find("a")->asArray().size() == 2);
    CHECK(root.find("o")->find("k")->asString() == "v");
    CHECK(root.find("missing") == nullptr);
    CHECK(root.find("n")->find("k") == nullptr);   // 非对象上取键 → nullptr
}

void testJsonReject() {
    test::section("2. JSON 解析：非法文本一律 false");
    json::Value v;
    CHECK(!json::parse("", v));
    CHECK(!json::parse(R"({"a":1)", v));        // 截断
    CHECK(!json::parse(R"({"a":1}})", v));      // 尾随内容
    CHECK(!json::parse(R"({'a':1})", v));       // 单引号
    CHECK(!json::parse(R"({"a":01})", v));      // 前导零
    CHECK(!json::parse(R"({"a":+})", v));       // 指数缺数字
    CHECK(!json::parse(R"({"a":tru})", v));     // 字面量截断
    CHECK(!json::parse(R"({"a":1,})", v));      // 尾逗号
    CHECK(json::parse("{}", v) && v.isObject());
    CHECK(json::parse("[]", v) && v.isArray());
}

void testJsonStringify() {
    test::section("3. JSON 序列化：整数形态、转义往返、键序");
    json::Value obj = json::Value::object();
    obj.set("z", json::Value::number(1.0));
    obj.set("a", json::Value::number(0.25));
    obj.set("s", json::Value::string("q\"\\\n\t"));
    obj.set("b", json::Value::boolean(false));
    obj.set("n", json::Value::null());
    const std::string text = json::stringify(obj);
    CHECK(text == R"({"a":0.25,"b":false,"n":null,"s":"q\"\\\n\t","z":1})");

    json::Value back;
    CHECK(json::parse(text, back));
    CHECK(back.find("s")->asString() == "q\"\\\n\t");
    CHECK(back.find("n")->isNull());
}

// ---- ConfigFile ----

const char* kFullConf =
    "# comment\n"
    "; another\n"
    "[server]\r\n"
    "socket_path = /run/schedd.sock\r\n"
    "max_conn = abc        # 非法字面量，按默认处理\n"
    "empty =\n"
    "[agent]\n"
    "model_file = dqn.onnx\n"
    "[system]\n"
    "cpu_freq_min = 408000\n"
    "cpu_freq_max = 2256000\n"
    "npu_freq_min = 300000\n"
    "npu_freq_max = 1000000\n"
    "cpu_freq_path = /sys/cpu0\n"
    "npu_path = /sys/npu\n"
    "proc_stat_path = /proc/stat\n"
    "sample_interval_ms = 500\n"
    "no_eq_line\n";

void testConfigGet() {
    test::section("4. ConfigFile 取值：注释 / CRLF / 空值 / 非法字面量");
    resetRoot();
    const std::string path = put("schedd.conf", kFullConf);
    ConfigFile config;
    CHECK(config.getConfig(path));

    CHECK(config.getString("server", "socket_path", "x") == "/run/schedd.sock");
    CHECK(config.getInt("system", "cpu_freq_min", 0) == 408000);
    CHECK(config.getInt("server", "max_conn", 16) == 16);        // 非法字面量 → def
    CHECK(config.getInt("server", "missing", 7) == 7);           // 键缺失 → def
    CHECK(config.has("server", "empty"));                        // 值为空仍算存在
    CHECK(config.getString("server", "empty", "d") == "d");      // 但取值回 def
    CHECK(!config.has("server", "no_eq_line"));                  // 无 '=' 的行被跳过
    CHECK(!config.has("nope", "x"));
    CHECK(!config.has("server", ""));                            // 空键 → false
    CHECK(!config.getConfig(""));                                // 空路径 → false
}

void testConfigValidate() {
    test::section("5. ConfigFile 校验：只报节 / 键缺失");
    resetRoot();
    CHECK(ConfigFile().configValidate(put("full.conf", kFullConf)));

    const std::string noSection = "[server]\nsocket_path = /run/schedd.sock\n";
    CHECK(!ConfigFile().configValidate(put("nosec.conf", noSection)));

    std::string missingKey = kFullConf;
    const std::string::size_type at = missingKey.find("sample_interval_ms");
    missingKey.erase(at, missingKey.find('\n', at) - at + 1);
    CHECK(!ConfigFile().configValidate(put("miss.conf", missingKey)));

    // 取值非法**不判失败**（§4.3.4）：把整数键改成非法字面量仍应通过
    std::string badValue = kFullConf;
    const std::string::size_type p = badValue.find("cpu_freq_min = 408000");
    badValue.replace(p, std::string("cpu_freq_min = 408000").size(), "cpu_freq_min = xyz");
    CHECK(ConfigFile().configValidate(put("badval.conf", badValue)));
}

// ---- ModelFile ----

void testModelFile() {
    test::section("6. ModelFile：存在且非空才通过");
    resetRoot();
    CHECK(!ModelFile().modelValidate(""));
    CHECK(!ModelFile().modelValidate(kRoot + "/nope.onnx"));
    CHECK(!ModelFile().modelValidate(put("empty.onnx", "")));
    CHECK(ModelFile().modelValidate(put("dqn.onnx", "\x08\x00\x00\x00onnx")));
}

// ---- ContractFile ----

// 九维示例（§4.4），任务段在前、系统段在后
const char* kContract =
    R"({"input_dim":9,
        "input_fields":[
          {"name":"phase","source":"task","raw_min":0,"raw_max":1,"min":0.0,"max":1.0},
          {"name":"prompt_len","source":"task","raw_min":0,"raw_max":8192},
          {"name":"ucpu","source":"system","raw_min":0,"raw_max":1,"min":0.0,"max":1.0},
          {"name":"unpu","source":"system","raw_min":0,"raw_max":1,"min":0.0,"max":1.0},
          {"name":"cpu_freq_khz","source":"system","raw_min":408000,"raw_max":2256000,"min":0.0,"max":1.0},
          {"name":"npu_freq_khz","source":"system","raw_min":300000,"raw_max":1000000,"min":0.0,"max":1.0},
          {"name":"kv_ratio","source":"task","raw_min":0,"raw_max":1,"min":0.0,"max":1.0},
          {"name":"batch_tokens","source":"task","raw_min":0,"raw_max":2048,"min":0.0,"max":1.0},
          {"name":"n_remaining","source":"task","raw_min":0,"raw_max":8192,"min":0.0,"max":1.0}],
        "output_dim":3,
        "actions":[{"device":"cpu","freq_khz":408000},
                   {"device":"npu","freq_khz":300000},
                   {"device":"cpu","freq_khz":2256000}]})";

void testContractParse() {
    test::section("7. ContractFile 解析：九维契约");
    resetRoot();
    Contract contract;
    CHECK(ContractFile().loadContract(put("dqn.json", kContract), contract));
    CHECK(contract.inputDim == 9);
    CHECK(contract.inputFields.size() == 9);
    CHECK(contract.outputDim == 3);
    CHECK(contract.actions.size() == 3);

    // 数组顺序即模型输入位置，不得被解析重排
    CHECK(contract.inputFields[0].name == "phase");
    CHECK(contract.inputFields[0].source == "task");
    CHECK(contract.inputFields[2].name == "ucpu" && contract.inputFields[2].source == "system");

    // min / max 缺省取 0.0 / 1.0（prompt_len 那项未写）
    CHECK(contract.inputFields[1].min == 0.0 && contract.inputFields[1].max == 1.0);
    CHECK(contract.inputFields[4].rawMin == 408000 && contract.inputFields[4].rawMax == 2256000);

    CHECK(contract.actions[0].device == "cpu" && contract.actions[0].freqKHz == 408000);
    CHECK(contract.actions[1].device == "npu" && contract.actions[1].freqKHz == 300000);

    CHECK(ContractFile().contractValidate(kRoot + "/dqn.json"));
}

void testContractReject() {
    test::section("8. ContractFile 解析：缺字段 / 非法取值一律 false");
    resetRoot();
    Contract contract;
    contract.inputDim = 1;   // 预置，用于验证失败时出参不被污染
    ContractFile file;

    CHECK(!file.loadContract("", contract));
    CHECK(!file.loadContract(kRoot + "/nope.json", contract));

    CHECK(!file.loadContract(put("bad.json", "{not json"), contract));
    CHECK(!file.loadContract(put("arr.json", "[1,2,3]"), contract));            // 顶层非对象
    CHECK(!file.loadContract(put("nodim.json", R"({"output_dim":1,"input_fields":[],"actions":[]})"), contract));
    CHECK(!file.loadContract(put("nofield.json", R"({"input_dim":1,"output_dim":1,"actions":[]})"), contract));
    CHECK(!file.loadContract(
        put("badsrc.json",
            R"({"input_dim":1,"input_fields":[{"name":"a","source":"gpu","raw_min":0,"raw_max":1}],
                "output_dim":1,"actions":[]})"), contract));
    CHECK(!file.loadContract(
        put("noname.json",
            R"({"input_dim":1,"input_fields":[{"source":"task","raw_min":0,"raw_max":1}],
                "output_dim":1,"actions":[]})"), contract));
    CHECK(!file.loadContract(
        put("noraw.json",
            R"({"input_dim":1,"input_fields":[{"name":"a","source":"task"}],
                "output_dim":1,"actions":[]})"), contract));
    CHECK(!file.loadContract(
        put("baddrv.json",
            R"({"input_dim":1,"input_fields":[],"output_dim":1,
                "actions":[{"device":"gpu","freq_khz":1}]})"), contract));
    CHECK(!file.loadContract(
        put("nofreq.json",
            R"({"input_dim":1,"input_fields":[],"output_dim":1,
                "actions":[{"device":"cpu"}]})"), contract));

    CHECK(contract.inputDim == 1);   // 失败未写出参
    CHECK(!file.contractValidate(""));
}

void testContractValidate() {
    test::section("9. ContractFile 校验：第 1、2 道维度一致性");
    resetRoot();
    // input_dim = 2 但 input_fields 只 1 项
    const std::string dimMismatch =
        R"({"input_dim":2,"input_fields":[{"name":"a","source":"task","raw_min":0,"raw_max":1}],
            "output_dim":1,"actions":[{"device":"cpu","freq_khz":1}]})";
    CHECK(!ContractFile().contractValidate(put("m1.json", dimMismatch)));

    // output_dim = 2 但 actions 只 1 项
    const std::string outMismatch =
        R"({"input_dim":1,"input_fields":[{"name":"a","source":"task","raw_min":0,"raw_max":1}],
            "output_dim":2,"actions":[{"device":"cpu","freq_khz":1}]})";
    CHECK(!ContractFile().contractValidate(put("m2.json", outMismatch)));

    // 空表合法（维度全 0 也一致）
    const std::string empty = R"({"input_dim":0,"input_fields":[],"output_dim":0,"actions":[]})";
    CHECK(ContractFile().contractValidate(put("empty.json", empty)));
}

} // namespace

int main() {
    resetRoot();

    testJsonTypes();
    testJsonReject();
    testJsonStringify();
    testConfigGet();
    testConfigValidate();
    testModelFile();
    testContractParse();
    testContractReject();
    testContractValidate();

    const int rc = test::report("data");
    fs::remove_all(kRoot);
    return rc;
}
