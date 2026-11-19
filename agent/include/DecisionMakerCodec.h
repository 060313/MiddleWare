// SPDX-License-Identifier: MIT
// DecisionMakerCodec —— decisionMaker 的编码 / 解码内核（实现层内部，非设计类）
//
// 依据：详细设计 §3.8 S2（特征编码）与 S4（动作解码）、§4.4（契约字段）、D-24 / D-30。
//
// 为什么单独抽成纯函数：
//   - S2 / S4 是**纯计算**（契约 + 载荷 -> 向量 / 向量 -> 决策），不含会话与状态，
//     抽出来可脱离 ONNXruntime 单独单测（test/decision_maker_test.cpp 直接调）；
//   - makeDecision 只剩「就绪校验 -> 编码 -> 推理 -> 解码」四步编排，读起来与设计一致。
//
// 边界：
//   - 不访问文件、不访问推理会话、不改任何状态；全部 inline，不产生新目标文件；
//   - 本文件不是设计类，不属于 §1 的 13 个设计类，也不进 §3.14 的边表（与
//     SchedTypes.h 中的 makeActionInfo / makeFailure 同类：只放跨调用复用的纯函数）。

#ifndef SCHED_AGENT_DECISIONMAKERCODEC_H
#define SCHED_AGENT_DECISIONMAKERCODEC_H

#include <cmath>
#include <cstddef>
#include <string>
#include <vector>

#include "common/include/SchedTypes.h"

namespace sched {
namespace codec {

// ---------------------------------------------------------------------------
// S2.2 系统分量取值：契约 name -> SystemInfo 的四个特征分量
//
// 名字集合与 §4.4 的九维示例严格一致（ucpu / unpu / cpu_freq_khz / npu_freq_khz），
// 不另立别名——契约与代码各写一套名字是特征错位的头号来源。
// 取不到（名字不在集合内）返回 false，由调用方按「契约与运行时不配对」判失败。
// ---------------------------------------------------------------------------
inline bool lookupSystemField(const std::string& name, const SystemInfo& info, double& out) {
    if (name == "ucpu") {
        out = info.ucpu;
        return true;
    }
    if (name == "unpu") {
        out = info.unpu;
        return true;
    }
    if (name == "cpu_freq_khz") {
        out = static_cast<double>(info.cpuFreqKHz);
        return true;
    }
    if (name == "npu_freq_khz") {
        out = static_cast<double>(info.npuFreqKHz);
        return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// S2.4 归一化：两次仿射 + 夹取（标度全部来自契约，D-30）
//
//   ① v = (x - raw_min) / (raw_max - raw_min)   —— 退化分量（raw_max <= raw_min）取 0.5
//   ② v = min + v * (max - min)                —— 仿射到目标区间
//   ③ 夹取到 [0,1]                              —— 越界不判失败
// ---------------------------------------------------------------------------
inline double normalizeComponent(double x, const InputField& field) {
    const double span = field.rawMax - field.rawMin;

    // ① 原始量 -> [0,1]；跨度非正即退化分量，取中值，不除零（§3.8 S2.4.1）
    double v = (span > 0.0) ? (x - field.rawMin) / span : 0.5;

    // ② 仿射到契约声明的目标区间（常规 min=0 / max=1 时不变）
    v = field.min + v * (field.max - field.min);

    // ③ 夹取（§3.8 S2.4.3；越界不判失败，与频率越界夹取同风格）
    if (!(v > 0.0)) {          // 同时挡住 NaN：NaN 的任何比较都为 false
        return 0.0;
    }
    if (v > 1.0) {
        return 1.0;
    }
    return v;
}

// ---------------------------------------------------------------------------
// S2 特征编码：按 contract.inputFields 的**数组顺序**拼出一维向量
//
// 返回 false 的情形（均映射为「推理失败 2002」，由 schedManager 计入连续失败）：
//   - 契约未成形（inputDim <= 0 或与 inputFields 项数不一致）；
//   - 契约要求的任务键在 payload 中缺失；
//   - 契约声明的系统分量名不在 lookupSystemField 的名字集合内；
//   - 取到的值非有限数（NaN / ±Inf）。
// **多余的键一律忽略**（与 GetState 忽略未知键同口径，§3.8 S2.3）。
// ---------------------------------------------------------------------------
inline bool encodeFeatures(const Contract& contract,
                           const TaskInfo& taskInfo,
                           const SystemInfo& sysInfo,
                           std::vector<float>& out) {
    out.clear();

    // S2.1 前置：数组顺序即模型输入位置，项数必须与 input_dim 一致（第 1 道校验的镜像）
    if (contract.inputDim <= 0 ||
        contract.inputFields.size() != static_cast<std::size_t>(contract.inputDim)) {
        return false;
    }
    out.reserve(static_cast<std::size_t>(contract.inputDim));

    for (const InputField& field : contract.inputFields) {
        double x = 0.0;

        // S2.2 按来源取值
        if (field.source == "task") {
            const auto it = taskInfo.task.find(field.name);
            if (it == taskInfo.task.end()) {
                return false;   // S2.3 键不全 → 无效决策（不再逐项兜底，避免训练/推理口径漂移）
            }
            x = it->second;
        } else if (field.source == "system") {
            if (!lookupSystemField(field.name, sysInfo, x)) {
                return false;
            }
        } else {
            return false;       // 未知 source 属契约非法
        }

        // 非有限数视同「值非数值」
        if (!std::isfinite(x)) {
            return false;
        }

        out.push_back(static_cast<float>(normalizeComponent(x, field)));
    }

    return true;
}

// ---------------------------------------------------------------------------
// S4 动作解码：argmax -> 按契约 actions 查出唯一的「处理器 + 目标频率」
//
// 返回 valid == false 的情形：
//   - 输出为空（推理失败 / 超时取消）；
//   - 输出全为非有限数；
//   - 输出长度超出 actions 条数（契约第 2 道校验本应挡住，此处兜底）。
// 平手时取**编号最小**者（严格大于才更新，行为确定、可复现）。
// ---------------------------------------------------------------------------
inline Decision decodeAction(const Contract& contract, const std::vector<float>& output) {
    Decision decision;   // 默认 valid == false

    // 契约第 2 道校验保证 output_dim == actions 条数；此处要求严格相等，
    // 长度不符即判「契约与模型不配对」，不做截断也不做补零（错配要往可见处走）
    if (output.empty() || contract.actions.empty() ||
        output.size() != contract.actions.size()) {
        return decision;
    }

    std::size_t best = 0;
    bool found = false;
    for (std::size_t i = 0; i < output.size(); ++i) {
        if (!std::isfinite(output[i])) {
            continue;
        }
        if (!found || output[i] > output[best]) {
            best = i;
            found = true;
        }
    }
    if (!found) {
        return decision;
    }

    decision.action.device = contract.actions[best].device;
    decision.action.freqKHz = contract.actions[best].freqKHz;
    decision.valid = true;
    return decision;
}

} // namespace codec
} // namespace sched

#endif // SCHED_AGENT_DECISIONMAKERCODEC_H
