// SPDX-License-Identifier: MIT
// Agent 子系统单元测试（详细设计 §3.6~§3.9 / D-08 / D-12 / D-13 / D-30）
//
// 手法：两棵桩——
//   ① 设备侧沿用伪 sysfs（build/test_fake_agent/ 下的假节点 + 假 /proc/stat）；
//   ② 推理侧用 test/stub/onnx_stub 替代 ONNX Runtime SDK，会话行为可编程。
// 故整条「采集 → 编码 → 推理 → 解码 → 下发 → 降级」链路在无 SDK、无设备的机器上都能跑。

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

#include "agent/include/DecisionExecutor.h"
#include "agent/include/DecisionMaker.h"
#include "agent/include/SchedManager.h"
#include "agent/include/SystemMonitor.h"
#include "common/include/FileService.h"
#include "system/include/SystemInterface.h"
#include "system/include/SystemManager.h"
#include "test/stub/onnx_stub.h"
#include "test/test_util.h"

using namespace sched;

namespace fs = std::filesystem;

namespace {

const std::string kRoot = "build/test_fake_agent";
const std::string kCpu = kRoot + "/cpu0/cpufreq";
const std::string kNpu = kRoot + "/npu";
const std::string kProc = kRoot + "/proc_stat";

void put(const std::string& path, const std::string& text) { FileService::write(path, text); }

void makeStubs() {
    fs::remove_all(kRoot);
    fs::create_directories(kCpu);
    fs::create_directories(kNpu + "/userspace");

    put(kCpu + "/scaling_available_frequencies", "1000000 1800000 1400000\n");
    put(kCpu + "/scaling_cur_freq", "1800000\n");
    put(kCpu + "/scaling_governor", "userspace\n");
    put(kCpu + "/scaling_setspeed", "0\n");
    put(kNpu + "/available_frequencies", "300000000 500000000 1000000000\n");
    put(kNpu + "/cur_freq", "500000000\n");
    put(kNpu + "/governor", "userspace\n");
    put(kNpu + "/load", "30\n");
    put(kNpu + "/userspace/set_freq", "0\n");
    put(kProc, "cpu  1000 0 500 8000 100 0 0 0 0 0\n");
}

// 三维契约：两维来自 task（phase / prompt_len）、一维来自 system（ucpu）
const char* kContract3 =
    R"({"input_dim":3,
        "input_fields":[
          {"name":"phase","source":"task","raw_min":0,"raw_max":1,"min":0.0,"max":1.0},
          {"name":"prompt_len","source":"task","raw_min":0,"raw_max":8192,"min":0.0,"max":1.0},
          {"name":"ucpu","source":"system","raw_min":0,"raw_max":1,"min":0.0,"max":1.0}],
        "output_dim":2,
        "actions":[{"device":"cpu","freq_khz":1000000},
                   {"device":"npu","freq_khz":500000}]})";

// 造一对同名模型 / 契约（契约名由模型名派生，§4.3.2）
std::string makeModel(const char* name, const char* contract) {
    const std::string base = kRoot + "/" + name;
    put(base + ".onnx", "onnx-bytes");
    put(base + ".json", contract);
    return base + ".onnx";
}

// ---- systemMonitor ----

void testMonitor() {
    test::section("1. systemMonitor：未装配回零值，装配后原样透传");
    Freq freq{"cpu", 1400000};

    systemMonitor bare(nullptr);
    CHECK(bare.getSystemInfo().cpuFreqKHz == 0);
    const ResultInfo noSys = bare.setFreq(freq);
    CHECK(!noSys.ok && noSys.reason == Reason::schedulerAbsent);

    makeStubs();
    systemManager mgr(kCpu, kNpu, kProc);
    systemInterface iface(&mgr);
    systemMonitor mon(&iface);

    mgr.refreshSystemInfo();
    CHECK(mon.getSystemInfo().cpuFreqKHz == 1800000);   // 读的是缓存，不是设备
    CHECK(std::fabs(mon.getSystemInfo().unpu - 0.30) < 1e-9);

    const ResultInfo ok = mon.setFreq(freq);            // 表已由 refresh 装配
    CHECK(ok.ok);
}

// ---- decisionExecutor ----

void testExecutor() {
    test::section("2. decisionExecutor：一元语义与两种前置失败");
    decisionExecutor bare(nullptr);
    const ActionInfo none = bare.execDecision(Decision{});
    CHECK(!none.ok && none.reason == Reason::inferenceFailed);   // 未选处理器

    Decision decision;
    decision.valid = true;
    decision.action = Freq{"cpu", 1400000};
    const ActionInfo noMon = bare.execDecision(decision);
    CHECK(!noMon.ok && noMon.reason == Reason::schedulerAbsent);
    CHECK(noMon.action.device.empty());   // 前置失败走 makeFailure：**无动作**

    makeStubs();
    systemManager mgr(kCpu, kNpu, kProc);
    systemInterface iface(&mgr);
    systemMonitor mon(&iface);
    decisionExecutor exec(&mon);
    mgr.refreshSystemInfo();

    const ActionInfo done = exec.execDecision(decision);
    CHECK(done.ok && done.result.ok);
    CHECK(done.action.freqKHz == 1400000);   // 下发路径：失败亦占位返回，不吞掉动作
}

// ---- decisionMaker：装载 ----

void testMakerLoad() {
    test::section("3. decisionMaker 装载：三道校验 + 幂等");
    makeStubs();
    stub::onnx().reset();
    stub::onnx().dim = 3;

    const std::string model = makeModel("dqn", kContract3);
    decisionMaker maker(model);
    CHECK(!maker.isReady());                     // 构造不加载（D-11）

    CHECK(maker.setModelPath(model));
    CHECK(maker.isReady());
    CHECK(stub::onnx().loadCalls == 1);

    CHECK(maker.setModelPath(model));            // 同路径 + 已就绪 → 不重建会话
    CHECK(stub::onnx().loadCalls == 1);

    CHECK(!maker.setModelPath(""));              // 空路径是非法入参，不是「停用」
    CHECK(!maker.setModelPath(kRoot + "/nope.onnx"));

    // 第 3 道：模型维度与契约 input_dim 不符 → 失败，**生效会话仍是旧模型**
    stub::onnx().dim = 8;
    CHECK(!maker.setModelPath(makeModel("big", kContract3)));
    CHECK(maker.isReady() && stub::onnx().lastPath == model);   // 未被新模型顶掉

    // 契约自身不过（第 1、2 道）→ 同样不提交，旧模型继续服务
    stub::onnx().dim = 3;
    const char* badContract =
        R"({"input_dim":2,"input_fields":[{"name":"phase","source":"task","raw_min":0,"raw_max":1}],
            "output_dim":1,"actions":[{"device":"cpu","freq_khz":1000000}]})";
    CHECK(!maker.setModelPath(makeModel("bad", badContract)));
    CHECK(maker.isReady() && stub::onnx().lastPath == model);

    // 回归：两次切换失败之后，「模型 + 契约」仍是原来那一对，决策照常可用
    stub::onnx().output = {0.25f, 0.75f};
    TaskInfo task;
    task.task["phase"] = 1.0;
    task.task["prompt_len"] = 4096.0;
    SystemInfo sys;
    sys.ucpu = 0.25;
    const Decision still = maker.makeDecision(task, sys);
    CHECK(still.valid);
    CHECK(stub::onnx().lastInput.size() == 3);   // 仍按旧的三维契约编码
    CHECK(still.action.device == "npu" && still.action.freqKHz == 500000);
}

// ---- decisionMaker：编码与解码 ----

void testMakerEncode() {
    test::section("4. decisionMaker 编码：顺序、归一化、夹取、仿射");
    makeStubs();
    stub::onnx().reset();
    stub::onnx().dim = 3;
    stub::onnx().output = {0.3f, 0.7f};

    const std::string model = makeModel("dqn", kContract3);
    decisionMaker maker(model);
    CHECK(maker.setModelPath(model));

    TaskInfo task;
    task.requestId = "r-1";
    task.task["phase"] = 1.0;
    task.task["prompt_len"] = 4096.0;
    task.task["ignored"] = 999.0;          // 多余键一律忽略

    SystemInfo sys;
    sys.ucpu = 0.25;
    sys.unpu = 0.5;
    sys.cpuFreqKHz = 1800000;
    sys.npuFreqKHz = 500000;

    const Decision d = maker.makeDecision(task, sys);
    CHECK(d.valid);
    CHECK(stub::onnx().lastInput.size() == 3);
    CHECK(std::fabs(stub::onnx().lastInput[0] - 1.0) < 1e-6);    // phase 1 → 1.0
    CHECK(std::fabs(stub::onnx().lastInput[1] - 0.5) < 1e-6);    // 4096/8192 → 0.5
    CHECK(std::fabs(stub::onnx().lastInput[2] - 0.25) < 1e-6);   // system 分量

    CHECK(d.action.device == "npu" && d.action.freqKHz == 500000);   // argmax = 1

    // 越出量程 → 夹取到 1.0，不判失败
    task.task["prompt_len"] = 20000.0;
    CHECK(maker.makeDecision(task, sys).valid);
    CHECK(std::fabs(stub::onnx().lastInput[1] - 1.0) < 1e-6);

    // 低于下界 → 夹取到 0.0
    task.task["prompt_len"] = -100.0;
    CHECK(maker.makeDecision(task, sys).valid);
    CHECK(std::fabs(stub::onnx().lastInput[1] - 0.0) < 1e-6);
}

void testMakerEncodeEdge() {
    test::section("5. decisionMaker 编码：退化量程、仿射目标、缺键兜底");
    makeStubs();

    // 退化分量（raw_min == raw_max）取 0.5；min / max 只作仿射目标
    const char* c =
        R"({"input_dim":2,
            "input_fields":[
              {"name":"phase","source":"task","raw_min":1,"raw_max":1,"min":0.0,"max":1.0},
              {"name":"ucpu","source":"system","raw_min":0,"raw_max":1,"min":0.2,"max":0.8}],
            "output_dim":1,
            "actions":[{"device":"cpu","freq_khz":1000000}]})";
    stub::onnx().reset();
    stub::onnx().dim = 2;
    stub::onnx().output = {0.5f};
    const std::string model = makeModel("edge", c);
    decisionMaker maker(model);
    CHECK(maker.setModelPath(model));

    TaskInfo task;
    task.task["phase"] = 1.0;
    SystemInfo sys;
    sys.ucpu = 0.5;

    CHECK(maker.makeDecision(task, sys).valid);
    CHECK(std::fabs(stub::onnx().lastInput[0] - 0.5) < 1e-6);    // 退化 → 中值
    CHECK(std::fabs(stub::onnx().lastInput[1] - 0.5) < 1e-6);    // 0.2 + 0.5*0.6

    // 契约要求的 task 键缺失 → 无效 Decision（映射 2002）
    TaskInfo partial;
    CHECK(!maker.makeDecision(partial, sys).valid);

    // system 分量名不在四个特征之内 → 同样无效
    const char* bad =
        R"({"input_dim":1,
            "input_fields":[{"name":"gpu_freq","source":"system","raw_min":0,"raw_max":1}],
            "output_dim":1,"actions":[{"device":"cpu","freq_khz":1000000}]})";
    stub::onnx().reset();
    stub::onnx().dim = 1;
    stub::onnx().output = {0.5f};
    decisionMaker maker2(makeModel("badname", bad));
    CHECK(maker2.setModelPath(kRoot + "/badname.onnx"));   // 契约本身合法，能装载
    CHECK(!maker2.makeDecision(task, sys).valid);          // 取不到分量 → 无效

    // 推理空结果（含超时被取消）→ 无效
    stub::onnx().output.clear();
    CHECK(!maker.makeDecision(task, sys).valid);

    // 输出维大于动作数 → 兜底判无效
    stub::onnx().output = {0.1f, 0.2f, 0.9f};
    CHECK(!maker.makeDecision(task, sys).valid);
}

// ---- schedManager ----

void testManagerFlow() {
    test::section("6. schedManager：前置失败与正常下发");
    makeStubs();
    stub::onnx().reset();
    stub::onnx().dim = 3;
    stub::onnx().output = {0.9f, 0.1f};

    const std::string model = makeModel("dqn", kContract3);
    systemManager mgr(kCpu, kNpu, kProc);
    systemInterface iface(&mgr);
    systemMonitor mon(&iface);
    decisionExecutor exec(&mon);
    decisionMaker maker(model);
    CHECK(maker.setModelPath(model));

    mgr.refreshSystemInfo();
    TaskInfo task;
    task.task["phase"] = 0.0;
    task.task["prompt_len"] = 1024.0;

    schedManager sm(&mon, &maker, &exec, "dqn");
    const ActionInfo done = sm.taskAsk(task);
    CHECK(done.ok && done.action.device == "cpu");   // argmax = 0
    CHECK(done.action.freqKHz == 1000000);

    const SystemStateSnapshot snap = sm.getSystemInfo();
    CHECK(snap.model == "dqn" && snap.system.cpuFreqKHz == 1800000);

    // 停用：model 置空 → 2001，且**不计 failCount**
    CHECK(sm.setModel(""));
    CHECK(sm.getModel().empty());
    const ActionInfo off = sm.taskAsk(task);
    CHECK(!off.ok && off.reason == Reason::modelNotReady);

    CHECK(sm.setModel(model));          // 重新启用
    CHECK(sm.getModel() == "dqn");
    CHECK(!sm.setModel(kRoot + "/"));   // 末段为空 → 解析不出模型名
}

void testManagerDegrade() {
    test::section("7. schedManager：未就绪不计数，连续三次推理失败即降级");
    makeStubs();
    stub::onnx().reset();
    stub::onnx().dim = 3;
    stub::onnx().output = {0.5f, 0.2f};

    const std::string model = makeModel("dqn", kContract3);
    systemManager mgr(kCpu, kNpu, kProc);
    systemInterface iface(&mgr);
    systemMonitor mon(&iface);
    decisionExecutor exec(&mon);
    decisionMaker maker(model);

    TaskInfo task;
    task.task["phase"] = 0.0;
    task.task["prompt_len"] = 1024.0;

    // maker 未就绪 → 每次都回 2001，但**不触发降级**（没发生推理就不算推理失败）
    schedManager sm(&mon, &maker, &exec, "dqn");
    for (int i = 0; i < 5; ++i) {
        const ActionInfo info = sm.taskAsk(task);
        CHECK(!info.ok && info.reason == Reason::modelNotReady);
    }
    CHECK(sm.getModel() == "dqn");

    // 就绪但推理空结果 → 2002；连续三次触发降级
    CHECK(maker.setModelPath(model));
    stub::onnx().output.clear();
    for (int i = 0; i < 3; ++i) {
        const ActionInfo info = sm.taskAsk(task);
        CHECK(!info.ok && info.reason == Reason::inferenceFailed);
    }
    // 降级第①级切 default.onnx，本机该文件不存在 → 降至停用（model 置空）
    CHECK(sm.getModel().empty());

    // 已在停用态再失败 → 无处可降，仍是 2001
    CHECK(sm.taskAsk(task).reason == Reason::modelNotReady);
}

void testManagerNoExecutor() {
    test::section("8. schedManager：推理成功但执行链缺失 → 1006");
    makeStubs();
    stub::onnx().reset();
    stub::onnx().dim = 3;
    stub::onnx().output = {0.9f, 0.1f};

    const std::string model = makeModel("dqn", kContract3);
    systemManager mgr(kCpu, kNpu, kProc);
    systemInterface iface(&mgr);
    systemMonitor mon(&iface);
    decisionMaker maker(model);
    CHECK(maker.setModelPath(model));

    TaskInfo task;
    task.task["phase"] = 0.0;
    task.task["prompt_len"] = 1024.0;

    schedManager sm(&mon, &maker, nullptr, "dqn");
    const ActionInfo info = sm.taskAsk(task);
    CHECK(!info.ok && info.reason == Reason::schedulerAbsent);
    CHECK(sm.getModel() == "dqn");   // 执行失败不触发降级（D-34）
}

} // namespace

int main() {
    makeStubs();

    testMonitor();
    testExecutor();
    testMakerLoad();
    testMakerEncode();
    testMakerEncodeEdge();
    testManagerFlow();
    testManagerDegrade();
    testManagerNoExecutor();

    const int rc = test::report("agent");
    fs::remove_all(kRoot);
    return rc;
}
