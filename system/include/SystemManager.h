// SPDX-License-Identifier: MIT
// SystemManager —— 管理器 · System 子系统 · private（详见详细设计 §7.10）
//
// 职责：汇聚 CPU / NPU 两路设备状态；按目标设备路由调频动作。
// 设计要点：
//   - cpu / npu 为组合关系（生存期由本类负责）；
//   - sampleIntervalMs 驱动后台采集（采样间隔，毫秒）。

#ifndef SCHED_SYSTEM_SYSTEMMANAGER_H
#define SCHED_SYSTEM_SYSTEMMANAGER_H

#include "common/include/SchedTypes.h"

namespace sched {

class CPU;
class NPU;

class SystemManager {
public:
    // 构造装配：本类负责 cpu / npu 的生存期（组合）
    SystemManager(CPU* cpu, NPU* npu);
    ~SystemManager();

    // 分别调 cpu->getInfo()、npu->getInfo() → 汇聚返回。
    SystemInfo getInfo();

    // 按目标设备类型路由 → cpu->apply() 或 npu->apply()。
    bool apply(const DeviceAction& action);

    int sampleIntervalMs() const { return sampleIntervalMs_; }
    void setSampleIntervalMs(int ms) { sampleIntervalMs_ = ms; }

private:
    CPU* cpu_ = nullptr;          // 组合（生存期由本类负责）
    NPU* npu_ = nullptr;          // 组合（生存期由本类负责）
    int sampleIntervalMs_ = 500;  // 采样间隔（毫秒）
};

} // namespace sched

#endif // SCHED_SYSTEM_SYSTEMMANAGER_H
