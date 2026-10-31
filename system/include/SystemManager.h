// SPDX-License-Identifier: MIT
// systemManager —— 管理器 · System 子系统 · private（详细设计 §3.11）
//
// 职责：汇聚 CPU / NPU 两路状态并缓存为 systemInfo；按设备标识路由调频动作。
// 设计依据：
//   - cpu_ / npu_ 为**组合**，unique_ptr 持有（裸指针版在第二个 new 抛异常时会泄漏第一个）；
//     二者在头文件里只前置声明，故**析构体必须定义在 .cpp**；
//   - refreshSystemInfo 是 systemInfo_ 的**唯一写入者**，不对外开 setter —— 这是
//     D-16「陈旧度 ≤ 一个 intervalMs」的前提；
//   - 跨线程共享**两处**：systemInfo_（sysLock_）与 CPU / NPU 对象自身（各一把 devMutex_）；
//   - **读设备在锁外、写缓存才持锁**；本类不持采样间隔（归 Timer，§3.5）。

#ifndef SCHED_SYSTEM_SYSTEMMANAGER_H
#define SCHED_SYSTEM_SYSTEMMANAGER_H

#include <memory>
#include <mutex>
#include <string>

#include "common/include/SchedDefs.h"
#include "common/include/SchedTypes.h"

namespace sched {

class CPU;
class NPU;

class systemManager {
public:
    // 四个路径为空则各自退化为对应宏默认（NPU 状态路径取 NPU_STAT_PATH，**不随 NPU 频率路径联动**）；
    // 四个收窄值非负，0 表不限。构造内创建 CPU / NPU，**不读设备节点**
    systemManager(const std::string& cpuFreqPath=std::string(CPU_FREQ_PATH),
                  const std::string& npuPath=std::string(NPU_FREQ_PATH),
                  const std::string& cpuStatPath=std::string(CPU_STAT_PATH),
                  const std::string& npuStatPath=std::string(NPU_STAT_PATH),
                  int cpuFreqMin = 0,
                  int cpuFreqMax = 0,
                  int npuFreqMin = 0,
                  int npuFreqMax = 0);
    ~systemManager();

    // 持锁把 systemInfo 副本写入出参 out（纯读，不触发采样）；不出借内部状态
    ResultInfo askSystemInfo(SystemInfo& out);

    // 定时刷新入口：读两路设备（锁外）→ 取四个特征分量 → 持锁覆盖缓存
    bool refreshSystemInfo();

    // 按设备标识**二选一**路由；目标设备未创建时回 deviceUnknown
    ResultInfo setFreq(const Freq& freq);

private:
    //操作锁
    template<typename Function>
    auto withSysLock(Function&& fun) {
        const std::lock_guard<std::mutex> lock(sysLock_);
        return fun();
    }
    // 组合：unique_ptr 持有，析构体在 .cpp（见 SystemManager.cpp）
    std::unique_ptr<CPU> cpu_;
    std::unique_ptr<NPU> npu_;

    SystemInfo systemInfo_;    // 最近一次刷新的缓存（跨线程读写）
    ResultInfo infoState;       
    std::mutex sysLock_;      // 只保护 systemInfo_，不保护设备 I/O

};

} // namespace sched

#endif // SCHED_SYSTEM_SYSTEMMANAGER_H
