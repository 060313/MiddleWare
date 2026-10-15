// SPDX-License-Identifier: MIT
// NPU —— 设备边界类 · System 子系统 · private（详见详细设计 §7.12）
//
// 职责：NPU 设备边界。读/写 devfreq 频率节点。
// 设计要点：
//   - 与 CPU 同构，方法集合一致；
//   - 属性差异：NPU 无 procfs，故无 procStatPath / prevTotal / prevIdle / hasPrevSample；
//   - devPath 为 devfreq 挂点，构造注入，测试可注入伪 sysfs。

#ifndef SCHED_SYSTEM_NPU_H
#define SCHED_SYSTEM_NPU_H

#include <string>
#include <vector>

#include "common/include/SchedTypes.h"

namespace sched {

class FileService;

class NPU {
public:
    explicit NPU(const std::string& devPath);
    ~NPU();

    // 确保频率表已读 → 读当前频率 → 组装返回。
    DeviceInfo getInfo();

    // 校验目标频率 ∈ freqTable → setFreq → 回读更新 curFreq。
    bool apply(const DeviceAction& action);

private:
    bool readFreqTable();
    bool setFreq(int freqHz);

    std::string devPath_;        // 设备节点路径（devfreq 挂点）
    std::vector<int> freqTable_; // 可用频率表（Hz）
    int curFreq_ = 0;            // 当前频率（Hz）
    FileService* fs_ = nullptr;  // 依赖：文件服务（实现层注入）
};

} // namespace sched

#endif // SCHED_SYSTEM_NPU_H
