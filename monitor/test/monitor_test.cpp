// monitor 冒烟测试：Monitor 为纯计算，无 I/O、无副作用、无阻塞。
#include <cmath>
#include <cstdio>

#include "Monitor.h"

namespace {

int g_failed = 0;

void expectNear(const char* name, double got, double want) {
    const bool ok = std::fabs(got - want) < 1e-9;
    std::printf("[%s] %-18s got=%.6f want=%.6f\n",
                ok ? "PASS" : "FAIL", name, got, want);
    if (!ok) ++g_failed;
}

}  // namespace

int main() {
    const monitor::Monitor mon;

    // ---- cpuUsage：差分求利用率 ----
    expectNear("cpu 60% busy",     mon.cpuUsage(1000, 400, 2000, 800),  0.6);   // dTotal=1000 dIdle=400
    expectNear("cpu all idle",     mon.cpuUsage(1000, 400, 2000, 1400), 0.0);
    expectNear("cpu all busy",     mon.cpuUsage(1000, 400, 2000, 400),  1.0);
    expectNear("first sample",     mon.cpuUsage(1000, 400, 1000, 400),  0.0);
    expectNear("counter regress",  mon.cpuUsage(2000, 800, 1000, 400),  0.0);
    expectNear("idle regress",     mon.cpuUsage(1000, 900, 2000, 100),  1.0);   // dIdle 视为 0

    // ---- npuUsage：三核取最大 ----
    sched::NpuState state;
    state.load  = {0.11, 0.62, 0.93};
    state.valid = true;
    expectNear("npu peak core",    mon.npuUsage(state), 0.93);

    state.valid = false;
    expectNear("npu invalid",      mon.npuUsage(state), 0.0);

    std::printf("[monitor_test] %s\n", g_failed == 0 ? "ALL PASS" : "FAILURES");
    return g_failed == 0 ? 0 : 1;
}
