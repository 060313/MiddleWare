// SPDX-License-Identifier: MIT
// DecisionMaker 实现（骨架 —— 待填充）
//
// TODO(实现层)：
//   - load()：modelPath_ 非空校验 → runtime_->load(modelPath_) → 置 ready_
//   - decide()：isReady 校验 → 特征编码 → runtime_->infer() → 动作解码 → Decision
//   - setModelPath()：装配期注入路径

#include "agent/include/DecisionMaker.h"

#include "common/include/ONNXruntime.h"

namespace sched {

DecisionMaker::DecisionMaker(ONNXruntime* runtime) : runtime_(runtime) {}

DecisionMaker::~DecisionMaker() = default;

bool DecisionMaker::isReady() const {
    return ready_;
}

bool DecisionMaker::load() {
    // TODO(实现层)
    return false;
}

Decision DecisionMaker::decide(const SystemFeature& feature) {
    // TODO(实现层)
    (void)feature;
    return {};
}

void DecisionMaker::setModelPath(const std::string& path) {
    modelPath_ = path;
}

} // namespace sched
