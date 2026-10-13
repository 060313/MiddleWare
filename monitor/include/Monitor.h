#pragma once

// CPU / NPU 利用率计算（既有代码资产，见《软件详细设计文档》5.4）。
//
// 定位：纯计算 —— 不读文件、不 sleep、不持有采样状态。
//   · procfs / sysfs 的原始读取统一由 syscore::OsMonitor 负责；
//   · 两次采样的上一次累计值（prevTotal / prevIdle）由调用方 SystemManager 保存。
//
// 这样拆的理由：
//   1) Monitor 可无副作用地单测，不必造假的 /proc；
//   2) 旧版 get_ucpu() 内部自带 m_intervalMs(500ms) 阻塞二次采样，
//      与监控线程的 sample_interval_ms 形成双重节流，collect() 耗时会 ≥500ms。

#include <cstdint>

#include "INpu.h"

namespace monitor {

class Monitor {
public:
    Monitor() = default;
    ~Monitor() = default;

    // 两次 /proc/stat 累计值 → CPU 利用率 [0,1]。
    // 累计值倒退（CPU 热插拔）或差值为 0（首次采样）时返回 0.0，不抛异常。
    //   cur/prev 单位为 jiffies；idle 应已计入 iowait（见 OsMonitor::readCpuStat）
    double cpuUsage(unsigned long long prevTotal, unsigned long long prevIdle,
                    unsigned long long curTotal,  unsigned long long curIdle) const;

    // NPU 三核负载取最大值（更能反映"忙"）；state.valid 为 false 时返回 0.0
    double npuUsage(const sched::NpuState& state) const;

};  // class Monitor

}  // namespace monitor
