// SPDX-License-Identifier: MIT
// schedManager 实现（骨架 —— 待填充）
//
// TODO(实现层)：
//   - makeDecision()：monitor_->getFeature() → maker_->decide() → executor_->execDecision()
//                     → 组装 TaskResult；失败调 handleDecideFailure()
//   - getFeature()：monitor_->getFeature() + 当前 model_ 组装 SystemFeaturedState
//   - setModel()：白名单校验 → 非 static 时 maker_->isReady() 判断
//                 → 新建会话加载新模型 → 成功才替换 model_，失败丢弃
//   - handleDecideFailure()：++failCount_；达阈值 N 则兜底（降级 static）并重置计数

#include "agent/include/SchedManager.h"

#include "agent/include/DecisionExecutor.h"
#include "agent/include/DecisionMaker.h"
#include "agent/include/SystemMonitor.h"

namespace sched {

schedManager::schedManager(systemMonitor* monitor,
                           decisionMaker* maker,
                           decisionExecutor* executor)
    : monitor_(monitor), maker_(maker), executor_(executor) {}

schedManager::~schedManager() = default;

TaskResult schedManager::makeDecision(const TaskRequest& request) {
    // TODO(实现层)
    (void)request;
    return {};
}

SystemFeaturedState schedManager::getFeature() {
    // TODO(实现层)
    return {};
}

std::string schedManager::getModel() const {
    return model_;
}

bool schedManager::setModel(const std::string& modelName) {
    // TODO(实现层)
    (void)modelName;
    return false;
}

void schedManager::handleDecideFailure() {
    // TODO(实现层)
    ++failCount_;
}

} // namespace sched
