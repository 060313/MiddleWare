#pragma once

// 决策落地：拆解为设备动作并校验结果。
// 见《软件详细设计文档》4.4、5.3。

#include <memory>

#include "Types.h"

namespace agent {

class SystemMonitor;

class DecisionExecutor {
public:
    explicit DecisionExecutor(std::shared_ptr<SystemMonitor> monitor);
    ~DecisionExecutor();

    // 把 Decision 拆成 CPU / NPU 动作逐个下发，汇总为 ActionInfo
    common::ActionInfo exec(const common::TaskFeature& task,
                            const common::Decision& decision);

private:
    std::shared_ptr<SystemMonitor> m_monitor;
};

}  // namespace agent
