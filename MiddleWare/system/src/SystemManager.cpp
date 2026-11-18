// SPDX-License-Identifier: MIT
// systemManager 实现（详细设计 §3.11 / D-15）
//
// 锁的使用只有一条规矩：**读设备在锁外，写缓存才持写锁**。
// 设备 I/O 可能阻塞数百微秒，持锁会让服务线程的 askSystemInfo 排队。

#include "system/include/SystemManager.h"

#include <mutex>

#include "common/include/SchedDefs.h"
#include "system/include/CPU.h"
#include "system/include/NPU.h"

namespace sched {

systemManager::systemManager(const std::string& cpuFreqPath,
                             const std::string& npuFreqPath,
                             const std::string& procStatPath,
                             int cpuFreqMin,
                             int cpuFreqMax,
                             int npuFreqMin,
                             int npuFreqMax) {
    // S1. 路径为空则退化为宏定义默认（测试可注入伪 sysfs）
    const std::string cpuPath = cpuFreqPath.empty() ? std::string(CPU_FREQ_PATH) : cpuFreqPath;
    const std::string npuPath = npuFreqPath.empty() ? std::string(NPU_FREQ_PATH) : npuFreqPath;
    const std::string procPath = procStatPath.empty() ? std::string(PROC_STAT_PATH) : procStatPath;

    // S2. 创建并持有两路设备（**组合**），把路径与档位收窄上下限分别注入二者；
    //     本类**不保留**这四个收窄值——它们是设备的约束
    cpu_ = new CPU(cpuPath, procPath, cpuFreqMin, cpuFreqMax);
    npu_ = new NPU(npuPath, npuFreqMin, npuFreqMax);

    // S3. systemInfo 置零值快照（不采样，等后台首次 refreshSystemInfo）
    systemInfo_ = SystemInfo{};
}

systemManager::~systemManager() {
    // 本类是 CPU / NPU 的唯一所有者：删除并置空
    delete cpu_;
    cpu_ = nullptr;
    delete npu_;
    npu_ = nullptr;
}

SystemInfo systemManager::askSystemInfo() {
    // 持读锁取副本后即释放（纯读，不触发采样）
    const std::lock_guard<std::mutex> lock(sysMutex_);
    return systemInfo_;
}

bool systemManager::refreshSystemInfo() {
    // S1. 两路设备均须已创建
    if (cpu_ == nullptr || npu_ == nullptr) {
        return false;
    }

    // S2. **读设备不持锁**（各自读失败按零值兜底，不中断采集）
    const NPUInfo npuInfo = npu_->getNPUInfo();
    const CPUInfo cpuInfo = cpu_->getCPUInfo();

    // S3. 组装 SystemInfo：两份设备状态 + 四个特征分量
    SystemInfo info;
    info.cpuInfo = cpuInfo;
    info.npuInfo = npuInfo;
    info.ucpu = cpuInfo.util;              // CPU 利用率
    info.unpu = npuInfo.util;              // NPU 利用率（单值，口径同 NPUInfo）
    info.cpuFreqKHz = cpuInfo.curFreqKHz;  // 单位已统一 kHz，无需折算
    info.npuFreqKHz = npuInfo.curFreqKHz;

    // S4. 写缓存（setSystemInfo 内部持写锁）
    setSystemInfo(info);
    return true;
}

void systemManager::setSystemInfo(const SystemInfo& info) {
    const std::lock_guard<std::mutex> lock(sysMutex_);
    systemInfo_ = info;
}

ResultInfo systemManager::setFreq(const Freq& freq) {
    // S1. 按设备标识**二选一**路由（CPU / NPU 互斥，每次只走一条分支）
    if (freq.device == "cpu") {
        // S1.1 目标设备未创建 → 不解引用空指针
        if (cpu_ == nullptr) {
            return ResultInfo{false, "unknown device", false, Reason::deviceUnknown};
        }
        return cpu_->setFreq(freq);
    }
    if (freq.device == "npu") {
        if (npu_ == nullptr) {
            return ResultInfo{false, "unknown device", false, Reason::deviceUnknown};
        }
        return npu_->setFreq(freq);
    }

    // S2. 标识非法（含空串 = 未选中处理器）
    return ResultInfo{false, "unknown device", false, Reason::deviceUnknown};
}

} // namespace sched
