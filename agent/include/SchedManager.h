// SPDX-License-Identifier: MIT
// schedManager —— 总控类 · Agent 子系统 · public（详细设计 §3.6）
//
// 职责：调度请求的总控。取系统状态 → 推理 → 下发动作 → 回传结果，并管模型切换与降级。
// 设计要点：
//   - 三个关联（monitor / maker / executor）均构造注入，**只持指针、不拥有**；
//   - model 是「当前目标模型名」的**单一事实源**：初值取构造形参默认值 "default"，
//     运行期由 setModel 按路径末段覆盖；**空串表示推理已停用**；
//     `default` 只是默认模型（default.onnx）的模型名，**不是哨兵**（D-09 / D-10）；
//   - failCount **只统计连续推理失败**：推理成功清零、推理失败递增；执行失败不计（D-34）。

#ifndef SCHED_AGENT_SCHEDMANAGER_H
#define SCHED_AGENT_SCHEDMANAGER_H

#include <string>

#include "common/include/SchedTypes.h"

namespace sched {

class systemMonitor;
class decisionMaker;
class decisionExecutor;

class schedManager {
public:
    // 构造注入三个关联（生存期由装配层保证）与目标模型名（带默认值，装配层可省略）。
    // **只做赋值、不做 I/O，也不加载模型**——首次加载由装配层在构造后调 setModel（D-11）。
    schedManager(systemMonitor* monitor,
                 decisionMaker* maker,
                 decisionExecutor* executor,
                 const std::string& model = "default");
    ~schedManager();

    // 受理一次调度请求：取系统状态 → 停用/就绪判断 → 推理 → 计失败 → 执行动作 → 返回。
    // 算法步骤 S1~S7 见 §3.6。
    ActionInfo taskAsk(const TaskInfo& taskInfo);

    // 取对外状态载荷：SystemInfo 本体 + 当前模型名（字段见 SystemStateSnapshot）。
    SystemStateSnapshot getSystemInfo();

    // 直接读当前目标模型名（空串 = 推理已停用）。
    std::string getModel() const;

    // 热切换模型：**只收完整路径**（空串表示停用推理）；
    // 成功后按路径末段解析模型名写入 model，失败则 model 原封不动。
    bool setModel(const std::string& modelPath);

private:
    // 一次推理失败后的计数与降级判定（两级阶梯，D-08）
    void handleDecideFailure();

    systemMonitor* monitor_ = nullptr;      // 关联，构造注入
    decisionMaker* maker_ = nullptr;        // 关联，构造注入
    decisionExecutor* executor_ = nullptr;  // 关联，构造注入
    std::string model_ = "default";         // 当前目标模型名（单一事实源；空串 = 停用）
    int failCount_ = 0;                     // 连续推理失败计数（执行失败不计，D-34）
};

} // namespace sched

#endif // SCHED_AGENT_SCHEDMANAGER_H
