// SPDX-License-Identifier: MIT
// decisionMaker —— 推理器 · Agent 子系统 · private（详细设计 §3.8）
//
// 职责：按契约拼接「任务特征 + 系统特征」→ 归一化 → ONNX 推理 → 动作解码。
// 设计要点：
//   - **不持 ONNXruntime 指针**：ONNXruntime 是全静态类的进程级资源（§1 / D-05），
//     直接以类名调用其静态方法；
//   - modelPath 只存**完整路径**，不存模型名（D-10）——本类只需知道「模型在哪里」；
//   - 契约路径**不存属性**：它恒等于「当前模型路径换扩展名」，由 setModelPath 派生；
//   - ready = 会话与契约**都已加载**才为真；构造不加载任何东西（D-11）。

#ifndef SCHED_AGENT_DECISIONMAKER_H
#define SCHED_AGENT_DECISIONMAKER_H

#include <string>

#include "common/include/SchedTypes.h"

namespace sched {

class decisionMaker {
public:
    // 构造注入模型完整路径（仅作**默认路径**预置）；空串表示未加载
    explicit decisionMaker(const std::string& modelPath = "");
    ~decisionMaker();

    // 就绪与否直接读 ready（模型会话与契约均已加载）
    bool isReady() const;

    // 热切换模型：路径校验 → 文件级校验 → 先建新会话 → 派生并加载契约 →
    // 两者都成功才替换并使 ready 转真；任一失败则路径 / 契约 / 就绪均不变。
    bool setModelPath(const std::string& modelPath);

    // 一次决策：按契约 input_fields 顺序拼接任务与系统特征 → 归一化 → 推理 → 解码。
    // 返回 valid == false 的 Decision 表示推理失败（未选中任何处理器）。
    Decision makeDecision(const TaskInfo& taskInfo, const SystemInfo& sysInfo);

private:
    // 解析并校验契约（三道校验的第 3 道在此处比对新会话输入维度）
    bool loadContract(const std::string& contractPath, int modelInputDim);

    std::string modelPath_;   // 当前生效模型的完整路径；空串 = 未加载
    Contract contract_;       // 已解析契约（inputDim / inputFields / outputDim / actions）
    bool ready_ = false;      // 就绪标志：会话与契约均已加载
};

} // namespace sched

#endif // SCHED_AGENT_DECISIONMAKER_H
