// SPDX-License-Identifier: MIT
// CPU —— 设备边界类 · System 子系统 · private（详见详细设计 §7.11）
//
// 职责：CPU 设备边界。读 /proc/stat 差分算利用率、读/写 cpufreq 频率节点。
// 设计要点：
//   - devPath / procStatPath 构造注入，默认即生产路径，测试可注入伪 sysfs；
//   - freqTable 首次采集后记住；curFreq 为当前频率；
//   - prevTotal / prevIdle / hasPrevSample 用于 jiffies 差分（CPU 独有，NPU 无）。
// 注意：cpufreq 标准节点名为 scaling_*，历史上曾误用 devfreq 命名，实现时须核对。

#ifndef SCHED_SYSTEM_CPU_H
#define SCHED_SYSTEM_CPU_H

#include <cstdint>
#include <string>
#include <vector>

#include "common/include/SchedTypes.h"

namespace sched {

class FileService;

class CPU {
public:
    // devPath：频率设备节点路径；procStatPath：/proc/stat 路径
    CPU(const std::string& devPath, const std::string& procStatPath);
    ~CPU();

    // 确保频率表已读 → 读状态文件累计值 + 读当前频率 → 组装返回。
    DeviceInfo getInfo();

    // 校验目标频率 ∈ freqTable → setFreq → 回读更新 curFreq。
    bool apply(const DeviceAction& action);

private:
    // 读可用频率节点 → 解析列表 → 写入 freqTable。
    bool readFreqTable();

    // 经 FileService 写频率节点。
    bool setFreq(int freqHz);

    std::string devPath_;        // 频率设备节点路径
    std::string procStatPath_;   // 状态文件路径（/proc/stat）
    std::vector<int> freqTable_; // 可用频率表（Hz）
    int curFreq_ = 0;            // 当前频率（Hz）
    uint64_t prevTotal_ = 0;     // 上次采样累计 jiffies（总）
    uint64_t prevIdle_ = 0;      // 上次采样累计 jiffies（空闲）
    bool hasPrevSample_ = false; // 是否已有差分基准
    FileService* fs_ = nullptr;  // 依赖：文件服务（实现层注入）
};

} // namespace sched

#endif // SCHED_SYSTEM_CPU_H
