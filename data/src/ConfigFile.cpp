// SPDX-License-Identifier: MIT
// ConfigFile 实现（骨架 —— 待填充）
//
// TODO(实现层)：
//   - getConfig()：打开 configPath → 逐行解析 INI（[section] / key = value）
//                  → 存入内部映射；失败不影响已有内容
//   - configValidate()：检查必需节与键是否存在、整型取值是否合法

#include "data/include/ConfigFile.h"

namespace sched {

bool ConfigFile::getConfig(const std::string& configPath) {
    // TODO(实现层)
    path_ = configPath;
    return false;
}

bool ConfigFile::configValidate(const std::string& configPath) {
    // TODO(实现层)
    (void)configPath;
    return false;
}

} // namespace sched
