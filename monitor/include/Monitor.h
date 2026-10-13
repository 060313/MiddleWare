#pragma once

// CPU / NPU 利用率与负载计算（既有代码资产，见《软件详细设计文档》5.4）。
// 通过依赖注入接收 INpu，便于 x86 侧注入 VirtualNpu 做单元测试。

#include <cstdint>

#include "INpu.h"

namespace monitor {

class Monitor {
public:
    explicit Monitor(sched::INpu* npu);
    ~Monitor() = default;

    // CPU 利用率：两次采样 /proc/stat 求差
    double get_ucpu();

    // NPU 利用率：取三核最大值（更能反映"忙"）
    double get_unpu();

    // NPU 完整状态
    sched::NpuState get_npu_state();

    uint32_t sample_interval_ms() const { return m_intervalMs; }
    void set_sample_interval_ms(uint32_t ms) { m_intervalMs = ms; }

private:
    sched::INpu* m_npu = nullptr;   // 不持有所有权
    uint32_t m_intervalMs = 500;
    uint64_t m_prevTotal = 0;       // 上一次 /proc/stat 累计值
    uint64_t m_prevIdle = 0;
};

}  // namespace monitor
