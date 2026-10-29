// SPDX-License-Identifier: MIT
// Timer 单元测试（详细设计 §3.5 / D-07）
//
// 重点覆盖：
//   - **不创建线程**、poll 不阻塞（测试自身用 sleep 推进时间，恰好证明这一点）；
//   - 非正间隔回退 500 ms、空回调拒装载；
//   - 到期后**先重置 lastTick 再回调**（回调耗时不累积成漂移）；
//   - 回调抛异常就地吞掉，定时链不断（§3.5 S5）；
//   - stop 后不再触发。

#include <chrono>
#include <stdexcept>
#include <string>
#include <thread>

#include "common/include/Timer.h"
#include "test_util.h"

using sched::Timer;
using namespace std::chrono_literals;

TEST_CASE(start_rejects_null_callback) {
    Timer::stop();
    CHECK_FALSE(Timer::start(100, nullptr));
    Timer::poll();          // 未装载：不崩、不调用任何东西
    Timer::stop();
}

TEST_CASE(non_positive_interval_falls_back_to_500ms) {
    Timer::stop();
    int count = 0;
    CHECK_TRUE(Timer::start(0, [&count] { ++count; }));

    Timer::poll();
    CHECK_EQ(count, 0);                       // 立即 poll 不该到期
    std::this_thread::sleep_for(200ms);
    Timer::poll();
    CHECK_EQ(count, 0);                       // 200 ms 仍未到回退后的 500 ms
    std::this_thread::sleep_for(400ms);
    Timer::poll();
    CHECK_EQ(count, 1);                       // 600 ms：到期一次

    Timer::stop();
}

TEST_CASE(fires_once_per_interval) {
    Timer::stop();
    int count = 0;
    CHECK_TRUE(Timer::start(80, [&count] { ++count; }));

    Timer::poll();
    CHECK_EQ(count, 0);
    std::this_thread::sleep_for(120ms);
    Timer::poll();
    CHECK_EQ(count, 1);
    Timer::poll();                            // 紧接着再 poll：lastTick 已重置，不重复触发
    CHECK_EQ(count, 1);

    Timer::stop();
}

TEST_CASE(no_drift_accumulation) {
    // 300 ms 内、间隔 50 ms：期望 4~8 次（允许调度抖动，但要排除「只触发 1 次」这种漂移）
    Timer::stop();
    int count = 0;
    CHECK_TRUE(Timer::start(50, [&count] { ++count; }));
    for (int i = 0; i < 30; ++i) {
        std::this_thread::sleep_for(10ms);
        Timer::poll();
    }
    CHECK_TRUE(count >= 4);
    CHECK_TRUE(count <= 8);

    Timer::stop();
}

TEST_CASE(stop_halts_dispatch) {
    Timer::stop();
    int count = 0;
    CHECK_TRUE(Timer::start(20, [&count] { ++count; }));
    std::this_thread::sleep_for(40ms);
    Timer::stop();

    Timer::poll();
    CHECK_EQ(count, 0);
    std::this_thread::sleep_for(60ms);
    Timer::poll();
    CHECK_EQ(count, 0);
}

TEST_CASE(callback_exception_is_swallowed) {
    // 回调抛异常必须就地吞掉：监控线程的主循环就是 poll()，异常逃出去会杀线程
    Timer::stop();
    int count = 0;
    CHECK_TRUE(Timer::start(20, [&count] {
        ++count;
        throw std::runtime_error("采样失败");
    }));

    std::this_thread::sleep_for(40ms);
    Timer::poll();                            // 不得向调用方传播
    CHECK_EQ(count, 1);

    // 定时链不能断：下一次到期仍然照常回调（再次抛、再次被吞）
    std::this_thread::sleep_for(40ms);
    Timer::poll();
    CHECK_EQ(count, 2);

    Timer::stop();
}

TEST_CASE(restart_resets_last_tick) {
    Timer::stop();
    int count = 0;
    CHECK_TRUE(Timer::start(200, [&count] { ++count; }));
    Timer::poll();
    CHECK_EQ(count, 0);

    CHECK_TRUE(Timer::start(10, [&count] { ++count; }));   // 重装载：起点重新计时
    Timer::poll();
    CHECK_EQ(count, 0);
    std::this_thread::sleep_for(30ms);
    Timer::poll();
    CHECK_EQ(count, 1);

    Timer::stop();
}

TEST_MAIN()
