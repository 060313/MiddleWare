// monitor 冒烟测试
#include <cstdio>

#include "Monitor.h"

int main() {
    auto npu = sched::INpu::Create();
    monitor::Monitor mon(npu.get());

    const double ucpu = mon.get_ucpu();
    const double unpu = mon.get_unpu();
    std::printf("[monitor_test] ucpu=%.4f  unpu=%.4f\n", ucpu, unpu);

    // TODO: 补充断言（ucpu/unpu 落在 [0,1]；/proc 不存在时应返回 0 而非异常）
    std::printf("[monitor_test] TODO: assertions pending\n");
    return 0;
}
