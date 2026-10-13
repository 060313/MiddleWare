#pragma once

// 参数↔状态换算、频率档位校验、状态快照维护（子系统内部，类可见性 internal）。
// 见《软件详细设计文档》6.3。

#include <memory>
#include <string>
#include <vector>

#include "INpu.h"
#include "Monitor.h"
#include "SnapshotRegion.h"
#include "StateSnapshot.h"
#include "SystemFS.h"
#include "Types.h"

namespace syscore {

class OsMonitor;

class SystemManager {
public:
    SystemManager();
    ~SystemManager();

    SystemManager(const SystemManager&) = delete;
    SystemManager& operator=(const SystemManager&) = delete;

    // 采样 → 换算 → 写共享内存快照
    bool collect(common::SystemFeature& out);

    // 档位夹取 → 写入设备 → 回读校验
    common::ResultInfo applyAction(const common::DeviceAction& action);

    common::StateSnapshot* snapshot() { return m_snap; }

    const std::vector<int>& availableCpuFreqs() const { return m_availCpuFreqs; }
    const std::vector<int>& availableNpuFreqs() const { return m_availNpuFreqs; }

private:
    // 把 want 夹取到 freqs 中最近档位；越界时 adjusted 置 true
    int clampToNearest(const std::vector<int>& freqs, int want, bool& adjusted) const;

    // ---------- 设备通道 ----------
    // NPU：走 HAL 抽象（sched::INpu），真实 / VirtualNpu 由工厂按环境变量决定。
    //
    // CPU：【刻意不套用 INpu】。INpu 的 GetLoad（返回 NpuState 三核负载）、
    //      SubmitTask（推理引擎）、SetGovernor 对 CPU 是语义空壳，且 RealNpu 的
    //      devfreq 路径硬编码为 fdab0000.npu，Create() 拿不到 CPU 实现。
    //      CPU 侧只需要「读档位 + 写 set_freq」，直接走 OsMonitor（读）与
    //      SystemFS（写），不引入一个只有 2/5 方法有意义的接口。
    std::shared_ptr<sched::INpu> m_npu;
    std::string                  m_cpuFreqPath;      // CPU 调频节点目录

    // ---------- 采集 ----------
    std::unique_ptr<OsMonitor>   m_os;               // 唯一的 procfs / sysfs 读取层
    monitor::Monitor             m_monitor;          // 纯计算、无状态，直接内嵌

    unsigned long long m_prevTotal = 0;              // 上一次 /proc/stat 累计值
    unsigned long long m_prevIdle  = 0;
    bool               m_hasPrevSample = false;      // 首次采样无差分，利用率记 0

    // ---------- 快照 ----------
    common::SnapshotRegion       m_region;           // 快照内存的宿主（shm / 进程内）
    common::StateSnapshot*       m_snap = nullptr;   // 等价于 m_region.get()，避免重复取值

    // ---------- 档位表 ----------
    sched::SystemFS              m_fs;
    std::vector<int>             m_availCpuFreqs;
    std::vector<int>             m_availNpuFreqs;
};

}  // namespace syscore
