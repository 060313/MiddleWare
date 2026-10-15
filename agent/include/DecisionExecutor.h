// SPDX-License-Identifier: MIT
// DecisionExecutor —— 执行器 · Agent 子系统 · private（详见详细设计 §7.8）
//
// 职责：将决策动作组拆解为逐设备动作并施加（调频 / 批大小）。
// 设计要点：
//   - 无设计层属性（设备访问通道由实现层提供，不入设计层）；
//   - 结果与动作严格对位、大小恒等；单设备失败不短路。

#ifndef SCHED_AGENT_DECISIONEXECUTOR_H
#define SCHED_AGENT_DECISIONEXECUTOR_H

#include <vector>

#include "common/include/SchedTypes.h"

namespace sched {

class DecisionExecutor {
public:
    DecisionExecutor() = default;
    ~DecisionExecutor() = default;

    // 遍历动作 → 逐设备施加 → 收集结果，不因单个失败中断。
    std::vector<ExecResult> execDecision(const Decision& decision);
};

} // namespace sched

#endif // SCHED_AGENT_DECISIONEXECUTOR_H
