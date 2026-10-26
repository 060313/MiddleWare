// SPDX-License-Identifier: MIT
// systemInterface —— 门面类 · System 子系统 · public（详细设计 §3.10）
//
// 职责：System 子系统对外唯一门面，供 Agent 侧的 systemMonitor 调用。
// 设计依据：只有 mgr 一个关联属性（不拥有）；两个方法都是纯转发，原样返回 systemManager 的结果。

#ifndef SCHED_SYSTEM_SYSTEMINTERFACE_H
#define SCHED_SYSTEM_SYSTEMINTERFACE_H

#include "common/include/SchedTypes.h"

namespace sched {

class systemManager;

class systemInterface {
public:
    // 构造注入；只做赋值、不做 I/O
    explicit systemInterface(systemManager* mgr);
    ~systemInterface();

    // 转交 mgr->askSystemInfo()：取缓存快照（纯读，不触发采样）
    SystemInfo getSystemInfo();

    // 转交 mgr->setFreq()：由 systemManager 二选一路由到 CPU / NPU
    ResultInfo setFreq(const Freq& freq);

private:
    systemManager* mgr_ = nullptr;   // 关联，构造注入
};

} // namespace sched

#endif // SCHED_SYSTEM_SYSTEMINTERFACE_H
