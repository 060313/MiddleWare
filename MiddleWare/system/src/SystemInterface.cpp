// SPDX-License-Identifier: MIT
// systemInterface 实现（详细设计 §3.10）
//
// 纯转发门面：不缓存、不加工，原样返回 systemManager 的结果。

#include "system/include/SystemInterface.h"

#include "system/include/SystemManager.h"

namespace sched {

systemInterface::systemInterface(systemManager* mgr) : mgr_(mgr) {
    // 构造只做赋值、不做 I/O（本类无其他属性需初始化）
}

systemInterface::~systemInterface() = default;
// 平凡析构：不拥有 mgr_；CPU / NPU 的所有者是 systemManager

SystemInfo systemInterface::getSystemInfo() {
    // S1. 未装配 → 回零值快照（该层永不失败）
    if (mgr_ == nullptr) {
        return SystemInfo{};
    }

    // S2. 纯读缓存
    return mgr_->askSystemInfo();
}

ResultInfo systemInterface::setFreq(const Freq& freq) {
    // S1. 未装配 → 调度链缺失
    if (mgr_ == nullptr) {
        return ResultInfo{false, "system not ready", false, Reason::schedulerAbsent};
    }

    // S2. 原样返回 systemManager 的 ResultInfo
    return mgr_->setFreq(freq);
}

} // namespace sched
