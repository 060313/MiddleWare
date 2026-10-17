// SPDX-License-Identifier: MIT
// ONNXruntime 实现（详细设计 §3.4 / D-05 / D-31）
//
// 静态成员定义集中在本文件顶部——全静态类没有构造，状态只能在此处落地。
//
// TODO(实现层)：三处待补
//   1. Env / Session 换成 Ort::Env / Ort::Session 的 RAII 包装（含输入输出名缓存）；
//      注意 **env_ 必须在 session_ 之前定义**（声明顺序决定析构顺序）；
//   2. load()：Ort::SessionOptions → Ort::Session(modelPath) → 覆盖 session_；
//      失败不得污染旧会话（新建成功才移动赋值）；
//   3. infer()：组装 input tensor → Run → 取输出 → vector<float>；
//      **超时机制**（D-31）：同步 Run() 不可从外部打断，故选
//      「局部 Ort::RunOptions + 一次性看护线程」——看护线程 sleep 到点后调
//      RunOptions::SetTerminate()，Run 返回前必须 join，不得留下持有 RunOptions
//      引用的游离线程。返回空列表表示失败 / 超时。
//   4. inputDim()：读输入形状的**末维**（首维是批维 -1），无效返回 0。

#include "common/include/ONNXruntime.h"

namespace sched {

struct ONNXruntime::Env {
    // TODO(实现层)：Ort::Env（ORT_LOGGING_LEVEL_WARNING, "schedd"）
    bool placeholder = false;
};

struct ONNXruntime::Session {
    // TODO(实现层)：Ort::Session + 输入名 / 输出名缓存
    bool placeholder = false;
};

// 定义顺序与声明顺序一致：env_ 先构造、后析构
std::unique_ptr<ONNXruntime::Env> ONNXruntime::env_ = nullptr;
std::unique_ptr<ONNXruntime::Session> ONNXruntime::session_ = nullptr;

bool ONNXruntime::load(const std::string& modelPath) {
    // S1. 前置：路径非空
    if (modelPath.empty()) {
        return false;
    }

    // S2. 环境惰性创建：进程内只建一次，此后复用
    if (!env_) {
        // TODO(实现层)：env_ = std::make_unique<Env>(...);
    }

    // S3. TODO(实现层)：建立新会话；成功才覆盖 session_（失败保持旧会话）
    return false;
}

std::vector<float> ONNXruntime::infer(const std::vector<float>& features, int timeoutMs) {
    // S1. 前置：会话有效、输入非空
    if (!session_ || features.empty()) {
        return std::vector<float>();
    }

    // S2. TODO(实现层)：组装输入张量
    // S3. TODO(实现层)：局部 RunOptions + 一次性看护线程，到点 SetTerminate()
    // S4. TODO(实现层)：Run → 取**首个输出**（契约侧已约定模型必须单输出）
    // S5. 返回空列表表示失败 / 超时，由 decisionMaker 映射为无效 Decision
    (void)timeoutMs;
    return std::vector<float>();
}

int ONNXruntime::inputDim() {
    // S1. 会话无效 → 返回 0（调用方据此判失败）
    if (!session_) {
        return 0;
    }

    // S2. TODO(实现层)：取输入形状末维；取不到 → 0
    return 0;
}

} // namespace sched
