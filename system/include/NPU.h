// SPDX-License-Identifier: MIT
// NPU —— 设备边界类 · System 子系统 · private（详细设计 §3.13）
//
// 职责：读 / 写 devfreq 频率节点、读 load 节点得利用率。
// 设计依据：
//   - 与 CPU **方法集合同构**（D-19），但无 procfs：**不持差分基准**；
//     statPath 仍持有，仅用于读 `load` 节点（异形设备可与频率挂点不同目录）；
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
#include "system/include/NPUFile.h"

namespace sched {

class NPU : public NPUFile{
public:
    // npuPath：devfreq 挂点；
    // freqMin / freqMax：收窄上下限（kHz，0 表不限）
    NPU(const std::string& freqPath=std::string(NPU_FREQ_PATH),const std::string& statPath=std::string(NPU_STAT_PATH),int freqMin = 0, int freqMax = 0)
            :NPUFile(freqPath,statPath,freqMin,freqMax){};
    ~NPU() override;

    // 出参写本次采样（内部 refreshInfo 采样后持 npuLock_ 拷出）；**不返回引用**，锁外触碰不到内部状态
    ResultInfo getNPUInfo(NPUInfo& out);
    
    ResultInfo refreshInfo() override;
    ResultInfo setFreq(const Freq& freq) override;

private:
    //操作锁
    template<typename Function>
    auto withNPULock(Function&& fun) {
        const std::lock_guard<std::mutex> lock(npuLock_);
        return fun();
    }
    // 跨线程共享状态，一律在 npuLock_ 下读写
    NPUInfo npuInfo_;
    mutable std::mutex npuLock_;
};

} // namespace sched

#endif // SCHED_SYSTEM_NPU_H
