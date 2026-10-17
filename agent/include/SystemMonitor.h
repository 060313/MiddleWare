// SPDX-License-Identifier: MIT
// systemMonitor —— 监控门面 · Agent 子系统 · private（详细设计 §3.7）
//
// 职责：Agent 侧通往 System 的**唯一出口**（D-12）——读系统状态、下发频率设置。
// 设计要点：
//   - 只有 sys 一个关联属性（构造注入），**不缓存、不采样**：刷新由后台
//     systemManager::refreshSystemInfo 承担，本类纯转发（读的是 System 侧缓存）；
//   - setFreq 只作通道，**不加工结果**（ok / message / adjusted / reason 原样透传）。

#ifndef SCHED_AGENT_SYSTEMMONITOR_H
#define SCHED_AGENT_SYSTEMMONITOR_H

#include "common/include/SchedTypes.h"

namespace sched {

class systemInterface;

class systemMonitor {
public:
    // 构造注入 System 门面引用；**不拥有 sys**，只做赋值、不做 I/O
    explicit systemMonitor(systemInterface* sys);
    ~systemMonitor();

    // 读 System 侧的 SystemInfo 缓存副本（纯转发）。
    SystemInfo getSystemInfo();

    // 下发一次频率设置（纯转发）；返回 systemInterface 的 ResultInfo。
    ResultInfo setFreq(const Freq& freq);

private:
    systemInterface* sys_ = nullptr;   // 关联，构造注入（生存期由装配层保证）
};

} // namespace sched

#endif // SCHED_AGENT_SYSTEMMONITOR_H
