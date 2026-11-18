// SPDX-License-Identifier: MIT
// Timer 实现（详细设计 §3.5）
//
// 线程模型：start / stop 由装配层调用，poll 由监控线程单独调用——
// 只有「装载前」与「装载后」两次交接，运行期无并发写，故不设互斥。

#include "common/include/Timer.h"

#include <utility>

namespace sched {

namespace {
constexpr int kDefaultIntervalMs = 500;
} // namespace

int Timer::intervalMs_ = kDefaultIntervalMs;
std::chrono::steady_clock::time_point Timer::lastTick_ = std::chrono::steady_clock::now();
std::function<void()> Timer::callback_ = nullptr;
bool Timer::armed_ = false;

bool Timer::start(int intervalMs, std::function<void()> callback) {
    // S1. 前置：回调必须存在，否则装载了也无人可调
    if (!callback) {
        return false;
    }

    // S2. 写入间隔（非正值回退 500）与回调
    intervalMs_ = (intervalMs > 0) ? intervalMs : kDefaultIntervalMs;
    callback_ = std::move(callback);

    // S3. 以当前时刻为起点，置装载标志
    lastTick_ = std::chrono::steady_clock::now();
    armed_ = true;
    return true;
}

void Timer::stop() {
    // S1. 先置标志，避免 poll 在本步与 S2 之间取到半状态
    armed_ = false;

    // S2. 清回调（释放捕获的对象引用，防止悬挂）
    callback_ = nullptr;
}

void Timer::poll() {
    // S1. 未装载 → 直接返回（不阻塞、不睡眠）
    if (!armed_) {
        return;
    }

    // S2. 取当前时刻，未达间隔 → 返回
    const std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
    const auto elapsedMs =
        std::chrono::duration_cast<std::chrono::milliseconds>(now - lastTick_).count();
    if (elapsedMs < intervalMs_) {
        return;
    }

    // S3. **先重置 lastTick 再回调**：回调耗时不从下一周期里扣
    lastTick_ = now;
    if (callback_) {
        callback_();
    }
}

} // namespace sched
