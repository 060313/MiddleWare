#include "SystemManager.h"

#include <string>

#include "OsMonitor.h"

namespace syscore {

SystemManager::SystemManager()
    : m_npu(nullptr),
      m_cpuFreqPath("/sys/devices/system/cpu/cpufreq/policy0"),
      m_os(std::make_unique<OsMonitor>()) {
    // CPU 调频节点：RK3588 上 CPU 归 cpufreq（policy0 = 小核簇），NPU 归 devfreq。
    // 若板卡 BSP 实际把 CPU 也暴露在 devfreq 下（部分版本走 dmc），改这里的常量；
    // 将来可由 schedd.conf 的 [system] 段覆盖。

    // 挂接状态快照 —— 这是"m_snap 到底由谁分配"的唯一答案。
    //   Linux（openEuler / aarch64）：shm_open + mmap → /dev/shm/schedd_state
    //   Windows（MinGW，x86 仿真）：无 POSIX shm，自动退化为进程内对象；
    //     schedd 本是单进程多线程，进程内对象 + seqlock 已提供同等的线程间一致性。
    // 返回 false 只表示"没落在共享内存上"，不影响可用性，故此处不检查返回值。
    m_region.open("schedd_state");
    m_snap = m_region.get();

    // TODO: 1) m_npu = sched::INpu::Create();
    //       2) 读档位：
    //            m_availNpuFreqs = m_os->readAvailableFreqs(m_npu->GetBasePath())
    //            m_availCpuFreqs = m_os->readAvailableFreqs(m_cpuFreqPath)
    //          两者读失败时留空表，applyAction 里据此报 "device unavailable"
}

SystemManager::~SystemManager() = default;

bool SystemManager::collect(common::SystemFeature& out) {
    // TODO: 1) CPU 利用率（差分）
    //          unsigned long long total = 0, idle = 0;
    //          if (!m_os->readCpuStat(total, idle)) { out.stale = true; return false; }
    //          if (m_hasPrevSample)
    //              out.ucpu = static_cast<float>(
    //                  m_monitor.cpuUsage(m_prevTotal, m_prevIdle, total, idle));
    //          m_prevTotal = total; m_prevIdle = idle; m_hasPrevSample = true;
    //       2) NPU 负载与频率
    //          sched::NpuState state;
    //          if (m_npu->GetLoad(state)) {
    //              out.unpu = { float(state.load[0]), float(state.load[1]), float(state.load[2]) };
    //              out.npuFreqKHz = state.freqKHz;
    //          }
    //       3) 写快照
    //          成功：snapshotStore(*m_snap, out, 当前 mode 的 int 形式)
    //          失败：out.stale = true，并且【不】调用 snapshotStore
    //                —— 旧快照自然继续生效，与 stale 的语义一致，也无需给
    //                   StateSnapshot 另加字段（否则改动共享内存布局）
    (void)out;
    return false;
}

common::ResultInfo SystemManager::applyAction(const common::DeviceAction& action) {
    // TODO: 1) 按 action.device 选档位表并夹取
    //            "cpu" → m_availCpuFreqs；"npu" → m_availNpuFreqs
    //            freq = clampToNearest(freqs, action.freqKHz, adjusted)
    //       2) 写入（两条路径不对称，这是"CPU 退出 INpu"的直接体现）
    //            "cpu" → 先确保 governor == "userspace"（否则写 set_freq 不生效），
    //                    再 m_fs.write(m_cpuFreqPath + "/userspace/set_freq",
    //                                  std::to_string(freq))
    //            "npu" → m_npu->SetFrequency(freq)
    //       3) 回读校验：m_os->readCurrentFreq(对应路径) 与 freq 比对，
    //                    结果写入 ResultInfo.ok / appliedFreqKHz / message
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
