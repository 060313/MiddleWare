// SPDX-License-Identifier: MIT
// ModelFile —— 模型文件封装（详细设计 §4.5.3 / D-26）
//
// 属 common 基础设施层，**不计入 §1 的 13 个设计类**。
// 职责：**只做文件级校验**（路径非空、文件可读、文件非空）。
// 边界：
//   - **不提供「读取」方法**：模型文件的读取即建立推理会话，已由 ONNXruntime::load 承担，
//     本类再读一遍纯属冗余；
//   - 不做魔数与 ONNX 头部解析——头部解析成本高，且 ONNXruntime::load 的失败信息更准确；
//   - 三段分工：ModelFile 管文件级、ONNXruntime 管会话级、decisionMaker 管配对级（§4.4 第 3 道）。

#ifndef SCHED_DATA_MODELFILE_H
#define SCHED_DATA_MODELFILE_H

#include <string>

namespace sched {

class ModelFile {
public:
    ModelFile() = default;

    // 校验模型：路径非空 → 文件可读 → 文件非空；任一不满足返回 false
    bool modelValidate(const std::string& modelPath);
};

} // namespace sched

#endif // SCHED_DATA_MODELFILE_H
