// SPDX-License-Identifier: MIT
// CPU 实现（骨架 —— 待填充）
//
// TODO(实现层)：
//   - getInfo()：readFreqTable() → 读 procStatPath_ 累计 jiffies → 与 prevTotal_/prevIdle_ 差分算利用率
//                → 读 devPath_/cur_freq → 组装 DeviceInfo{device="cpu", utilization=...}
//   - apply()：校验 action.freqHz ∈ freqTable_ → setFreq() → 回读更新 curFreq_
//   - readFreqTable()：读 scaling_available_frequencies → 解析 → freqTable_
//   - setFreq()：经 fs_->write() 写 scaling_setspeed（注意 cpufreq 节点名）
//   首次采样无差分基准，利用率记 0。

#include "system/include/CPU.h"

#include "common/include/FileService.h"

namespace sched {

CPU::CPU(const std::string& devPath, const std::string& procStatPath)
    : devPath_(devPath), procStatPath_(procStatPath) {}

CPU::~CPU() = default;

DeviceInfo CPU::getInfo() {
    // TODO(实现层)
    DeviceInfo info;
    info.device = "cpu";
    return info;
}

bool CPU::apply(const DeviceAction& action) {
    // TODO(实现层)
    (void)action;
    return false;
}

bool CPU::readFreqTable() {
    // TODO(实现层)
    return false;
}

bool CPU::setFreq(int freqHz) {
    // TODO(实现层)
    (void)freqHz;
    return false;
}

} // namespace sched
