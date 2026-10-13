#pragma once

// 模型 Q 值 → 离散动作 → Decision。
// 见《软件详细设计文档》4.1、5.3；动作语义来自 model_contract.json 的 actions。

#include <string>
#include <vector>

#include "Types.h"

namespace agent {

class ActionDecoder {
public:
    ActionDecoder() = default;
    ~ActionDecoder() = default;

    bool loadContract(const std::string& path);
    int  actionCount() const { return m_actionCount; }

    common::Decision decode(const std::vector<float>& qValues) const;

private:
    int m_actionCount = 0;
};

}  // namespace agent
