// SPDX-License-Identifier: MIT
// ONNXruntime —— 公共基础设施类 · common 子系统 · public（详细设计 §3.4）
//
// 职责：ONNX 推理会话的封装，供 decisionMaker 调用。
// 设计要点：
//   - **全静态类**（§1）：不实例化、无构造与析构；env / session 都是**进程级静态**
//     资源，由静态析构在进程退出时回收（故 D-05 说「decisionMaker 析构不释放会话」）；
//   - **env 必须声明在 session 之前**：保证运行环境先构造、后析构，会话不会活过环境；
//   - 本类不持模型路径属性，load 以参数接收路径；load 覆盖即回收旧会话，无 reset()；
//   - 对运行时而言，DQN 网络与表格法 Q 表都只是一张 .onnx 计算图，本类不做区分。

#ifndef SCHED_COMMON_ONNXRUNTIME_H
#define SCHED_COMMON_ONNXRUNTIME_H

#include <memory>
#include <string>
#include <vector>

namespace sched {

class ONNXruntime {
public:
    // 全静态类：禁止实例化
    ONNXruntime() = delete;

    // 读模型 → 建推理会话 → 存入 session。前置：路径非空且模型文件可读。
    // 失败保持旧会话不变（热切换的「先建后换」由 decisionMaker 编排）。
    static bool load(const std::string& modelPath);

    // 输入张量 → 执行会话（受超时取消约束）→ 取出输出张量转列表返回。
    // timeoutMs 非正值取内部固化默认；**超时或失败一律返回空列表**。
    // 同步 Run() 不能从外部打断，取消必须在本方法内部完成（D-31）。
    static std::vector<float> infer(const std::vector<float>& features, int timeoutMs = 0);

    // 读当前会话的模型输入维度（输入形状**末维**，首维是批维 -1）；
    // session 无效 → 返回 0（调用方据此判失败）。
    static int inputDim();

private:
    // PIMPL：隔离 ONNX Runtime SDK，避免 SDK 头文件污染上游编译单元
    struct Env;      // Ort::Env 的包装
    struct Session;  // Ort::Session 的包装

    static std::unique_ptr<Env> env_;      // **必须声明在 session_ 之前**（先建后毁）
    static std::unique_ptr<Session> session_;
};

} // namespace sched

#endif // SCHED_COMMON_ONNXRUNTIME_H
