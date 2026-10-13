#include "SchedManager.h"

#include "DecisionExecutor.h"
#include "DecisionMaker.h"
#include "SystemMonitor.h"

namespace agent {

SchedManager::SchedManager()
    : m_monitor(nullptr), m_maker(nullptr), m_executor(nullptr) {
    // TODO: 装配 SystemMonitor（依赖 syscore::SystemInterface）、
    //       DecisionMaker、DecisionExecutor；从配置读取初始模式与推理超时
}

SchedManager::~SchedManager() = default;

common::ActionInfo SchedManager::handleTask(const common::TaskFeature& task) {
    // TODO: 读锁校验模式；auto → getFeature + infer + exec；
    //       static → 固定规则；off → 仅返回状态摘要
    common::ActionInfo info;
    info.taskId = task.taskId;
    return info;
}

bool SchedManager::setMode(common::SchedMode mode) {
    // TODO: 校验模式合法；off→auto/static 需 m_maker->ready()；加写锁更新 m_mode
    (void)mode;
    return false;
}

common::SchedMode SchedManager::mode() const {
    // TODO: 加读锁返回 m_mode
    return m_mode;
}

}  // namespace agent
