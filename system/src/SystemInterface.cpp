// SPDX-License-Identifier: MIT
// systemInterface 实现（详细设计 §3.10）
//
// 纯转发门面：不缓存、不加工，原样返回 systemManager 的结果；**不拥有 mgr_**。

#include "system/include/SystemInterface.h"

#include "system/include/SystemManager.h"

namespace sched {

systemInterface::systemInterface(systemManager* mgr) : mgr_(mgr) {}

systemInterface::~systemInterface() = default;

SystemInfo systemInterface::getSystemInfo() {
    return (mgr_ == nullptr) ? SystemInfo{} : mgr_->askSystemInfo();
}

ResultInfo systemInterface::setFreq(const Freq& freq) {
    if (mgr_ == nullptr) {
        return ResultInfo{false, "system not ready", false, Reason::schedulerAbsent};
    }
    return mgr_->setFreq(freq);
}

} // namespace sched
