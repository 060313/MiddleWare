// SPDX-License-Identifier: MIT
// ContractFile —— 契约文件封装（详细设计 §4.5.4 / §4.4 / D-25 / D-26）
//
// 属 common 基础设施层，**不计入 §1 的 13 个设计类**。
// 职责：解析契约文本（JSON）并校验其**内部一致性**（§4.4 三道校验的第 1、2 道）。
// 边界：
//   - 只看契约自身，不访问推理会话；第 3 道（模型输入维 vs input_dim）由 decisionMaker 承担；
//   - 只读、只校验，不写契约。

#ifndef SCHED_DATA_CONTRACTFILE_H
#define SCHED_DATA_CONTRACTFILE_H

#include <string>

#include "common/include/SchedTypes.h"

namespace sched {

class ContractFile {
public:
    ContractFile() = default;

    // 解析契约并经出参返回（**调用方持临时副本，校验全过才提交**）。
    // 收**完整路径**；文件缺失、格式非法、必需字段缺失均返回 false。
    bool loadContract(const std::string& contractPath, Contract& out);

    // 校验内部一致性（§4.4 第 1、2 道）：input_dim 与 input_fields 项数一致、
    // output_dim 与 actions 条数一致。
    bool contractValidate(const std::string& contractPath);
};

} // namespace sched

#endif // SCHED_DATA_CONTRACTFILE_H
