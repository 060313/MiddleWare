// SPDX-License-Identifier: MIT
// ConfigFile —— 配置文件封装（详见详细设计 §4.5.2）
//
// 属 common 基础设施层，不计入 12 个设计类。
// 职责：解析 INI 文本格式配置并提供逐键取值。
// 边界：只负责读写文本配置文件，不做模型加载/推理。

#ifndef SCHED_DATA_CONFIGFILE_H
#define SCHED_DATA_CONFIGFILE_H

#include <string>

namespace sched {

class ConfigFile {
public:
    ConfigFile() = default;

    // 读入并解析配置文件；失败不影响已有内容。
    bool getConfig(const std::string& configPath);

    // 校验配置：节与键是否存在、取值格式是否合法。
    bool configValidate(const std::string& configPath);

private:
    std::string path_;  // 最近一次读取的配置路径
};

} // namespace sched

#endif // SCHED_DATA_CONFIGFILE_H
