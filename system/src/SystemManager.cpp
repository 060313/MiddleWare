#include "SystemManager.h"

#include "OsMonitor.h"

namespace syscore {

SystemManager::SystemManager()
    : m_os(std::make_unique<OsMonitor>()),
      m_monitor(nullptr),
      m_cpu(nullptr),
      m_npu(nullptr) {
    // TODO: 用 sched::INpu::Create() 建立 CPU / NPU 通道；
    //       构造 monitor::Monitor（注入 NPU）；
    //       读 devfreq available_frequencies 填充 m_availCpuFreqs / m_availNpuFreqs；
    //       挂接共享内存快照（/dev/shm/schedd_state）
}

SystemManager::~SystemManager() = default;

bool SystemManager::collect(common::SystemFeature& out) {
    // TODO: readCpuStat 求差 → m_monitor->get_ucpu()；
    //       m_npu->GetLoad() → 三核负载 / 频率；
    //       snapshotBeginWrite/Store/EndWrite 写共享内存；失败时 out.stale = true
    (void)out;
    return false;
}

common::ResultInfo SystemManager::applyAction(const common::DeviceAction& action) {
    // TODO: clampToNearest → SetFrequency → 回读 cur_freq 校验 → 组装 ResultInfo
    (void)action;
    return common::ResultInfo{};
}

int SystemManager::clampToNearest(const std::vector<int>& freqs, int want, bool& adjusted) const {
    // TODO: 在 freqs 中找与 want 距离最近的档位；若不等则 adjusted = true
    (void)freqs;
    (void)adjusted;
    return want;
}

}  // namespace syscore
