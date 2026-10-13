#pragma once

#include <string>

#include "INpu.h"

namespace sched {

// 虚拟 NPU：在 <临时目录>/virtual_npu/ 下模拟 sysfs 文件，用于 x86 仿真调试。
// 目录结构：load、governor、cur_freq、available_frequencies、task_count
class VirtualNpu final : public INpu {
public:
    VirtualNpu();
    ~VirtualNpu() override;

    bool GetLoad(NpuState& out) override;
    bool SubmitTask(const std::string& task) override;
    bool SetGovernor(const std::string& governor) override;
    bool SetFrequency(int freqKHz) override;
    std::string GetBasePath() const override;

private:
    std::string m_base;   // <tempDir>/virtual_npu
};

}  // namespace sched
