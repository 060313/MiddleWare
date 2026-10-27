// SPDX-License-Identifier: MIT
// ONNXruntime 的链接期桩：提供可编程的会话行为，替代依赖 SDK 的真实实现
//
// 用途：agent 层（decisionMaker / schedManager）的逻辑与 ONNX Runtime SDK 无关，
//       但链接期需要 ONNXruntime 的三个符号。测试链路用本桩替代 common/src/ONNXruntime.cpp，
//       即可在无 SDK 的机器上把整条决策链跑通（真实实现仍在容器 / 真机上验证）。

#ifndef SCHED_TEST_ONNX_STUB_H
#define SCHED_TEST_ONNX_STUB_H

#include <string>
#include <vector>

namespace sched {
namespace stub {

struct OnnxState {
    bool loadOk = true;             // load 的成败开关
    int dim = 0;                    // inputDim 的返回值（0 表示会话无效）
    std::vector<float> output;      // infer 的返回值（空 = 推理失败）
    int loadCalls = 0;              // prepare 被调用的次数
    int inferCalls = 0;
    std::string lastPath;           // **已生效**会话的模型路径（commit 时更新，prepare 不算）
    std::vector<float> lastInput;   // 最近一次喂进模型的特征向量

    void reset() { *this = OnnxState(); }
};

OnnxState& onnx();

} // namespace stub
} // namespace sched

#endif // SCHED_TEST_ONNX_STUB_H
