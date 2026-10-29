// SPDX-License-Identifier: MIT
// schedManager 单元测试（详细设计 §3.6 / D-08 / D-09 / D-10 / D-34）
//
// 覆盖：默认模型名、停用/未就绪两条 2001 分支、setModel 的三类拒绝与「失败不改 model」、
//       getSystemInfo 组装、ActionInfo 三镜像字段不漂移。
//
// 未覆盖（依赖外部实现，见提交说明）：
//   - 推理成功 → 下发 → 返回执行结果（需 ONNXruntime + 真实 .onnx）；
//   - 连续 3 次推理失败的两级降级阶梯（同上，且需可控的失败模型）。

#include <string>

#include "agent/include/DecisionExecutor.h"
#include "agent/include/DecisionMaker.h"
#include "agent/include/SchedManager.h"
#include "agent/include/SystemMonitor.h"
#include "system/include/SystemInterface.h"
#include "test/test_util.h"

using namespace sched;

namespace {

// 一组装配好的空壳搭档：monitor 未接 System（回零值）、maker 未加载模型
struct Rig {
    systemMonitor monitor{nullptr};
    decisionMaker maker;
    decisionExecutor executor{&monitor};
    schedManager manager{&monitor, &maker, &executor};

    TaskInfo task() const {
        TaskInfo t;
        t.requestId = "req-1";
        t.task["phase"] = 0.5;
        return t;
    }
};

} // namespace

TEST_CASE(default_model_name_is_default) {
    Rig rig;
    CHECK_EQ(rig.manager.getModel(), std::string("default"));
}

TEST_CASE(constructor_takes_custom_model_name) {
    systemMonitor monitor{nullptr};
    decisionMaker maker;
    decisionExecutor executor{&monitor};
    schedManager manager{&monitor, &maker, &executor, "dqn"};
    CHECK_EQ(manager.getModel(), std::string("dqn"));
}

TEST_CASE(task_ask_without_ready_returns_model_not_ready) {
    Rig rig;
    const ActionInfo info = rig.manager.taskAsk(rig.task());

    CHECK(!info.ok);
    CHECK_EQ(info.reason, Reason::modelNotReady);
    CHECK_EQ(info.message, std::string("model not ready"));
    CHECK_EQ(info.action.device, std::string());   // 无动作
    CHECK_EQ(info.action.freqKHz, 0);
}

TEST_CASE(task_ask_after_disable_returns_model_not_loaded) {
    Rig rig;
    CHECK(rig.manager.setModel(""));               // 停用分流
    CHECK_EQ(rig.manager.getModel(), std::string());

    const ActionInfo info = rig.manager.taskAsk(rig.task());
    CHECK(!info.ok);
    CHECK_EQ(info.reason, Reason::modelNotReady);
    CHECK_EQ(info.message, std::string("model not loaded"));
}

TEST_CASE(set_model_rejects_missing_file_and_keeps_name) {
    Rig rig;
    CHECK(!rig.manager.setModel("no_such_dir/dqn.onnx"));
    CHECK_EQ(rig.manager.getModel(), std::string("default"));   // 失败原封不动
}

TEST_CASE(set_model_rejects_trailing_slash) {
    Rig rig;
    CHECK(!rig.manager.setModel("/usr/lib/schedd/models/"));
    CHECK_EQ(rig.manager.getModel(), std::string("default"));
}

TEST_CASE(set_model_disable_does_not_touch_maker) {
    Rig rig;
    CHECK(rig.manager.setModel(""));
    CHECK(!rig.maker.isReady());        // 停用只停调用路径，不去卸载会话
    CHECK_EQ(rig.manager.getSystemInfo().model, std::string());
}

TEST_CASE(get_system_info_without_monitor_returns_zero_snapshot) {
    schedManager manager{nullptr, nullptr, nullptr, "dqn"};
    const SystemStateSnapshot snap = manager.getSystemInfo();

    CHECK_EQ(snap.model, std::string("dqn"));
    CHECK_NEAR(snap.system.ucpu, 0.0, 1e-9);
    CHECK_NEAR(snap.system.unpu, 0.0, 1e-9);
    CHECK_EQ(snap.system.cpuFreqKHz, 0);
    CHECK_EQ(snap.system.npuFreqKHz, 0);
}

TEST_CASE(get_system_info_forwards_from_monitor) {
    systemInterface sys{nullptr};              // System 侧未装配 → 零值
    systemMonitor monitor{&sys};
    decisionMaker maker;
    decisionExecutor executor{&monitor};
    schedManager manager{&monitor, &maker, &executor};

    const SystemStateSnapshot snap = manager.getSystemInfo();
    CHECK_NEAR(snap.system.ucpu, 0.0, 1e-9);
    CHECK_EQ(snap.model, std::string("default"));
}

TEST_CASE(action_info_mirrors_result_fields) {
    Rig rig;
    const ActionInfo info = rig.manager.taskAsk(rig.task());

    // ActionInfo 的 ok / message / reason 是 result 的只读镜像，不得各写一份
    CHECK_EQ(info.ok, info.result.ok);
    CHECK_EQ(info.message, info.result.message);
    CHECK_EQ(info.reason, info.result.reason);
    CHECK(!info.result.adjusted);
}

int main() {
    RUN(default_model_name_is_default);
    RUN(constructor_takes_custom_model_name);
    RUN(task_ask_without_ready_returns_model_not_ready);
    RUN(task_ask_after_disable_returns_model_not_loaded);
    RUN(set_model_rejects_missing_file_and_keeps_name);
    RUN(set_model_rejects_trailing_slash);
    RUN(set_model_disable_does_not_touch_maker);
    RUN(get_system_info_without_monitor_returns_zero_snapshot);
    RUN(get_system_info_forwards_from_monitor);
    RUN(action_info_mirrors_result_fields);

    return test::summary();
}
