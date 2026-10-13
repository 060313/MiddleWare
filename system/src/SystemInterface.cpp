#include "SystemInterface.h"

#include "SystemManager.h"

namespace syscore {

SystemInterface::SystemInterface() : m_manager(std::make_unique<SystemManager>()) {}

SystemInterface::~SystemInterface() = default;

bool SystemInterface::getInfo(common::SystemFeature& out) {
    // TODO: 委派 m_manager->collect(out)
    (void)out;
    return false;
}

common::ResultInfo SystemInterface::applyAction(const common::DeviceAction& action) {
    // TODO: 委派 m_manager->applyAction(action)
    (void)action;
    return common::ResultInfo{};
}

common::StateSnapshot* SystemInterface::snapshot() {
    // TODO: 返回 m_manager->snapshot()
    return nullptr;
}

}  // namespace syscore
