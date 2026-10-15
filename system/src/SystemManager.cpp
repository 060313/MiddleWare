// SPDX-License-Identifier: MIT
// SystemManager 实现（骨架 —— 待填充）
//
// TODO(实现层)：
//   - getInfo()：cpu_->getInfo() + npu_->getInfo() → 组装 SystemInfo{valid = cpu.valid && npu.valid}
//   - apply()：按 action.target 路由（"cpu" → cpu_->apply / "npu" → npu_->apply）
//   - 析构：释放组合对象 cpu_ / npu_

#include "system/include/SystemManager.h"

#include "system/include/CPU.h"
#include "system/include/NPU.h"

namespace sched {

SystemManager::SystemManager(CPU* cpu, NPU* npu) : cpu_(cpu), npu_(npu) {}

SystemManager::~SystemManager() {
    // TODO(实现层)：组合语义 —— 释放 cpu_ / npu_（若由本类 new 出来）
}

SystemInfo SystemManager::getInfo() {
    // TODO(实现层)
    return {};
}

bool SystemManager::apply(const DeviceAction& action) {
    // TODO(实现层)
    (void)action;
    return false;
}

} // namespace sched
