// SPDX-License-Identifier: MIT
// ContractFile 实现（骨架 —— 待填充）
//
// TODO(实现层)：
//   - loadContract()：读 JSON 文本 → 解析 input_dim / state_bounds / output_dim / actions
//                     → 任一缺失或格式非法返回 false
//   - contractValidate()：校验内部一致性
//                         ① input_dim == state_bounds.fields 项数
//                         ② actions 元素数 == output_dim

#include "data/include/ContractFile.h"

namespace sched {

bool ContractFile::loadContract(const std::string& contractPath) {
    // TODO(实现层)
    path_ = contractPath;
    return false;
}

bool ContractFile::contractValidate(const std::string& contractPath) {
    // TODO(实现层)
    (void)contractPath;
    return false;
}

} // namespace sched
