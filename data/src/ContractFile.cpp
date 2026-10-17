// SPDX-License-Identifier: MIT
// ContractFile 实现（详细设计 §4.5.4 / §4.4）
//
// TODO(实现层)：JSON 解析待补（parse 部分）。契约字段形态见 §4.4：
//   {"input_dim":9,
//    "input_fields":[{"name":"phase","source":"task","raw_min":0,"raw_max":1,"min":0.0,"max":1.0}, ...],
//    "output_dim":N,
//    "actions":[{"device":"cpu","freq_khz":408000}, ...]}
// 解析只填数据、不做业务判断；一致性校验交给 contractValidate（第 1、2 道）。

#include "data/include/ContractFile.h"

#include "common/include/FileService.h"

namespace sched {

bool ContractFile::loadContract(const std::string& contractPath, Contract& out) {
    // S1. 路径非空
    if (contractPath.empty()) {
        return false;
    }

    // S2. 读全文；读失败（文件缺失）→ false
    const std::string text = FileService::read(contractPath);
    if (text.empty()) {
        return false;
    }

    // S3. TODO(实现层)：解析 JSON；格式非法 → false
    // S4. TODO(实现层)：校验必需字段存在（input_dim / input_fields / output_dim / actions）；
    //     任一缺失 → false
    // S5. TODO(实现层)：写入临时对象 → 校验全过才提交出参
    (void)text;
    (void)out;
    return false;
}

bool ContractFile::contractValidate(const std::string& contractPath) {
    // S1. 路径非空
    if (contractPath.empty()) {
        return false;
    }

    // S2. 解析到临时副本（不污染任何已生效内容）
    Contract tmp;
    if (!loadContract(contractPath, tmp)) {
        return false;
    }

    // S3.1 input_dim 与 input_fields 项数一致
    if (tmp.inputDim != static_cast<int>(tmp.inputFields.size())) {
        return false;
    }

    // S3.2 output_dim 与 actions 条数一致
    if (tmp.outputDim != static_cast<int>(tmp.actions.size())) {
        return false;
    }

    // S4. 全部一致
    return true;
}

} // namespace sched
