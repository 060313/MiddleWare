#include "SystemMonitor.h"

#include "SystemInterface.h"

namespace agent {

SystemMonitor::SystemMonitor(std::shared_ptr<syscore::SystemInterface> sys)
    : m_sys(std::move(sys)) {}

SystemMonitor::~SystemMonitor() = default;

bool SystemMonitor::getFeature(common::SystemFeature& out) {
    // TODO: 委派 m_sys->getInfo(out)
    (void)out;
    return false;
}

common::ResultInfo SystemMonitor::applyAction(const common::DeviceAction& action) {
    // TODO: 委派 m_sys->applyAction(action)
    (void)action;
    return common::ResultInfo{};
}

}  // namespace agent
