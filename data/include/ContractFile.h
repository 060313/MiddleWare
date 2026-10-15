// SPDX-License-Identifier: MIT
// ContractFile —— 契约文件封装（详见详细设计 §4.5.4）
//
// 属 common 基础设施层，不计入 12 个设计类。
// 职责：解析模型契约（JSON 文本格式）。
// 边界：只负责解析与内部一致性校验。

#ifndef SCHED_DATA_CONTRACTFILE_H
#define SCHED_DATA_CONTRACTFILE_H

#include <string>

namespace sched {

class ContractFile {
public:
    ContractFile() = default;

    // 解析契约；文件缺失、格式非法、字段缺失均返回 false（收完整路径）。
    bool loadContract(const std::string& contractPath);

    // 校验内部一致性（见 §4.4 三道校验前两条）。
    bool contractValidate(const std::string& contractPath);

private:
    std::string path_;  // 最近一次读取的契约路径
};

} // namespace sched

#endif // SCHED_DATA_CONTRACTFILE_H
