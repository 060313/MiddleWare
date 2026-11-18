// SPDX-License-Identifier: MIT
// FeatureEncoder 实现（详细设计 §4.4 D-28 / D-30）

#include "agent/include/FeatureEncoder.h"

namespace sched {

namespace {

// SystemInfo 的系统分量名是闭集（§4.4 九维示例的四个 system 分量）
double systemFeature(const SystemInfo& sysInfo, const std::string& name, bool& found) {
    if (name == "ucpu") {
        found = true;
        return sysInfo.ucpu;
    }
    if (name == "unpu") {
        found = true;
        return sysInfo.unpu;
    }
    if (name == "cpu_freq_khz") {
        found = true;
        return sysInfo.cpuFreqKHz;
    }
    if (name == "npu_freq_khz") {
        found = true;
        return sysInfo.npuFreqKHz;
    }
    found = false;
    return 0.0;
}

} // namespace

bool encodeFeatures(const Contract& contract, const TaskInfo& taskInfo,
                    const SystemInfo& sysInfo, std::vector<float>& out) {
    out.clear();
    out.reserve(contract.inputFields.size());

    for (const InputField& field : contract.inputFields) {
        double raw = 0.0;

        if (field.source == "task") {
            const auto it = taskInfo.task.find(field.name);
            if (it == taskInfo.task.end()) {
                return false;   // 契约要求而载荷未提供 → 无效 Decision（2002）
            }
            raw = it->second;
        } else if (field.source == "system") {
            bool found = false;
            raw = systemFeature(sysInfo, field.name, found);
            if (!found) {
                return false;
            }
        } else {
            return false;   // source 闭集已由契约校验兜底，此处再拦一层
        }

        // 归一化（D-30）：第一次仿射到 [0,1]，rawMin == rawMax 取中值 0.5（不除零）
        double value = 0.5;
        if (field.rawMax != field.rawMin) {
            value = (raw - field.rawMin) / (field.rawMax - field.rawMin);
        }
        // 第二次仿射到目标区间 [min, max]
        value = field.min + value * (field.max - field.min);
        // 夹取到 [0,1]，越界不判失败
        if (value < 0.0) {
            value = 0.0;
        }
        if (value > 1.0) {
            value = 1.0;
        }
        out.push_back(static_cast<float>(value));
    }

    return true;
}

} // namespace sched
