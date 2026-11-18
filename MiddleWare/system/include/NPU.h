// SPDX-License-Identifier: MIT
// NPU —— 设备边界类 · System 子系统 · private（详细设计 §3.13）
//
// 职责：NPU 设备边界。读 / 写 devfreq 频率节点、读 load 节点得利用率。
// 设计要点：
//   - 与 CPU **方法集合同构**（D-19），但无 procfs：不持 procStatPath / 差分基准；
//   - 利用率取 devfreq `load`（0~100 → [0,1]），**单帧即得**，无首帧占位问题；
//   - **devfreq 节点原生单位 Hz**（D-21）：读入后 ÷1000、写出前 ×1000，
//     freqTable / curFreq / freqMin / freqMax 的比较与存储一律在 kHz 上进行。

#ifndef SCHED_SYSTEM_NPU_H
#define SCHED_SYSTEM_NPU_H

#include <string>
#include <vector>

#include "common/include/SchedTypes.h"

namespace sched {

class NPU {
public:
    // devPath：devfreq 挂点；freqMin / freqMax：档位收窄上下限（kHz，0 表不限）
    NPU(const std::string& devPath, int freqMin = 0, int freqMax = 0);
    ~NPU();

    // 确保频率表已读 → 读当前频率 → 读 load 节点 → 组装 NPUInfo 返回
    NPUInfo getNPUInfo();

    // 可承载性判定（越界夹取）→ 确保 governor 为 userspace → 写节点 → 回读刷新 curFreq
    ResultInfo setFreq(const Freq& freq);

private:
    bool readFreqTable();
    bool writeFreq(int freqKHz);

    std::string devPath_;         // 设备节点路径（devfreq 挂点）
    int freqMin_ = 0;             // 档位收窄下限（kHz；0 表不限）
    int freqMax_ = 0;             // 档位收窄上限（kHz；0 表不限）
    std::vector<int> freqTable_;  // 可用频率表（kHz，已收窄）
    int curFreq_ = 0;             // 当前频率（kHz）
};

} // namespace sched

#endif // SCHED_SYSTEM_NPU_H
