// SPDX-License-Identifier: MIT
// ModelFile 实现（详细设计 §4.5.3）
//
// 只做文件级校验；模型文件的「读取」不在此处——那属于建会话，归 ONNXruntime。

#include "data/include/ModelFile.h"

#include <fstream>

namespace sched {

bool ModelFile::modelValidate(const std::string& modelPath) {
    // S1. 路径非空
    if (modelPath.empty()) {
        return false;
    }

    // S2. 文件可读（存在 + 可打开）——失败即返回
    std::ifstream ifs(modelPath, std::ios::in | std::ios::binary | std::ios::ate);
    if (!ifs.is_open()) {
        return false;
    }

    // S3. 文件非空校验：长度 0 → false。
    //     此处是**普通文件**，用文件长度安全；sysfs / proc 才禁止用 st_size（那类读取走 FileService）
    const std::streamoff size = ifs.tellg();
    return size > 0;
}

} // namespace sched
