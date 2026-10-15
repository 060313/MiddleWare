// SPDX-License-Identifier: MIT
// SystemInterface —— 门面类 · System 子系统 · public（详见详细设计 §7.9）
//
// 职责：System 子系统对外唯一门面，供 Agent 侧 systemMonitor 调用。
// 设计要点：mgr 关联构造注入；getInfo / apply 均为转发。

#ifndef SCHED_SYSTEM_SYSTEMINTERFACE_H
#define SCHED_SYSTEM_SYSTEMINTERFACE_H

#include "common/include/SchedTypes.h"

namespace sched {

class SystemManager;

class SystemInterface {
public:
    explicit SystemInterface(SystemManager* mgr);
    ~SystemInterface();

    // 转交 mgr->getInfo()，汇聚设备状态返回。
    SystemInfo getInfo();

    // 转交 mgr->apply(action)，由其路由到具体设备。
    bool apply(const DeviceAction& action);

private:
    SystemManager* mgr_ = nullptr;  // 关联，构造注入
};

} // namespace sched

#endif // SCHED_SYSTEM_SYSTEMINTERFACE_H
