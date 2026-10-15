// SPDX-License-Identifier: MIT
// SystemMonitor 实现（骨架 —— 待填充）
//
// TODO(实现层)：
//   - getFeature()：collect() → 据 lastInfo_ 组装 SystemFeature；失败回兜底值（stale=true）
//   - collect()：sys_->getInfo() → 有效性判断 → 算特征（ucpu/unpu/freq）→ 写 lastInfo_

#include "agent/include/SystemMonitor.h"

#include "system/include/SystemInterface.h"

namespace sched {

SystemMonitor::SystemMonitor(systemInterface* sys) : sys_(sys) {}

SystemMonitor::~SystemMonitor() = default;

SystemFeature SystemMonitor::getFeature() {
    // TODO(实现层)
    collect();
    return {};
}

bool SystemMonitor::collect() {
    // TODO(实现层)
    return false;
}

} // namespace sched
