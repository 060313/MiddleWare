// SPDX-License-Identifier: MIT
// systemManager —— 管理器 · System 子系统 · private（详细设计 §3.11）
//
// 职责：汇聚 CPU / NPU 两路设备状态并缓存为 systemInfo；按目标设备路由调频动作。
// 设计要点：
//   - cpu / npu 为**组合**关系：由本类在构造内 new、在析构内 delete，是二者唯一所有者；
//   - 后台 refreshSystemInfo 定时刷新缓存（由 Timer 到期回调驱动），前台 askSystemInfo 纯读；
//   - **跨线程共享的唯一一处**：systemInfo 受 sysMutex 保护——监控线程写、服务线程读；
//   - **读设备在锁外、写缓存才持写锁**，避免长时间持锁（I/O 不进临界区）；
//   - 本类**不持采样间隔**：间隔归 Timer（§3.5），本类只管「刷一次」。

#ifndef SCHED_SYSTEM_SYSTEMMANAGER_H
#define SCHED_SYSTEM_SYSTEMMANAGER_H

#include <mutex>
#include <string>

#include "common/include/SchedTypes.h"

namespace sched {

class CPU;
class NPU;

class systemManager {
public:
    // 三个路径为空则退化为对应宏默认（CPU_FREQ_PATH / NPU_FREQ_PATH / PROC_STAT_PATH）；
    // 四个收窄值非负，0 表不限。构造内创建 CPU / NPU 并注入其约束，**不读设备节点**。
    systemManager(const std::string& cpuFreqPath,
                  const std::string& npuFreqPath,
                  const std::string& procStatPath,
                  int cpuFreqMin = 0,
                  int cpuFreqMax = 0,
                  int npuFreqMin = 0,
                  int npuFreqMax = 0);
    ~systemManager();

    // 持读锁取 systemInfo 缓存副本返回（纯读，不触发采样）
    SystemInfo askSystemInfo();

    // 定时刷新入口：读 NPU / CPU → 组装 SystemInfo（含四个特征分量）→ setSystemInfo
    bool refreshSystemInfo();

    // 持写锁覆盖式写入缓存（纯 setter）
    void setSystemInfo(const SystemInfo& info);

    // 按设备标识**二选一**路由（CPU / NPU 互斥）；目标设备未创建时回 deviceUnknown
    ResultInfo setFreq(const Freq& freq);

private:
    CPU* cpu_ = nullptr;       // 组合（生存期由本类负责）
    NPU* npu_ = nullptr;       // 组合（生存期由本类负责）
    SystemInfo systemInfo_;    // 最近一次刷新的缓存（跨线程读写）
    std::mutex sysMutex_;      // 保护 systemInfo_；**只保护内存缓存，不保护设备 I/O**
};

} // namespace sched

#endif // SCHED_SYSTEM_SYSTEMMANAGER_H
