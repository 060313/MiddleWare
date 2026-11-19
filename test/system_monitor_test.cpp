// SPDX-License-Identifier: MIT
// systemMonitor 单元测试（详细设计 §3.7 / D-12）
//
// 本类是 Agent 通往 System 的唯一出口，且**只作通道**：测试重点不是设备行为，
// 而是「未装配时的兜底」与「转发时一个字段都不加工」。
//
// 注：CPU / NPU 的节点 I/O 尚未实现，档位表为空，故经 systemManager
// 路由后的结果是 3002（档位表不可用）——正好用来验证 reason 是否被原样透传。

#include <string>

#include "agent/include/SystemMonitor.h"
#include "system/include/SystemInterface.h"
#include "system/include/SystemManager.h"
#include "test/test_util.h"

using namespace sched;

TEST_CASE(get_system_info_without_system_returns_zero) {
    systemMonitor monitor{nullptr};
    const SystemInfo info = monitor.getSystemInfo();

    CHECK_NEAR(info.ucpu, 0.0, 1e-9);
    CHECK_NEAR(info.unpu, 0.0, 1e-9);
    CHECK_EQ(info.cpuFreqKHz, 0);
    CHECK_EQ(info.npuFreqKHz, 0);
    CHECK(info.cpuInfo.freqTable.empty());
    CHECK(info.npuInfo.freqTable.empty());
}

TEST_CASE(set_freq_without_system_reports_scheduler_absent) {
    systemMonitor monitor{nullptr};
    Freq freq;
    freq.device = "cpu";
    freq.freqKHz = 1228800;

    const ResultInfo result = monitor.setFreq(freq);
    CHECK(!result.ok);
    CHECK(!result.adjusted);
    CHECK_EQ(result.reason, Reason::schedulerAbsent);
    CHECK_EQ(result.message, std::string("monitor not ready"));
}

TEST_CASE(get_system_info_forwards_cached_snapshot) {
    systemInterface sys{nullptr};        // System 门面未接管理器 → 零值缓存
    systemMonitor monitor{&sys};

    const SystemInfo info = monitor.getSystemInfo();
    CHECK_NEAR(info.ucpu, 0.0, 1e-9);
    CHECK_EQ(info.cpuFreqKHz, 0);
}

TEST_CASE(set_freq_forwards_to_cpu_and_keeps_reason) {
    systemManager mgr{"", "", ""};       // 构造不读节点，档位表待首次采集
    systemInterface sys{&mgr};
    systemMonitor monitor{&sys};

    Freq freq;
    freq.device = "cpu";
    freq.freqKHz = 1228800;

    const ResultInfo result = monitor.setFreq(freq);
    CHECK(!result.ok);
    CHECK_EQ(result.reason, Reason::freqUnavailable);           // 原样透传 3002
    CHECK_EQ(result.message, std::string("freq table unavailable"));
}

TEST_CASE(set_freq_forwards_to_npu) {
    systemManager mgr{"", "", ""};
    systemInterface sys{&mgr};
    systemMonitor monitor{&sys};

    Freq freq;
    freq.device = "npu";
    freq.freqKHz = 1000000;

    const ResultInfo result = monitor.setFreq(freq);
    CHECK(!result.ok);
    CHECK_EQ(result.reason, Reason::freqUnavailable);
}

TEST_CASE(set_freq_unknown_device_reports_device_unknown) {
    systemManager mgr{"", "", ""};
    systemInterface sys{&mgr};
    systemMonitor monitor{&sys};

    Freq freq;
    freq.device = "gpu";
    freq.freqKHz = 1000000;

    const ResultInfo result = monitor.setFreq(freq);
    CHECK(!result.ok);
    CHECK_EQ(result.reason, Reason::deviceUnknown);
}

TEST_CASE(set_freq_empty_device_does_not_reach_any_device) {
    systemManager mgr{"", "", ""};
    systemInterface sys{&mgr};
    systemMonitor monitor{&sys};

    const ResultInfo result = monitor.setFreq(Freq{});   // 空动作（未选中处理器）
    CHECK(!result.ok);
    CHECK_EQ(result.reason, Reason::deviceUnknown);
}

int main() {
    RUN(get_system_info_without_system_returns_zero);
    RUN(set_freq_without_system_reports_scheduler_absent);
    RUN(get_system_info_forwards_cached_snapshot);
    RUN(set_freq_forwards_to_cpu_and_keeps_reason);
    RUN(set_freq_forwards_to_npu);
    RUN(set_freq_unknown_device_reports_device_unknown);
    RUN(set_freq_empty_device_does_not_reach_any_device);

    return test::summary();
}
