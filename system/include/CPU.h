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

#include <mutex>
#include <string>

#include "common/include/SchedTypes.h"
#include "system/include/CPUFile.h"

namespace sched {

class CPU : public openEulerCPUFile{
public:
    CPU(const std::string& freqPath,
        const std::string& statPath,
        int freqMin = 0,
        int freqMax = 0):openEulerCPUFile(freqPath,statPath,freqMin,freqMax){};
    ~CPU() override;

    CPUInfo getCPUInfo();

    // 覆写基类
    CPUInfo& getInfo() override;
    ResultInfo setFreq(const Freq& freq) override;

private:

    // 操作上锁：持 cpuLock_ 执行 fun 并转发其返回值（模板定义须在使用点可见，故放头文件）
    template<typename Function>
    auto withCPULock(Function&& fun) {
        const std::lock_guard<std::mutex> lock(cpuLock_);
        return fun();
    }

    // 跨线程共享状态，一律在 cpuLock_ 下读写
    CPUInfo cpuInfo_;
    mutable std::mutex cpuLock_;
};

} // namespace sched

#endif // SCHED_SYSTEM_CPU_H
