#pragma once

#include "INpu.h"

namespace sched {

// 真实 RK3588 NPU：读 /sys/kernel/debug/rknpu/load 与
// /sys/class/devfreq/fdab0000.npu/*（governor、cur_freq、available_frequencies）
class RealNpu final : public INpu {
public:
    RealNpu();
    ~RealNpu() override;

    bool GetLoad(NpuState& out) override;
    bool SubmitTask(const std::string& task) override;
    bool SetGovernor(const std::string& governor) override;
    bool SetFrequency(int freqKHz) override;
    std::string GetBasePath() const override;

    static constexpr const char* kDebugLoadPath = "/sys/kernel/debug/rknpu/load";
    static constexpr const char* kDevfreqPath   = "/sys/class/devfreq/fdab0000.npu";
};

}  // namespace sched
