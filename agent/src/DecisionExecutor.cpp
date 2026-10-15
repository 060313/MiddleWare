// SPDX-License-Identifier: MIT
// DecisionExecutor 实现（骨架 —— 待填充）
//
// TODO(实现层)：
//   - execDecision()：遍历 decision.actionIds / targets
//                     → 逐设备施加（经实现层提供的设备访问通道）
//                     → 收集 ExecResult，单设备失败不短路
//   注：设备访问通道不入设计层，由实现层提供（例如 system 模块的 apply 出口）。

#include "agent/include/DecisionExecutor.h"

namespace sched {

std::vector<ExecResult> DecisionExecutor::execDecision(const Decision& decision) {
    // TODO(实现层)：结果与 decision.actionIds 严格对位、大小恒等
    (void)decision;
    return {};
}

} // namespace sched
