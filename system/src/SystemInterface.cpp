// SPDX-License-Identifier: MIT
// SystemInterface 实现（骨架 —— 待填充）
//
// TODO(实现层)：getInfo / apply 均为对 mgr_ 的转发

#include "system/include/SystemInterface.h"

#include "system/include/SystemManager.h"

namespace sched {

SystemInterface::SystemInterface(SystemManager* mgr) : mgr_(mgr) {}

SystemInterface::~SystemInterface() = default;

SystemInfo SystemInterface::getInfo() {
    // TODO(实现层)
    return {};
}

bool SystemInterface::apply(const DeviceAction& action) {
    // TODO(实现层)
    (void)action;
    return false;
}

} // namespace sched
