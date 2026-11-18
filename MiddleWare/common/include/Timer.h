// SPDX-License-Identifier: MIT
// Timer —— 公共基础设施类 · common 子系统 · public（详细设计 §3.5）
//
// 职责：**只做到期判定与回调派发**，不含线程、不含睡眠。
// 设计要点：
//   - **全静态类**（§1）：不实例化、无构造与析构；
//   - **不创建线程**——由调用方（装配层起的监控线程）在自己的循环里反复调 poll() 推进时间；
//   - 监控线程的**循环条件不归本类**（D-36）：本类只有 armed（是否装载），
//     线程开关由装配层持有的 std::atomic<bool> 承担。

#ifndef SCHED_COMMON_TIMER_H
#define SCHED_COMMON_TIMER_H

#include <chrono>
#include <functional>

namespace sched {

class Timer {
public:
    // 全静态类：禁止实例化
    Timer() = delete;

    // 装载定时器：写入间隔与回调、把 lastTick 置当前时刻、armed 置真。
    // 间隔非正 → 回退 500 ms；回调为空 → 返回 false 且不装载。
    // **不创建线程**，调用方自负 poll() 节奏。
    static bool start(int intervalMs, std::function<void()> callback);

    // 卸载定时器：先置 armed = false，再清回调。**不回收外部资源、不等待线程**。
    static void stop();

    // 推进时间：已装载且距 lastTick 达到 intervalMs → **先重置 lastTick 再调 callback**。
    // 未到期或未装载 → 直接返回。**不阻塞、不睡眠、不建线程**。
    static void poll();

private:
    static int intervalMs_;                                 // 定时间隔（毫秒），初值 500
    static std::chrono::steady_clock::time_point lastTick_; // 上一次到期判定时刻
    static std::function<void()> callback_;                 // 到期回调
    static bool armed_;                                     // 装载标志
};

} // namespace sched

#endif // SCHED_COMMON_TIMER_H
