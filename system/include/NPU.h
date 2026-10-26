// SPDX-License-Identifier: MIT
// NPU —— 设备边界类 · System 子系统 · private（详细设计 §3.13）
//
// 职责：读 / 写 devfreq 频率节点、读 load 节点得利用率。
// 设计依据：
//   - 与 CPU **方法集合同构**（D-19），但无 procfs：不持 procStatPath / 差分基准；
//   - 利用率取 devfreq `load`（0~100 → [0,1]），**单帧即得**，无首帧占位问题；
//   - D-21：devfreq 节点**原生单位 Hz** —— 读入 ÷1000、写出 ×1000，
//     表 / 当前值 / 收窄上下限的比较与存储一律在 kHz 上进行；
//   - 跨线程共享：同 CPU，共享状态由 npuLock_ 保护，**设备 I/O 一律在锁外**。

#ifndef SCHED_SYSTEM_NPU_H
#define SCHED_SYSTEM_NPU_H

#include <mutex>
#include <string>
#include <vector>

#include "common/include/SchedDefs.h"
#include "common/include/SchedTypes.h"

namespace sched {

class NPU {
public:
    // npuPath：devfreq 挂点；
    // freqMin / freqMax：收窄上下限（kHz，0 表不限）
    NPU(const std::string& npuPath=std::string(NPU_PATH), int freqMin = 0, int freqMax = 0);
    ~NPU();

    // 确保频率表已读 → 读当前频率 → 读 load → 组装 NPUInfo
    NPUInfo getNPUInfo();

    // 确保频率表已读（懒加载）→ 可承载性判定（越界夹取）→ 确保 governor 为 userspace
    // → 写节点 → 回读刷新 curFreq
    ResultInfo setFreq(const Freq& freq);

private:
    //操作锁
    template<typename Function>
    auto withNPULock(Function&& fun) {
        const std::lock_guard<std::mutex> lock(npuLock_);
        return fun();
    }

    // 读档位表：解析（Hz → ÷1000）→ 去重升序 → 收窄 → 持锁替换 freqTable_（唯一写入点）
    bool readFreqTable();

    // 经 FileService 写频率节点（写出前 ×1000 折成 Hz，D-21）
    bool writeFreq(int freqKHz);

    std::string npuPath_;         // devfreq 挂点
    int freqMin_ = 0;             // 档位收窄下限（kHz；0 表不限）
    int freqMax_ = 0;             // 档位收窄上限（kHz；0 表不限）

    // 跨线程共享状态，一律在 npuLock_ 下读写
    std::vector<int> freqTable_;  // 可用频率表（kHz，已收窄）
    int curFreq_ = 0;             // 当前频率（kHz）

    mutable std::mutex npuLock_;
};

} // namespace sched

#endif // SCHED_SYSTEM_NPU_H
