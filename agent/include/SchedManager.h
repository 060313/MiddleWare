#pragma once

// Agent 子系统对外主控（流程总控 + 模式状态机）。
// 见《软件详细设计文档》5.3、6.3、6.5。

#include <memory>
#include <shared_mutex>

#include "Types.h"

namespace agent {

class SystemMonitor;
class DecisionMaker;
class DecisionExecutor;

class SchedManager {
public:
    SchedManager();
    ~SchedManager();

    SchedManager(const SchedManager&) = delete;
    SchedManager& operator=(const SchedManager&) = delete;

    // 前置：m_mode != off；后置：返回与请求一一对应的 ActionInfo（UC1）
    common::ActionInfo handleTask(const common::TaskFeature& task);

    // 模式取值合法时更新；off→static/auto 需模型可用
    bool setMode(common::SchedMode mode);

    common::SchedMode mode() const;

private:
    std::shared_ptr<SystemMonitor>    m_monitor;    // 门面，不可空
    std::shared_ptr<DecisionMaker>    m_maker;      // 推理器
    std::shared_ptr<DecisionExecutor> m_executor;   // 动作执行器
    common::SchedMode                 m_mode = common::SchedMode::Auto;
    mutable std::shared_mutex         m_modeMutex;  // 模式读写保护
};

}  // namespace agent
