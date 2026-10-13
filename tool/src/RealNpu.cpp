#include "RealNpu.h"

namespace sched {

RealNpu::RealNpu() = default;
RealNpu::~RealNpu() = default;

bool RealNpu::GetLoad(NpuState& out) {
    // TODO: 读 kDebugLoadPath（形如 "NPU load: Core0: 39%, ..."）解析三核负载；
    //       读 kDevfreqPath/cur_freq 与 governor 填充 out
    (void)out;
    return false;
}

bool RealNpu::SubmitTask(const std::string& task) {
    // TODO: RKLLM runtime 预留
    (void)task;
    return false;
}

bool RealNpu::SetGovernor(const std::string& governor) {
    // TODO: 写 kDevfreqPath/governor
    (void)governor;
    return false;
}

bool RealNpu::SetFrequency(int freqKHz) {
    // TODO: 校验 available_frequencies 后写 kDevfreqPath/userspace/set_freq
    (void)freqKHz;
    return false;
}

std::string RealNpu::GetBasePath() const {
    return kDevfreqPath;
}

}  // namespace sched
