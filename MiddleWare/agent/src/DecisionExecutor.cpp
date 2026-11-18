// SPDX-License-Identifier: MIT
// decisionExecutor 实现（详细设计 §3.9 / D-12 / D-13）
//
// 一元语义：不遍历动作组，一次调用恒产生「一条动作 + 一个结果」。

#include "agent/include/DecisionExecutor.h"

#include "agent/include/SystemMonitor.h"

namespace sched {

decisionExecutor::decisionExecutor(systemMonitor* monitor) : monitor_(monitor) {
    // 构造只做赋值、不做 I/O
}

decisionExecutor::~decisionExecutor() = default;
// 平凡析构：不拥有 monitor_，不持设备句柄或会话

ActionInfo decisionExecutor::execDecision(const Decision& decision) {
    // S1. 取决策中**唯一**的动作
    // S1.1 取不到（decision 无效 / 未选中处理器）→ 不下发任何请求
    if (!decision.valid) {
        return makeFailure(Reason::inferenceFailed, "no action selected");
    }

    // S1.2 monitor 未装配 → 属调度链缺失，与「无动作」区分开
    if (monitor_ == nullptr) {
        return makeFailure(Reason::schedulerAbsent, "monitor not ready");
    }

    // S2 / S3. 经 Agent 侧唯一出口下发（systemManager 按设备标识二选一路由）
    const ResultInfo result = monitor_->setFreq(decision.action);

    // S4. 组装「本次动作 + 本次结果」返回；失败亦占位返回，不吞掉动作
    return makeActionInfo(decision.action, result);
}

} // namespace sched
