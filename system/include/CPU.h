// SPDX-License-Identifier: MIT
// CPU —— 设备边界类 · System 子系统 · private（详细设计 §3.12）
//
// 职责：读 /proc/stat 差分算利用率、读 / 写 cpufreq 频率节点。
// 设计依据：
//   - D-17：节点用 cpufreq 语义（scaling_*），均在 policy 目录下；
//   - D-18：节点**原生即 kHz** —— 读入直解析、写出直写，**不做折算**；
//   - D-32：setFreq 的 ok 只取写入结果，回读仅刷新 cpuInfo_.curFreqKHz，**不与目标值比对**；
//   - 构造只赋值、不读文件，故构造不可失败；freqTable 与档位收窄**懒加载**——
//     首次 getCPUInfo 或 setFreq 谁先到谁装表（服务线程可能先于监控线程到达调频请求）；
//   - 跨线程共享：监控线程 getCPUInfo / 服务线程 setFreq，共享状态由 cpuLock_ 保护，
//     **设备 I/O 一律在锁外**。

#ifndef SCHED_SYSTEM_CPU_H
#define SCHED_SYSTEM_CPU_H

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

#include "common/include/SchedDefs.h"
#include "common/include/SchedTypes.h"

namespace sched {

class CPU {
public:
    // freqPath_:CPU设备频率路径；
    // statPath:CPU设备状态路径；
    // freqMin：频率下限；
    // freqMax：频率上限。
    CPU(const std::string& freqPath=std::string(CPU_FREQ_PATH),
        const std::string& statPath=std::string(CPU_STAT_PATH),
        int freqMin = 0,
        int freqMax = 0);
    ~CPU();

    //返回CPU设备信息
    CPUInfo getCPUInfo();

    //设置处理器频率
    ResultInfo setFreq(const Freq& freq);

private:
    // 操作上锁：持 cpuLock_ 执行 fun 并转发其返回值（模板定义须在使用点可见，故放头文件）
    template<typename Function>
    auto withCPULock(Function&& fun) {
        const std::lock_guard<std::mutex> lock(cpuLock_);
        return fun();
    }
    // 读档位表
    bool readFreqTable();
    // 写频率节点
    bool writeFreq(int freqKHz);

    std::string freqPath_;          // 频率目录
    std::string statPath_;          // 状态目录
    int freqMin_ = 0;               // 档位收窄下限（kHz；0 表不限）
    int freqMax_ = 0;               // 档位收窄上限（kHz；0 表不限）

    // 跨线程共享状态，一律在 cpuLock_ 下读写
    CPUInfo cpuInfo_;
    std::vector<int> freqTable_;  // 可用频率表（kHz，已收窄）
    uint64_t prevTotal_ = 0;      // 上次采样累计 jiffies（总）
    uint64_t prevIdle_ = 0;       // 上次采样累计 jiffies（空闲）
    bool hasPrevSample_ = false;  // 已有差分基准？首帧利用率为占位值

    mutable std::mutex cpuLock_;
};

} // namespace sched

#endif // SCHED_SYSTEM_CPU_H
