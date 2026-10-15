// SPDX-License-Identifier: MIT
// ONNXruntime 实现（骨架 —— 待填充）
//
// TODO(实现层)：
//   - 构造：初始化 Ort::Env（全局单例语义）
//   - load()：Ort::SessionOptions → Ort::Session(modelPath) → 存 session_
//   - infer()：input tensor 组装 → session_->Run → 取输出 → 转 vector<float>
//   - 头文件用 PIMPL（struct Session）隔离 SDK，避免污染上游编译单元

#include "common/include/ONNXruntime.h"

namespace sched {

struct ONNXruntime::Session {
    // TODO(实现层)：Ort::Session 实例 + 输入/输出名缓存
    int placeholder = 0;
};

ONNXruntime::ONNXruntime() = default;

ONNXruntime::~ONNXruntime() = default;

bool ONNXruntime::load(const std::string& modelPath) {
    // TODO(实现层)
    (void)modelPath;
    return false;
}

std::vector<float> ONNXruntime::infer(const std::vector<float>& features) {
    // TODO(实现层)
    (void)features;
    return {};
}

} // namespace sched
