// SPDX-License-Identifier: MIT
// DecisionMaker —— 推理器 · Agent 子系统 · private（详见详细设计 §7.7）
//
// 职责：特征编码 → DQN 推理（经 ONNXruntime）→ 动作解码。
// 设计要点：
//   - modelPath 为配置注入属性，空串表示未就绪；
//   - load 无参（路径取自 modelPath）；isReady 判断模型是否加载成功。

#ifndef SCHED_AGENT_DECISIONMAKER_H
#define SCHED_AGENT_DECISIONMAKER_H

#include <string>

#include "common/include/SchedTypes.h"

namespace sched {

class ONNXruntime;

class DecisionMaker {
public:
    explicit DecisionMaker(ONNXruntime* runtime);
    ~DecisionMaker();

    // 判断模型是否已成功加载。
    bool isReady() const;

    // 加载模型建会话（路径取自 modelPath_）。
    bool load();

    // 执行一次推理。前置：isReady() == true。
    Decision decide(const SystemFeature& feature);

    // 配置注入模型路径（装配期调用）。
    void setModelPath(const std::string& path);

private:
    ONNXruntime* runtime_ = nullptr;         // 依赖：推理引擎封装
    std::string modelPath_;                  // ONNX 模型路径（配置注入）
    bool ready_ = false;                     // 就绪标志
};

} // namespace sched

#endif // SCHED_AGENT_DECISIONMAKER_H
