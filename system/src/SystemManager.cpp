// SPDX-License-Identifier: MIT
// systemManager 实现（详细设计 §3.11）
//
// 锁规矩：**读设备在锁外，写缓存才持 sysLock_**（I/O 可能阻塞数百微秒，持锁会让服务线程排队）。
// 组合：cpu_ / npu_ 由 unique_ptr 持有；析构体必须留在 .cpp —— CPU / NPU 在头文件里只前置声明，
//       若隐式生成就会对不完整类型调 delete。

#include "system/include/SystemManager.h"

#include <memory>
#include <mutex>

#include "common/include/SchedDefs.h"
#include "system/include/CPU.h"
#include "system/include/NPU.h"

namespace sched {

systemManager::systemManager(const std::string& cpuFreqPath,
                             const std::string& npufreqPath,
                             const std::string& cpuStatPath,
                             const std::string& npuStatPath,
                             int cpuFreqMin,
                             int cpuFreqMax,
                             int npuFreqMin,
                             int npuFreqMax) {

    // 路径留空退化为宏默认（D-22）：配置文件键留空时不应把空串一路传到设备层
    const std::string cpuFreq = cpuFreqPath.empty() ? std::string(CPU_FREQ_PATH) : cpuFreqPath;
    const std::string cpuStat = cpuStatPath.empty() ? std::string(CPU_STAT_PATH) : cpuStatPath;
    const std::string npuFreq = npufreqPath.empty() ? std::string(NPU_FREQ_PATH) : npufreqPath;
    const std::string npuStat = npuStatPath.empty() ? std::string(NPU_STAT_PATH) : npuStatPath;

    // make_unique 而非裸 new：第二步抛异常时，已建好的那个由 unique_ptr 成员自行析构
    cpu_ = std::make_unique<CPU>(cpuFreq, cpuStat, cpuFreqMin, cpuFreqMax);
    npu_ = std::make_unique<NPU>(npuFreq, npuStat, npuFreqMin, npuFreqMax);

    systemInfo_ = SystemInfo{};   // 零值快照，等后台首次 refreshSystemInfo
}

systemManager::~systemManager() = default;

SystemInfo systemManager::askSystemInfo() {
    return withSysLock([&](){return systemInfo_;});   // 返回副本：锁释放后仍安全
}

bool systemManager::refreshSystemInfo() {
    if (cpu_ == nullptr || npu_ == nullptr) {
        return false;
    }

    // 读设备不持锁（各自读失败按零值兜底，不中断采集）
    const NPUInfo npuInfo = npu_->getNPUInfo();
    const CPUInfo cpuInfo = cpu_->getCPUInfo();

    withSysLock([&](){
        systemInfo_.ucpu = cpuInfo.util;
        systemInfo_.unpu = npuInfo.util;
        systemInfo_.cpuFreqKHz = cpuInfo.curFreqKHz;
        systemInfo_.npuFreqKHz = npuInfo.curFreqKHz;
    });
    return true;
}

ResultInfo systemManager::setFreq(const Freq& freq) {
    if (freq.device == "cpu") {
        return (cpu_ == nullptr)
                   ? ResultInfo{false, "unknown device", Reason::deviceUnknown}
                   : cpu_->setFreq(freq);
    }
    if (freq.device == "npu") {
        return (npu_ == nullptr)
                   ? ResultInfo{false, "unknown device", Reason::deviceUnknown}
                   : npu_->setFreq(freq);
    }
    return ResultInfo{false, "unknown device", Reason::deviceUnknown};
}

} // namespace sched
