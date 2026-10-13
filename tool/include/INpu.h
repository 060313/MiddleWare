#pragma once

// NPU 能力抽象 + 工厂（设计模式：工厂方法 / 适配器）。
// 见《软件详细设计文档》5.4、8。
// 平台切换：环境变量 SCHED_NPU_MODE=real → RealNpu，否则 VirtualNpu。

#include <array>
#include <memory>
#include <string>

namespace sched {

enum class NpuGovernor { Performance, Powersave, Unknown };

struct NpuState {
    std::array<double, 3> load{0.0, 0.0, 0.0};   // 三核负载 [0,1]
    int freqKHz = 0;
    NpuGovernor governor = NpuGovernor::Unknown;
    int taskCount = 0;
    bool valid = false;                           // 读取失败时为 false
};

class INpu {
public:
    virtual ~INpu() = default;

    // 读取 NPU 当前状态（三核负载 / 频率 / governor / 任务数）
    virtual bool GetLoad(NpuState& out) = 0;

    // 提交任务（RKLLM runtime 预留接口）
    virtual bool SubmitTask(const std::string& task) = 0;

    virtual bool SetGovernor(const std::string& governor) = 0;
    virtual bool SetFrequency(int freqKHz) = 0;

    // 设备根路径，便于诊断与日志
    virtual std::string GetBasePath() const = 0;

    // 工厂：按 SCHED_NPU_MODE 返回真实 / 虚拟实现
    static std::unique_ptr<INpu> Create();
};

}  // namespace sched
