#include "Monitor.h"

#include <algorithm>

namespace monitor {

double Monitor::cpuUsage(unsigned long long prevTotal, unsigned long long prevIdle,
                         unsigned long long curTotal,  unsigned long long curIdle) const {
    // 首次采样（两者相等）或计数器倒退（CPU 热插拔后 jiffies 归位）：
    // 差分无意义，返回 0 而不是算出负数 / 溢出。
    if (curTotal <= prevTotal) return 0.0;

    const unsigned long long dTotal = curTotal - prevTotal;
    const unsigned long long dIdle  = (curIdle >= prevIdle) ? (curIdle - prevIdle) : 0ULL;
    const unsigned long long dBusy  = (dTotal > dIdle) ? (dTotal - dIdle) : 0ULL;

    const double ratio = static_cast<double>(dBusy) / static_cast<double>(dTotal);
    return std::clamp(ratio, 0.0, 1.0);
}

double Monitor::npuUsage(const sched::NpuState& state) const {
    if (!state.valid) return 0.0;
    const double peak = *std::max_element(state.load.begin(), state.load.end());
    return std::clamp(peak, 0.0, 1.0);
}

}  // namespace monitor
