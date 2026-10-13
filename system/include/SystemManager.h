#pragma once

// 参数↔状态换算、频率档位校验、状态快照维护（子系统内部，类可见性 internal）。
// 见《软件详细设计文档》6.3。

#include <memory>
#include <vector>

#include "INpu.h"
#include "Monitor.h"
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

    std::unique_ptr<OsMonitor>            m_os;
    std::unique_ptr<monitor::Monitor>     m_monitor;
    std::shared_ptr<sched::INpu>          m_cpu;   // CPU 调频通道（devfreq）
    std::shared_ptr<sched::INpu>          m_npu;   // NPU 通道
    sched::SystemFS                       m_fs;
    common::StateSnapshot*                m_snap = nullptr;
    std::vector<int>                      m_availCpuFreqs;
    std::vector<int>                      m_availNpuFreqs;
};

}  // namespace syscore
