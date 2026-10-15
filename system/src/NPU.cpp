// SPDX-License-Identifier: MIT
// NPU 实现（骨架 —— 待填充）
//
// TODO(实现层)：
//   - getInfo()：readFreqTable() → 读 devPath_/cur_freq → 组装 DeviceInfo{device="npu"}
//   - apply()：校验 action.freqHz ∈ freqTable_ → setFreq() → 回读更新 curFreq_
//   - readFreqTable()：读 devfreq 的 available_frequencies → 解析 → freqTable_
//   - setFreq()：经 fs_->write() 写 devfreq 的 userspace/set_freq

#include "system/include/NPU.h"

#include "common/include/FileService.h"

namespace sched {

NPU::NPU(const std::string& devPath) : devPath_(devPath) {}

NPU::~NPU() = default;

DeviceInfo NPU::getInfo() {
    // TODO(实现层)
    DeviceInfo info;
    info.device = "npu";
    return info;
}

bool NPU::apply(const DeviceAction& action) {
    // TODO(实现层)
    (void)action;
    return false;
}

bool NPU::readFreqTable() {
    // TODO(实现层)
    return false;
}

bool NPU::setFreq(int freqHz) {
    // TODO(实现层)
    (void)freqHz;
    return false;
}

} // namespace sched
