// SPDX-License-Identifier: MIT
// Service 子系统单元测试（详细设计 §3.1 / §4.6 / D-27 / D-33）
//
// 手法：UDSService 用链接期桩（test/stub/uds_stub），start 注入的 handler 被桩留存——
//       它是 private 的 handleFrame 的唯一入口，故协议层可脱离 socket 全量单测。
//       推理侧同样用 onnx_stub，设备侧用伪 sysfs，四种错误码族都能造出来。

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <string>

#include "agent/include/DecisionExecutor.h"
#include "agent/include/DecisionMaker.h"
#include "agent/include/SchedManager.h"
#include "agent/include/SystemMonitor.h"
#include "common/include/FileService.h"
#include "common/include/JsonUtil.h"
#include "service/include/ServiceInterface.h"
#include "system/include/SystemInterface.h"
#include "system/include/SystemManager.h"
#include "test/stub/onnx_stub.h"
#include "test/stub/uds_stub.h"
#include "test/test_util.h"

using namespace sched;

namespace fs = std::filesystem;

namespace {

const std::string kRoot = "build/test_fake_service";
const std::string kCpu = kRoot + "/cpu0/cpufreq";
const std::string kNpu = kRoot + "/npu";
const std::string kProc = kRoot + "/proc_stat";

void put(const std::string& path, const std::string& text) { FileService::write(path, text); }

void makeStubs() {
    fs::remove_all(kRoot);
    fs::create_directories(kCpu);
    fs::create_directories(kNpu + "/userspace");

    put(kCpu + "/scaling_available_frequencies", "1000000 1800000\n");
    put(kCpu + "/scaling_cur_freq", "1800000\n");
    put(kCpu + "/scaling_governor", "userspace\n");
    put(kCpu + "/scaling_setspeed", "0\n");
    put(kNpu + "/available_frequencies", "300000000 500000000\n");
    put(kNpu + "/cur_freq", "500000000\n");
    put(kNpu + "/governor", "userspace\n");
    put(kNpu + "/load", "30\n");
    put(kNpu + "/userspace/set_freq", "0\n");
    put(kProc, "cpu  1000 0 500 8000 100 0 0 0 0 0\n");
}

const char* kContract =
    R"({"input_dim":2,
        "input_fields":[
          {"name":"phase","source":"task","raw_min":0,"raw_max":1,"min":0.0,"max":1.0},
          {"name":"ucpu","source":"system","raw_min":0,"raw_max":1,"min":0.0,"max":1.0}],
        "output_dim":2,
        "actions":[{"device":"cpu","freq_khz":1000000},
                   {"device":"npu","freq_khz":500000}]})";

std::string makeModel() {
    const std::string base = kRoot + "/dqn";
    put(base + ".onnx", "onnx-bytes");
    put(base + ".json", kContract);
    return base + ".onnx";
}

// 经桩留存的 handler 发一帧，返回解析后的四键外壳
json::Value frame(const std::string& type, const std::string& payload) {
    const std::string text = stub::uds().handler(type, payload);
    json::Value out;
    if (!json::parse(text, out)) {
        return json::Value::null();
    }
    return out;
}

int code(const json::Value& shell) {
    const json::Value* item = shell.find("code");
    return (item == nullptr || !item->isNumber()) ? -1 : static_cast<int>(item->asNumber());
}

bool okFlag(const json::Value& shell) {
    const json::Value* item = shell.find("ok");
    return item != nullptr && item->isBool() && item->asBool();
}

bool dataIsNull(const json::Value& shell) {
    const json::Value* item = shell.find("data");
    return item != nullptr && item->isNull();
}

// 外壳自身：四键恒在、ok 与 code 不打架
bool wellFormed(const json::Value& shell) {
    return shell.isObject() && shell.find("ok") != nullptr && shell.find("code") != nullptr &&
           shell.find("message") != nullptr && shell.find("data") != nullptr &&
           (okFlag(shell) == (code(shell) == 0));
}

void testLifecycle() {
    test::section("1. 启停：running 门控与 UDS 参数透传");
    makeStubs();
    stub::uds().reset();

    serviceInterface svc(kRoot + "/schedd.sock", nullptr, 0);
    CHECK(!svc.running());
    CHECK(!svc.taskAsk(TaskInfo{}).ok);
    CHECK(svc.taskAsk(TaskInfo{}).reason == Reason::serviceNotRunning);
    CHECK(svc.getSystemInfo().model.empty());
    CHECK(!svc.setModel("x"));
    CHECK(svc.start());             // manager 为空也能 start（监听与业务链无关）

    CHECK(!svc.start());            // 已运行 → 再 start 失败
    CHECK(svc.running());

    CHECK(stub::uds().maxConn == 16);          // 非正值回退 16
    CHECK(stub::uds().socketPath == kRoot + "/schedd.sock");
    CHECK(stub::uds().hasHandler);

    svc.stop();
    CHECK(!svc.running());
    CHECK(stub::uds().stopCalls == 1);
}

void testUnknownType() {
    test::section("2. 未知报文类型 → 1002，且不断连");
    makeStubs();
    stub::uds().reset();
    systemManager mgr(kCpu, kNpu, kProc);
    systemInterface iface(&mgr);
    systemMonitor mon(&iface);
    decisionExecutor exec(&mon);
    decisionMaker maker("");
    schedManager sm(&mon, &maker, &exec, "dqn");

    serviceInterface svc(kRoot + "/s.sock", &sm, 4);
    CHECK(svc.start());
    CHECK(stub::uds().maxConn == 4);

    const json::Value shell = frame("NoSuchType", "");
    CHECK(wellFormed(shell));
    CHECK(code(shell) == 1002);
    CHECK(dataIsNull(shell));
}

void testTaskAskPayload() {
    test::section("3. TaskAsk 载荷校验：结构键与类型不符 → 1004");
    makeStubs();
    stub::uds().reset();
    stub::onnx().reset();
    stub::onnx().dim = 2;
    stub::onnx().output = {0.9f, 0.1f};

    const std::string model = makeModel();
    systemManager mgr(kCpu, kNpu, kProc);
    systemInterface iface(&mgr);
    systemMonitor mon(&iface);
    decisionExecutor exec(&mon);
    decisionMaker maker(model);
    CHECK(maker.setModelPath(model));
    mgr.refreshSystemInfo();

    schedManager sm(&mon, &maker, &exec, "dqn");
    serviceInterface svc(kRoot + "/s.sock", &sm, 4);
    CHECK(svc.start());

    CHECK(code(frame("TaskAsk", "{not json")) == 1004);
    CHECK(code(frame("TaskAsk", "")) == 1004);                                  // 空载荷缺 request_id
    CHECK(code(frame("TaskAsk", R"({"task":{}})")) == 1004);                    // 缺 request_id
    CHECK(code(frame("TaskAsk", R"({"request_id":""})")) == 1004);              // request_id 为空
    CHECK(code(frame("TaskAsk", R"({"request_id":"r1"})")) == 1004);            // 缺 task
    CHECK(code(frame("TaskAsk", R"({"request_id":"r1","task":[]})")) == 1004);  // task 非对象
    CHECK(code(frame("TaskAsk", R"({"request_id":"r1","task":{"phase":"0"}})")) == 1004);   // 值非数值

    // 合法：成功回 0，data 为动作结果
    const json::Value done = frame("TaskAsk", R"({"request_id":"r1","task":{"phase":1}})");
    CHECK(wellFormed(done) && code(done) == 0);
    const json::Value* data = done.find("data");
    CHECK(data != nullptr && data->isObject());
    CHECK(data->find("action")->find("device")->asString() == "cpu");
    CHECK(data->find("action")->find("freq_khz")->asNumber() == 1000000.0);
    CHECK(data->find("result")->find("ok")->asBool());
    CHECK(!data->find("result")->find("adjusted")->asBool());
}

void testGetState() {
    test::section("4. GetState：快照两字段、空载荷可接受");
    makeStubs();
    stub::uds().reset();
    systemManager mgr(kCpu, kNpu, kProc);
    systemInterface iface(&mgr);
    systemMonitor mon(&iface);
    decisionExecutor exec(&mon);
    decisionMaker maker("");
    schedManager sm(&mon, &maker, &exec, "dqn");
    mgr.refreshSystemInfo();

    serviceInterface svc(kRoot + "/s.sock", &sm, 4);
    CHECK(svc.start());

    const json::Value snap = frame("GetState", "");
    CHECK(wellFormed(snap) && code(snap) == 0);
    CHECK(snap.find("data")->find("model")->asString() == "dqn");
    CHECK(snap.find("data")->find("system")->find("cpu_freq_khz")->asNumber() == 1800000.0);
    CHECK(std::fabs(snap.find("data")->find("system")->find("unpu")->asNumber() - 0.30) < 1e-9);

    // 未知键忽略：带载荷也不报错
    CHECK(code(frame("GetState", R"({"whatever":1})")) == 0);
}

void testSetModel() {
    test::section("5. SetModel：只收 JSON 对象，空串即停用");
    makeStubs();
    stub::uds().reset();
    stub::onnx().reset();
    stub::onnx().dim = 2;

    const std::string model = makeModel();
    systemManager mgr(kCpu, kNpu, kProc);
    systemInterface iface(&mgr);
    systemMonitor mon(&iface);
    decisionExecutor exec(&mon);
    decisionMaker maker(model);
    schedManager sm(&mon, &maker, &exec, "old");

    serviceInterface svc(kRoot + "/s.sock", &sm, 4);
    CHECK(svc.start());

    CHECK(code(frame("SetModel", R"("/tmp/x.onnx")")) == 1004);   // 裸字符串不接受
    CHECK(code(frame("SetModel", "{}")) == 1004);                 // 缺 model_path
    CHECK(code(frame("SetModel", R"({"model_path":1})")) == 1004);  // 非字符串
    CHECK(code(frame("SetModel", R"({"model_path":"/tmp/"})")) == 2003);   // 末段为空

    const json::Value on = frame("SetModel", R"({"model_path":")" + model + R"("})");
    CHECK(wellFormed(on) && code(on) == 0);
    CHECK(on.find("data")->find("model")->asString() == "dqn");
    CHECK(sm.getModel() == "dqn");

    const json::Value off = frame("SetModel", R"({"model_path":""})");
    CHECK(code(off) == 0);
    CHECK(off.find("data")->find("model")->asString().empty());   // 停用后模型名为空串
    CHECK(sm.getModel().empty());
}

void testErrorMapping() {
    test::section("6. 错误码映射（D-33）：Reason → code 全覆盖");
    makeStubs();
    stub::uds().reset();
    stub::onnx().reset();
    stub::onnx().dim = 2;
    stub::onnx().output = {0.9f, 0.1f};

    const std::string model = makeModel();
    systemManager mgr(kCpu, kNpu, kProc);
    systemInterface iface(&mgr);
    systemMonitor mon(&iface);
    decisionExecutor exec(&mon);
    decisionMaker maker(model);
    CHECK(maker.setModelPath(model));
    mgr.refreshSystemInfo();

    schedManager sm(&mon, &maker, &exec, "dqn");
    serviceInterface svc(kRoot + "/s.sock", &sm, 4);
    CHECK(svc.start());
    const std::string ask = R"({"request_id":"r1","task":{"phase":1}})";

    // 推理失败（空输出）→ 2002
    stub::onnx().output.clear();
    const json::Value infer = frame("TaskAsk", ask);
    CHECK(code(infer) == 2002 && dataIsNull(infer));
    CHECK(infer.find("message")->asString() == "inference failed");

    svc.stop();   // 让出桩的 running，后面的链才能 start
    stub::onnx().output = {0.9f, 0.1f};

    // 档位表不可用 → 3002：设备路径不存在，表始终装不起来
    //   （已装配过的 freqTable 不会重读，故必须换链，不能只清空节点）
    systemManager mgr2(kRoot + "/none", kRoot + "/none", kProc);
    systemInterface iface2(&mgr2);
    systemMonitor mon2(&iface2);
    decisionExecutor exec2(&mon2);
    decisionMaker maker2(model);
    CHECK(maker2.setModelPath(model));
    schedManager sm2(&mon2, &maker2, &exec2, "dqn");
    serviceInterface svc2(kRoot + "/s2.sock", &sm2, 4);
    CHECK(svc2.start());
    const json::Value freq = frame("TaskAsk", ask);
    CHECK(code(freq) == 3002 && dataIsNull(freq));
    svc2.stop();

    // 写节点失败 → 3003：governor 是目录（FileService 写目录必然失败），表本身是好的
    const std::string kCpu2 = kRoot + "/cpu2/cpufreq";
    fs::create_directories(kCpu2 + "/scaling_governor");
    put(kCpu2 + "/scaling_available_frequencies", "1000000 1800000\n");
    put(kCpu2 + "/scaling_cur_freq", "1800000\n");
    put(kCpu2 + "/scaling_setspeed", "0\n");

    systemManager mgr3(kCpu2, kNpu, kProc);
    systemInterface iface3(&mgr3);
    systemMonitor mon3(&iface3);
    decisionExecutor exec3(&mon3);
    decisionMaker maker3(model);
    CHECK(maker3.setModelPath(model));
    mgr3.refreshSystemInfo();
    schedManager sm3(&mon3, &maker3, &exec3, "dqn");
    serviceInterface svc3(kRoot + "/s3.sock", &sm3, 4);
    CHECK(svc3.start());
    const json::Value write = frame("TaskAsk", ask);
    CHECK(code(write) == 3003 && dataIsNull(write));

    // 服务已停 → 1005（handler 由桩留存，stop 后仍能调用）
    svc3.stop();
    CHECK(code(frame("TaskAsk", ask)) == 1005);
    CHECK(code(frame("GetState", "")) == 1005);
    CHECK(code(frame("SetModel", R"({"model_path":"x"})")) == 1005);
}

void testNoScheduler() {
    test::section("7. 调度链未装配 → 1006");
    makeStubs();
    stub::uds().reset();

    serviceInterface svc(kRoot + "/s.sock", nullptr, 4);
    CHECK(svc.start());

    CHECK(code(frame("TaskAsk", R"({"request_id":"r1","task":{}})")) == 1006);
    CHECK(code(frame("GetState", "")) == 1006);
    CHECK(code(frame("SetModel", R"({"model_path":"x"})")) == 1006);
}

void testModelNotReady() {
    test::section("8. 推理未就绪 → 2001");
    makeStubs();
    stub::uds().reset();
    systemManager mgr(kCpu, kNpu, kProc);
    systemInterface iface(&mgr);
    systemMonitor mon(&iface);
    decisionExecutor exec(&mon);
    decisionMaker maker("");
    schedManager sm(&mon, &maker, &exec, "dqn");   // maker 未装载模型

    serviceInterface svc(kRoot + "/s.sock", &sm, 4);
    CHECK(svc.start());
    CHECK(code(frame("TaskAsk", R"({"request_id":"r1","task":{}})")) == 2001);
}

} // namespace

int main() {
    makeStubs();

    testLifecycle();
    testUnknownType();
    testTaskAskPayload();
    testGetState();
    testSetModel();
    testErrorMapping();
    testNoScheduler();
    testModelNotReady();

    const int rc = test::report("service");
    fs::remove_all(kRoot);
    return rc;
}
