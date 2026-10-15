// SPDX-License-Identifier: MIT
// ModelFile —— 模型文件封装（详见详细设计 §4.5.3）
//
// 属 common 基础设施层，不计入 12 个设计类。
// 职责：模型文件的读取与校验。
// 边界：只负责文本/路径层面的处理，推理由 ONNXruntime 承担。

#ifndef SCHED_DATA_MODELFILE_H
#define SCHED_DATA_MODELFILE_H

#include <string>

namespace sched {

class ModelFile {
public:
    ModelFile() = default;

    // 读入模型文件（收完整路径）。
    bool loadModel(const std::string& modelPath);

    // 校验模型：路径非空、文件可读。
    bool modelValidate(const std::string& modelPath);

private:
    std::string path_;  // 最近一次读取的模型路径
};

} // namespace sched

#endif // SCHED_DATA_MODELFILE_H
