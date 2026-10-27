// SPDX-License-Identifier: MIT
// ONNXruntime 桩实现（配套 test/stub/onnx_stub.h）

#include "test/stub/onnx_stub.h"

#include <memory>
#include <utility>

#include "common/include/ONNXruntime.h"

namespace sched {

namespace stub {

OnnxState& onnx() {
    static OnnxState state;
    return state;
}

} // namespace stub

// PIMPL 的桩形态：只记会话维度与来源路径，不触达任何 SDK
struct ONNXruntime::Env {
    bool placeholder = false;
};

struct ONNXruntime::Session {
    int dim = 0;
    std::string path;
};

std::unique_ptr<ONNXruntime::Env> ONNXruntime::env_ = nullptr;
std::unique_ptr<ONNXruntime::Session> ONNXruntime::session_ = nullptr;
std::unique_ptr<ONNXruntime::Session> ONNXruntime::pending_ = nullptr;

bool ONNXruntime::prepare(const std::string& modelPath) {
    ++stub::onnx().loadCalls;
    pending_.reset();
    if (modelPath.empty() || !stub::onnx().loadOk) {
        return false;
    }
    if (!env_) {
        env_ = std::make_unique<Env>();
    }
    auto session = std::make_unique<Session>();
    session->dim = stub::onnx().dim;
    session->path = modelPath;
    pending_ = std::move(session);
    return true;
}

int ONNXruntime::pendingInputDim() {
    return pending_ ? pending_->dim : 0;
}

bool ONNXruntime::commit() {
    if (!pending_) {
        return false;
    }
    // lastPath 只在**真正生效**时更新：测试据此断言「切换失败后旧模型仍在服务」
    stub::onnx().lastPath = pending_->path;
    session_ = std::move(pending_);
    return true;
}

void ONNXruntime::discard() {
    pending_.reset();
}

bool ONNXruntime::load(const std::string& modelPath) {
    return prepare(modelPath) && commit();
}

std::vector<float> ONNXruntime::infer(const std::vector<float>& features, int timeoutMs) {
    (void)timeoutMs;
    ++stub::onnx().inferCalls;
    stub::onnx().lastInput = features;
    if (!session_) {
        return std::vector<float>();
    }
    return stub::onnx().output;
}

int ONNXruntime::inputDim() {
    return session_ ? session_->dim : 0;
}

} // namespace sched
