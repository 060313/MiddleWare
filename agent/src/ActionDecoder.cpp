#include "ActionDecoder.h"

namespace agent {

bool ActionDecoder::loadContract(const std::string& path) {
    // TODO: 解析 model_contract.json 的 output_dim 与 actions 映射
    (void)path;
    return false;
}

common::Decision ActionDecoder::decode(const std::vector<float>& qValues) const {
    // TODO: argmax(qValues) → actionIndex → 按 actions 映射展开为设备动作
    (void)qValues;
    return common::Decision{};
}

}  // namespace agent
