// SPDX-License-Identifier: MIT
// ModelFile 实现（骨架 —— 待填充）
//
// TODO(实现层)：
//   - loadModel()：校验路径非空 → 打开模型文件读取（本类不做推理）
//   - modelValidate()：路径非空校验 + 文件可读性探测

#include "data/include/ModelFile.h"

namespace sched {

bool ModelFile::loadModel(const std::string& modelPath) {
    // TODO(实现层)
    path_ = modelPath;
    return false;
}

bool ModelFile::modelValidate(const std::string& modelPath) {
    // TODO(实现层)
    (void)modelPath;
    return false;
}

} // namespace sched
