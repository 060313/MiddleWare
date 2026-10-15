// SPDX-License-Identifier: MIT
// schedManager —— 总控类 · Agent 子系统 · public（详见详细设计 §7.5）
//
// 职责：调度请求的总控。依次索取系统状态 → 执行 DQN 推理 → 下发动作 → 回传结果。
// 设计要点：
//   - 三个关联（monitor / maker / executor）均构造注入，持指针；
//   - model 为"当前所选模型名"的单一事实源，初值 "dqn"，static 仅作标识不触发分支；
//   - failCount 成功清零，达阈值 N 自动降级 static。

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
    // 构造注入三个关联对象（由装配层创建并保证生存期覆盖本对象）
    schedManager(systemMonitor* monitor,
                 decisionMaker* maker,
                 decisionExecutor* executor);
    ~schedManager();

    // 受理一次调度请求：取特征 → 模型判断 → 失败计数 → 执行动作 → 组装返回。
    TaskResult makeDecision(const TaskRequest& request);

    // 取系统特征（经 monitor）+ 附当前 model 返回。
    SystemFeaturedState getFeature();

    // 直接读当前模型名。
    std::string getModel() const;

    // 热切换模型：白名单校验 → 就绪性判断 → 新建会话加载 → 成功才原子替换。
    bool setModel(const std::string& modelName);

private:
    // 一次推理失败后的计数与兜底判定。
    void handleDecideFailure();

    systemMonitor* monitor_ = nullptr;      // 关联，构造注入
    decisionMaker* maker_ = nullptr;        // 关联，构造注入
    decisionExecutor* executor_ = nullptr;  // 关联，构造注入
    std::string model_ = "dqn";             // 当前所选模型名（单一事实源）
    int failCount_ = 0;                     // 连续推理失败计数
};

} // namespace sched

#endif // SCHED_AGENT_SCHEDMANAGER_H
