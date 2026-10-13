#include "Monitor.h"

namespace monitor {

Monitor::Monitor(sched::INpu* npu) : m_npu(npu) {}

double Monitor::get_ucpu() {
    // TODO: 读 /proc/stat 首行，两次采样（间隔 m_intervalMs）求
    //       (total_diff - idle_diff) / total_diff
    return 0.0;
}

double Monitor::get_unpu() {
    // TODO: 取 m_npu->GetLoad(state) 三核负载的最大值
    return 0.0;
}

sched::NpuState Monitor::get_npu_state() {
    // TODO: m_npu->GetLoad(state) 后原样返回
    return sched::NpuState{};
}

}  // namespace monitor
