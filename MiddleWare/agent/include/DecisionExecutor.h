// SPDX-License-Identifier: MIT
// decisionExecutor —— 执行器 · Agent 子系统 · private（详细设计 §3.9）
//
// 职责：把决策中的**唯一**动作经 Agent 侧统一出口下发，并组装 ActionInfo 返回。
// 设计要点：
//   - 只有 monitor 一个关联属性（构造注入）——**不直连** systemInterface / CPU / NPU（D-12）；
//   - **不遍历动作组**：一次 execDecision 恒产生「一条动作 + 一个结果」（D-13）；
//   - 无设计层属性之外的状态，不持设备句柄或会话。

#ifndef SCHED_AGENT_DECISIONEXECUTOR_H
#define SCHED_AGENT_DECISIONEXECUTOR_H

#include "common/include/SchedTypes.h"

namespace sched {

class systemMonitor;

class decisionExecutor {
public:
    // 构造注入监控门面引用；**只做赋值、不做 I/O**，不拥有 monitor
    explicit decisionExecutor(systemMonitor* monitor);
    ~decisionExecutor();

    // 取决策中唯一的 Freq → 经 monitor->setFreq 下发 → 组装 ActionInfo 返回。
    // 无动作（decision 无效 / monitor 未装配）时返回 ok=false 的空 ActionInfo，不下发请求。
    ActionInfo execDecision(const Decision& decision);

private:
    systemMonitor* monitor_ = nullptr;   // 关联，构造注入
};

} // namespace sched

#endif // SCHED_AGENT_DECISIONEXECUTOR_H
