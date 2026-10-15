// SPDX-License-Identifier: MIT
// ONNXruntime —— 公共基础设施 · common 子系统（详见详细设计 §7.4）
//
// 职责：ONNX 推理会话的封装，供 decisionMaker 调用。
// 设计要点：
//   - 本类不持模型路径属性，load 以参数接收路径；
//   - 不区分内部是 DQN 网络还是表格法 Q 表，对运行时都只是一张 .onnx 计算图。
//
// 依赖：ONNX Runtime SDK（本框架为硬依赖，不设可选开关）。
//       SDK 头文件与库路径由 Makefile 的 ONNXRUNTIME_ROOT 提供。

#ifndef SCHED_COMMON_ONNXRUNTIME_H
#define SCHED_COMMON_ONNXRUNTIME_H

#include <memory>
#include <string>
#include <vector>

namespace sched {

class ONNXruntime {
public:
    ONNXruntime();
    ~ONNXruntime();

    ONNXruntime(const ONNXruntime&) = delete;
    ONNXruntime& operator=(const ONNXruntime&) = delete;

    // 读模型 → 建推理会话 → 存 session。前置：路径非空且模型文件可读。
    bool load(const std::string& modelPath);

    // 输入张量 → 执行会话 → 取出输出张量转列表返回。前置：session 有效。
    std::vector<float> infer(const std::vector<float>& features);

private:
    // 推理会话句柄；实现层用 Ort::Session 填充（PIMPL，避免头文件暴露 SDK）
    struct Session;
    std::unique_ptr<Session> session_;
};

} // namespace sched

#endif // SCHED_COMMON_ONNXRUNTIME_H
