#include "FeatureEncoder.h"

namespace agent {

bool FeatureEncoder::loadContract(const std::string& path) {
    // TODO: 解析 model_contract.json 的 input_dim 与 state_bounds
    (void)path;
    return false;
}

std::vector<float> FeatureEncoder::encode(const common::TaskFeature& task,
                                          const common::SystemFeature& sys) const {
    // TODO: 按 state_bounds 归一化，拼成定长向量（默认 8 维）
    (void)task;
    (void)sys;
    return std::vector<float>(static_cast<std::size_t>(m_inputDim), 0.0f);
}

}  // namespace agent
