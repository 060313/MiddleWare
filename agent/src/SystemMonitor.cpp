// SPDX-License-Identifier: MIT
// systemMonitor 实现（详细设计 §3.7 / D-12）
//
// 本类只作通道：两个方法都是纯转发，不加工结果、不缓存、不采样。

#include "agent/include/SystemMonitor.h"

#include "system/include/SystemInterface.h"

namespace sched {

systemMonitor::systemMonitor(systemInterface* sys) : sys_(sys) {
    // 构造只做赋值、不做 I/O（§3 全类约定）
}

systemMonitor::~systemMonitor() = default;
// 平凡析构：不拥有 sys_，本类无自有资源

SystemInfo systemMonitor::getSystemInfo() {
    // S1. 未装配 → 回零值快照（该层永不失败）
    if (sys_ == nullptr) {
        return SystemInfo{};
    }

    // S2. 转发：读的是 System 侧缓存，本类不采样
    return sys_->getSystemInfo();
}

ResultInfo systemMonitor::setFreq(const Freq& freq) {
    // S1. 未装配 → 按「调度链未装配」报失败，不解引用空指针
    //     message 与 §3.7 S1 一致（"monitor not ready"）；程序判定只认 reason（D-33）
    if (sys_ == nullptr) {
        return ResultInfo{false, "monitor not ready", false, Reason::schedulerAbsent};
    }

    // S2. 原样透传（ok / message / adjusted / reason 一律不加工）
    return sys_->setFreq(freq);
}

} // namespace sched
