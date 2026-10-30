// SPDX-License-Identifier: MIT
// system 子系统单元测试（详细设计 §3.10~§3.13）
//
// 手法：**伪 sysfs 桩** —— CPU / NPU / systemManager 的路径都可注入，故在 build/test_fake/
// 下造一棵假节点树即可在任意平台跑通采集与调频全链路，不需要真实设备与 root 权限。
//
// 覆盖：解析去重升序、档位收窄、收窄后退化、D-21 单位折算、越界与并列夹取、
//       governor 切换、D-32 回读不比对、/proc/stat 两帧差分、档位表懒加载、
//       节点缺失兜底（含「装表失败才算真 3002」）、并发。
//
// 探针：档位表落在 deviceFile 内且为 protected，而「解析去重升序 / 收窄 / 收窄退化」
//       三者的效果都被 setFreq 的夹取吸收掉了，没有公开出口可断言 —— 故派生 CpuProbe /
//       NpuProbe 两个只读探针把表取出来（不改任何状态）；其余用例一律走公开接口。

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <string>
#include <thread>
#include <vector>

#include "common/include/FileService.h"
#include "common/include/SchedTypes.h"
#include "system/include/CPU.h"
#include "system/include/NPU.h"
#include "system/include/SystemManager.h"
#include "test/test_util.h"

// 被测类型全部在 sched 下；测试文件本身不设命名空间，统一在此处引入
using namespace sched;

namespace fs = std::filesystem;

namespace {

const std::string kRoot = "build/test_fake";
const std::string kCpu = kRoot + "/cpu0/cpufreq";
const std::string kNpu = kRoot + "/npu";
const std::string kProc = kRoot + "/proc_stat";

// 只读探针：仅把 protected 的 freqTable_ 取出来，不暴露任何写入口
struct CpuProbe : CPU {
    using CPU::CPU;
    const std::vector<int>& table() const { return freqTable_; }
};

struct NpuProbe : NPU {
    using NPU::NPU;
    const std::vector<int>& table() const { return freqTable_; }
};

void put(const std::string& path, const std::string& text) {
    FileService::write(path, text);
}

std::string get(const std::string& path) {
    return FileService::read(path);
}

// 建一棵全新的桩树（每次运行都重置，避免上轮写入污染）
void makeStubs() {
    fs::remove_all(kRoot);
    fs::create_directories(kCpu);
    fs::create_directories(kNpu + "/userspace");
}

void resetCpu() {
    put(kCpu + "/scaling_available_frequencies", "1800000 1600000 2000000 1800000\n");
    put(kCpu + "/scaling_cur_freq", "1800000\n");
    put(kCpu + "/scaling_governor", "userspace\n");
    put(kCpu + "/scaling_setspeed", "0\n");
}

void resetNpu() {
    put(kNpu + "/available_frequencies", "1000000000 800000000 1200000000\n");
    put(kNpu + "/cur_freq", "1000000000\n");
    put(kNpu + "/governor", "userspace\n");
    put(kNpu + "/load", "42\n");
    put(kNpu + "/userspace/set_freq", "0\n");
}

void resetProc(const std::string& user, const std::string& system,
               const std::string& idle, const std::string& iowait) {
    put(kProc, "cpu  " + user + " 0 " + system + " " + idle + " " + iowait +
                   " 0 0 0 0 0\ncpu0 500 0 250 4000 50 0 0 0 0 0\n");
}

// 1. 解析：乱序 + 重复 → 去重升序
void testParseTable() {
    std::printf("1. 档位表解析（乱序 + 重复 → 去重升序）\n");
    resetCpu();
    CpuProbe cpu(kCpu, kProc);
    cpu.getCPUInfo();                        // 触发懒加载装表
    const std::vector<int>& table = cpu.table();
    CHECK(table.size() == 3);
    CHECK(table[0] == 1600000 && table[1] == 1800000 && table[2] == 2000000);
}

// 2. 收窄：上下限生效
void testNarrow() {
    std::printf("2. 档位收窄（freqMin / freqMax）\n");
    resetCpu();
    CpuProbe cpu(kCpu, kProc, 1700000, 1900000);
    cpu.getCPUInfo();
    const std::vector<int>& table = cpu.table();
    CHECK(table.size() == 1 && table[0] == 1800000);
}

// 3. 收窄后为空 → 退化为未收窄整表（§3.12 S3.1），而非空表
void testNarrowEmpty() {
    std::printf("3. 收窄后为空 → 退化整表（否则 setFreq 永远 3002）\n");
    resetCpu();
    CpuProbe cpu(kCpu, kProc, 5000000, 6000000);   // 上下限高于所有档位
    cpu.getCPUInfo();
    const std::vector<int>& table = cpu.table();
    CHECK(table.size() == 3);   // 退化：仍是完整三档
}

// 4. D-21：devfreq 原生 Hz，读入 ÷1000
void testNpuUnit() {
    std::printf("4. NPU 单位折算（Hz → kHz，D-21）\n");
    resetNpu();
    NpuProbe npu(kNpu, kNpu);   // statPath 显式给同目录：留空会退化为宏 NPU_STAT_PATH
    const NPUInfo info = npu.getNPUInfo();
    CHECK(npu.table().size() == 3);
    CHECK(npu.table()[0] == 800000 && npu.table()[2] == 1200000);
    CHECK(info.curFreqKHz == 1000000);
    CHECK(std::fabs(info.util - 0.42) < 1e-9);   // load = 42 → 0.42
}

// 5. 越界夹取：夹到收窄后的最大档
void testClampHigh() {
    std::printf("5. 越界夹取（目标远超上限）\n");
    resetCpu();
    CPU cpu(kCpu, kProc, 900000, 1900000);   // 收窄后表 = {1600000, 1800000}
    cpu.getCPUInfo();   // 常规路径是「先采集再调频」；setFreq 亦会懒加载，此处非必需
    Freq f;
    f.device = "cpu";
    f.freqKHz = 5000000;
    const ResultInfo r = cpu.setFreq(f);
    CHECK(r.ok);
    CHECK(get(kCpu + "/scaling_setspeed") == "1800000");   // 夹到收窄表内最近档，而非上限值 1900000
}

// 6. 并列夹取：距两档等距时取较小档（nearestFreqKHz 约定）
void testClampTie() {
    std::printf("6. 并列夹取（等距 → 取较小档）\n");
    resetNpu();
    NPU npu(kNpu, kNpu);   // 表 = {800000, 1000000, 1200000} kHz
    npu.getNPUInfo();
    Freq f;
    f.device = "npu";
    f.freqKHz = 900000;   // 距 800000 与 1000000 均为 100000
    const ResultInfo r = npu.setFreq(f);
    CHECK(r.ok);
    CHECK(get(kNpu + "/userspace/set_freq") == "800000000");   // D-21：×1000 后写出
}

// 7. governor：非 userspace 时自动切换
void testGovernor() {
    std::printf("7. governor 自动切换（非 userspace → 写 userspace）\n");
    resetCpu();
    put(kCpu + "/scaling_governor", "schedutil\n");
    CPU cpu(kCpu, kProc);
    cpu.getCPUInfo();
    Freq f;
    f.device = "cpu";
    f.freqKHz = 1800000;
    const ResultInfo r = cpu.setFreq(f);
    CHECK(r.ok);
    CHECK(get(kCpu + "/scaling_governor") == "userspace");
}

// 8. D-32：回读不作成败判据，curFreq 刷成**回读值**而非目标值
void testReadbackNotCompared() {
    std::printf("8. D-32 回读不比对（ok 只取写入结果，curFreq 刷成回读值）\n");
    resetCpu();
    put(kCpu + "/scaling_cur_freq", "1800000\n");   // 桩恒为 1800000，模拟驱动滞后
    CPU cpu(kCpu, kProc);
    cpu.getCPUInfo();
    Freq f;
    f.device = "cpu";
    f.freqKHz = 1600000;
    const ResultInfo r = cpu.setFreq(f);
    CHECK(r.ok);                                    // 写入成功 → ok，尽管回读仍是 1800000
    CHECK(get(kCpu + "/scaling_setspeed") == "1600000");
    CHECK(cpu.getCPUInfo().curFreqKHz == 1800000);  // 刷的是回读值，不是目标值
}

// 9. /proc/stat 两帧差分：首帧占位 0，第二帧为真值
void testProcStatDiff() {
    std::printf("9. /proc/stat 两帧差分（首帧占位 0）\n");
    resetCpu();
    resetNpu();
    resetProc("1000", "500", "8000", "100");   // total 9600, idle 8100

    systemManager mgr(kCpu, kNpu, kProc, kNpu);   // 第 4 参显式给：留空退化为宏 NPU_STAT_PATH
    mgr.refreshSystemInfo();
    CHECK(mgr.askSystemInfo().ucpu == 0.0);   // 首帧无基准 → 占位 0
    CHECK(std::fabs(mgr.askSystemInfo().unpu - 0.42) < 1e-9);   // load 单帧即得

    // total 9950（+350），idle 8250（+150）→ util = 1 - 150/350 ≈ 0.5714
    resetProc("1100", "600", "8100", "150");
    mgr.refreshSystemInfo();
    const double util = mgr.askSystemInfo().ucpu;
    CHECK(std::fabs(util - (1.0 - 150.0 / 350.0)) < 1e-9);
}

// 10. 节点缺失：不崩溃、零值兜底、setFreq 回 3002
//     —— 这里的 3002 是「懒加载也装不出表」的**真失败**，与用例 11 的假失败相对
void testMissingNodes() {
    std::printf("10. 节点缺失（零值兜底 + setFreq 回 3002）\n");
    const std::string bad = kRoot + "/nonexistent";
    CpuProbe cpu(bad, bad);
    const CPUInfo info = cpu.getCPUInfo();
    CHECK(cpu.table().empty() && info.curFreqKHz == 0 && info.util == 0.0);

    Freq f;
    f.device = "cpu";
    f.freqKHz = 1600000;
    const ResultInfo r = cpu.setFreq(f);
    CHECK(!r.ok && r.reason == Reason::freqUnavailable);

    NpuProbe npu(bad, bad);
    const NPUInfo ninfo = npu.getNPUInfo();
    CHECK(npu.table().empty() && ninfo.curFreqKHz == 0 && ninfo.util == 0.0);

    Freq fn;
    fn.device = "npu";
    fn.freqKHz = 1000000;
    const ResultInfo rn = npu.setFreq(fn);
    CHECK(!rn.ok && rn.reason == Reason::freqUnavailable);
}

// 11. 懒加载：从未采集过的设备上 setFreq → 自行装表并成功
//     （旧口径「setFreq 不自装表 → 3002」把「还没读过一次」误判成「设备不可用」，
//      服务线程先于监控线程到达时会假失败，故改为谁先到谁装表）
void testLazyTableLoad() {
    std::printf("11. 档位表懒加载（未经采集直接 setFreq → 自行装表并成功）\n");
    resetCpu();
    resetNpu();
    CpuProbe cpu(kCpu, kProc);
    NpuProbe npu(kNpu, kNpu);

    Freq fc;
    fc.device = "cpu";
    fc.freqKHz = 1800000;
    const ResultInfo rc = cpu.setFreq(fc);
    CHECK(rc.ok);                                          // 懒加载后表已装配，不再假 3002
    CHECK(get(kCpu + "/scaling_setspeed") == "1800000");   // 确实写了节点
    CHECK(cpu.table().size() == 3);                        // 装表副作用可被后续读表看到

    Freq fn;
    fn.device = "npu";
    fn.freqKHz = 1000000;
    const ResultInfo rn = npu.setFreq(fn);
    CHECK(rn.ok);
    CHECK(get(kNpu + "/userspace/set_freq") == "1000000000");   // D-21：×1000 后写出
    CHECK(npu.table().size() == 3);
}

// 12. 并发：监控线程刷、服务线程读与调频
void testConcurrent() {
    std::printf("12. 并发（采集 / 读缓存 / 调频 三线程）\n");
    resetCpu();
    resetNpu();
    resetProc("1000", "500", "8000", "100");

    systemManager mgr(kCpu, kNpu, kProc, kNpu);
    std::thread t1([&mgr] { for (int i = 0; i < 100; ++i) { mgr.refreshSystemInfo(); } });
    std::thread t2([&mgr] { for (int i = 0; i < 100; ++i) { (void)mgr.askSystemInfo(); } });
    std::thread t3([&mgr] {
        for (int i = 0; i < 100; ++i) {
            Freq f;
            f.device = (i % 2 == 0) ? "cpu" : "npu";
            f.freqKHz = 1600000 + (i % 3) * 100000;
            (void)mgr.setFreq(f);
        }
    });
    t1.join();
    t2.join();
    t3.join();
    CHECK(true);   // 不崩即通过
}

} // namespace

int main() {
    makeStubs();

    testParseTable();
    testNarrow();
    testNarrowEmpty();
    testNpuUnit();
    testClampHigh();
    testClampTie();
    testGovernor();
    testReadbackNotCompared();
    testProcStatDiff();
    testMissingNodes();
    testLazyTableLoad();
    testConcurrent();

    const int rc = test::report("system");
    fs::remove_all(kRoot);
    return rc;
}
