// SPDX-License-Identifier: MIT
// SystemMonitor —— 监控门面 · Agent 子系统 · private（详见详细设计 §7.6）
//
// 职责：Agent 侧系统状态门面。将 System 子系统（systemInterface）采集的
//       设备状态转换为用于决策的特征向量。
// 设计要点：
//   - sys 关联构造注入；
//   - getFeature 内部先调 collect，再据 lastInfo 返回；失败回兜底值；
//   - lastInfo 保存最近一次有效 SystemInfo，作 stale 兜底。

#ifndef SCHED_AGENT_SYSTEMMONITOR_H
#define SCHED_AGENT_SYSTEMMONITOR_H

#include "common/include/SchedTypes.h"

namespace sched {

class systemInterface;

class SystemMonitor {
public:
    // 构造注入 System 子系统门面
    explicit SystemMonitor(systemInterface* sys);
    ~SystemMonitor();

    // 对外取特征：内部先 collect，再据 lastInfo 返回；失败回兜底值。
    SystemFeature getFeature();

private:
    // 采集一次并更新 lastInfo。成功返回 true；失败保持不变。
    bool collect();

    systemInterface* sys_ = nullptr;  // 关联，构造注入
    SystemInfo lastInfo_;             // 最近一次有效快照（stale 兜底）
};

} // namespace sched

#endif // SCHED_AGENT_SYSTEMMONITOR_H
