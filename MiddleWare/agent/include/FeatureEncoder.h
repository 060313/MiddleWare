// SPDX-License-Identifier: MIT
// FeatureEncoder —— 契约驱动的特征编码（详细设计 §4.4 D-28/D-30 的实现层，非设计类）
//
// 把 decisionMaker::makeDecision 的 S2「按契约拼接 + 归一化」抽成纯函数，可独立单测。
// 关键约束（D-30）：归一化分母只来自契约的 raw_min/raw_max，不硬编码任何量程字面量。

#ifndef SCHED_AGENT_FEATUREENCODER_H
#define SCHED_AGENT_FEATUREENCODER_H

#include <vector>

#include "common/include/SchedTypes.h"

namespace sched {

// 按契约 input_fields 的**数组顺序**（即模型输入位置）拼接任务/系统特征并归一化。
//   - source == "task"：按 name 从 TaskInfo.task 取值；缺失 → false（上层映射 2002）；
//   - source == "system"：按 name 取 SystemInfo 对应分量；未知分量名 → false；
//   - 归一化两次仿射后夹取 [0,1]，越界不判失败；rawMax == rawMin 取中值 0.5；
//   - 多余的任务键一律忽略（D-28）。
bool encodeFeatures(const Contract& contract, const TaskInfo& taskInfo,
                    const SystemInfo& sysInfo, std::vector<float>& out);

} // namespace sched

#endif // SCHED_AGENT_FEATUREENCODER_H
