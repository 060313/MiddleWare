// SPDX-License-Identifier: MIT
// CPU —— 设备边界类 · System 子系统 · private（详细设计 §3.12）
//
// 职责：CPU 设备边界。读 /proc/stat 差分算利用率、读 / 写 cpufreq 频率节点。
// 设计要点：
//   - **对外统一 kHz**（D-18）：cpufreq 节点原生即 kHz，读入直解析、写出直写，**不做折算**；
//   - 节点命名用 cpufreq 语义（scaling_*，D-17），`scaling_setspeed` 就在 policy 目录下；
//   - `setFreq` 的 ok **只取写入结果**，回读仅刷新 curFreq、**不比对**（D-32）；
//   - 构造只赋值、不读文件，故构造不可失败；freqTable 与档位收窄都在首次 getCPUInfo 时做。

#ifndef SCHED_SYSTEM_CPU_H
#define SCHED_SYSTEM_CPU_H

#include <cstdint>
#include <string>
#include <vector>

#include "common/include/SchedDefs.h"
#include "common/include/SchedTypes.h"

namespace sched {

class CPU {
public:
    // devPath：cpufreq policy 目录；procStatPath：/proc/stat；
    // freqMin / freqMax：档位收窄上下限（kHz，0 表不限），来源 §4.3.3
    CPU(const std::string& devPath,
        const std::string& procStatPath = PROC_STAT_PATH,
        int freqMin = 0,
        int freqMax = 0);
    ~CPU();

    // 确保频率表已读 → 读 /proc/stat 累计值 + 读当前频率 → 组装 CPUInfo 返回
    CPUInfo getCPUInfo();

    // 可承载性判定（越界夹取）→ 确保 governor 为 userspace → 写节点 → 回读刷新 curFreq
    ResultInfo setFreq(const Freq& freq);

private:
    // 读可用频率节点 → 解析 → 去重升序 → 按上下限收窄 → 写入 freqTable
    bool readFreqTable();

    // 经 FileService 写频率节点（纯物理写，不校验、不夹取）
    bool writeFreq(int freqKHz);

    std::string devPath_;         // 频率设备节点路径（cpufreq policy 目录）
    std::string procStatPath_;    // 状态文件路径（/proc/stat）
    int freqMin_ = 0;             // 档位收窄下限（kHz；0 表不限）
    int freqMax_ = 0;             // 档位收窄上限（kHz；0 表不限）
    std::vector<int> freqTable_;  // 可用频率表（kHz，已收窄）
    int curFreq_ = 0;             // 当前频率（kHz）
    uint64_t prevTotal_ = 0;      // 上次采样累计 jiffies（总）
    uint64_t prevIdle_ = 0;       // 上次采样累计 jiffies（空闲）
    bool hasPrevSample_ = false;  // 是否已有差分基准（首帧利用率为占位值）
};

} // namespace sched

#endif // SCHED_SYSTEM_CPU_H
