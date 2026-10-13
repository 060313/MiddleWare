#pragma once

// 任务特征 + 系统特征 → 模型输入向量（归一化）。
// 见《软件详细设计文档》4.1、5.3；归一化范围来自 model_contract.json 的 state_bounds。

#include <string>
#include <vector>

#include "Types.h"

namespace agent {

class FeatureEncoder {
public:
    FeatureEncoder() = default;
    ~FeatureEncoder() = default;

    bool loadContract(const std::string& path);
    int  inputDim() const { return m_inputDim; }

    std::vector<float> encode(const common::TaskFeature& task,
                              const common::SystemFeature& sys) const;

private:
    int m_inputDim = 0;
};

}  // namespace agent
