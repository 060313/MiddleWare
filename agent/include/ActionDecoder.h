#pragma once

// 模型 Q 值 → 离散动作 → Decision。
// 见《软件详细设计文档》4.1、5.3；动作语义来自 model_contract.json 的 actions。

#include <string>
#include <vector>

#include "Types.h"

namespace agent {

class ActionDecoder {
public:
    // 契约 actions[] 中的单条动作语义（Q 值下标 → 设备动作的展开依据）
    struct ActionSpec {
        int         index = 0;         // 动作编号（与 Q 值下标一致）
        std::string name;              // 语义名，如 "cpu_heavy"
        double      cpuShare = 0.0;    // CPU 资源份额；0 表示本动作不下发 CPU 动作
        int         batchSize = 0;     // 目标批大小
        int         cpuFreqLevel = 0;  // CPU 档位序号（0 最低）
        int         npuFreqLevel = 0;  // NPU 档位序号（0 最低）
    };

    ActionDecoder() = default;
    ~ActionDecoder() = default;

    bool loadContract(const std::string& path);
    int  actionCount() const { return m_actionCount; }

    common::Decision decode(const std::vector<float>& qValues) const;

private:
    int                     m_actionCount = 0;
    std::vector<ActionSpec> m_actions;   // 由 loadContract 填充，长度与 m_actionCount 一致
};

}  // namespace agent
