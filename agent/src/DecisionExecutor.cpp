#include "DecisionExecutor.h"

#include "SystemMonitor.h"

namespace agent {

DecisionExecutor::DecisionExecutor(std::shared_ptr<SystemMonitor> monitor)
    : m_monitor(std::move(monitor)) {}

DecisionExecutor::~DecisionExecutor() = default;

common::ActionInfo DecisionExecutor::exec(const common::TaskFeature& task,
                                          const common::Decision& decision) {
    // TODO: 遍历 decision.actions 逐个 m_monitor->applyAction()；
    //       任一失败置 info.ok=false 并由上层触发降级
    (void)decision;
    common::ActionInfo info;
    info.taskId = task.taskId;
    info.actionIndex = decision.actionIndex;
    return info;
}

}  // namespace agent
