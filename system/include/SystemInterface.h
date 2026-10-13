#pragma once

// System 子系统对外唯一入口（类可见性 public）。
// 见《软件详细设计文档》5.4、6.2。

#include <memory>

#include "StateSnapshot.h"
#include "Types.h"

namespace syscore {

class SystemManager;

class SystemInterface {
public:
    SystemInterface();
    ~SystemInterface();

    SystemInterface(const SystemInterface&) = delete;
    SystemInterface& operator=(const SystemInterface&) = delete;

    // 采集系统状态量（UC2）
    bool getInfo(common::SystemFeature& out);

    // 下发单设备动作（UC3）
    common::ResultInfo applyAction(const common::DeviceAction& action);

    // 共享内存状态快照指针（供监控线程写入）
    common::StateSnapshot* snapshot();

private:
    std::unique_ptr<SystemManager> m_manager;   // pimpl：实现细节不外泄
};

}  // namespace syscore
