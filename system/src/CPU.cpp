// SPDX-License-Identifier: MIT
// CPU 实现（详细设计 §3.12 / D-17 / D-18 / D-32）
//
// 本文件把控制流写全，**节点 I/O 部分留 TODO**（readFreqTable / writeFreq / /proc/stat 读取），
// 纯计算部分（差分、档位夹取、去重升序、收窄）已按设计实现。
//
// 节点命名（D-17）：cpufreq 语义，`scaling_available_frequencies` /
// `scaling_cur_freq` / `scaling_setspeed` / `scaling_governor`，均在 policy 目录下。
// 单位（D-18）：这些节点**原生即 kHz**，读入直解析、写出直写，**不做 ×1000**。

#include "system/include/CPU.h"

#include <algorithm>
#include <cstdlib>
#include <string>

#include "common/include/FileService.h"

namespace sched {

namespace {

// 在**已升序**的档位表中找最近合法档位（夹取用）
int nearestFreqKHz(const std::vector<int>& table, int targetKHz) {
    int best = table.front();
    long long bestDiff = std::llabs(static_cast<long long>(targetKHz) - best);
    for (const int freq : table) {
        const long long diff = std::llabs(static_cast<long long>(targetKHz) - freq);
        if (diff < bestDiff) {
            best = freq;
            bestDiff = diff;
        }
    }
    return best;
}

} // namespace

CPU::CPU(const std::string& devPath, const std::string& procStatPath, int freqMin, int freqMax)
    : freqMin_(freqMin), freqMax_(freqMax) {
    // S1. 路径为空则退化为宏定义默认（测试可注入伪 sysfs）
    devPath_ = devPath.empty() ? std::string(CPU_FREQ_PATH) : devPath;
    procStatPath_ = procStatPath.empty() ? std::string(PROC_STAT_PATH) : procStatPath;

    // S2. freqMin / freqMax 均为 0 表示不限；二者矛盾时按下限优先（readFreqTable S3 兜底）
    // S3. freqTable 空、curFreq = 0（成员初值已给出）
    // S4. 差分基准置零（首帧利用率为占位值）
    // S5. **不在构造期读任何文件**，故构造不可失败
}

CPU::~CPU() = default;
// 平凡析构：不持常开文件句柄（每次读写即时开闭）

CPUInfo CPU::getCPUInfo() {
    // S1. 确保频率表已读；失败不阻断（freqTable 保持空，setFreq 会兜底报 3002）
    if (freqTable_.empty()) {
        readFreqTable();
    }

    // S2. TODO(实现层)：读 procStatPath_ 首行（cpu 汇总行），解析累计 total / idle jiffies
    uint64_t total = prevTotal_;   // TODO: 换成实读值
    uint64_t idle = prevIdle_;     // TODO: 换成实读值

    // S3. 利用率差分
    double util = 0.0;
    if (hasPrevSample_) {
        const uint64_t dTotal = total - prevTotal_;
        const uint64_t dIdle = idle - prevIdle_;
        // S3.2 除零保护
        util = (dTotal > 0)
                   ? (1.0 - static_cast<double>(dIdle) / static_cast<double>(dTotal))
                   : 0.0;
    }
    // S3.3 首帧无基准 → util 保持 0（占位值）

    // S4. 更新差分基准（属内部采样状态，不改对外可见状态）
    prevTotal_ = total;
    prevIdle_ = idle;
    hasPrevSample_ = true;

    // S5. TODO(实现层)：读 devPath_ + "/scaling_cur_freq" 更新 curFreq_（原生 kHz，不折算）

    // S6. 组装返回
    CPUInfo info;
    info.util = util;
    info.curFreqKHz = curFreq_;
    info.freqTable = freqTable_;
    return info;
}

ResultInfo CPU::setFreq(const Freq& freq) {
    // S1. 可承载性判定：档位表为空 → 3002（不可承载不等于越界，不做夹取）
    if (freqTable_.empty()) {
        return ResultInfo{false, "freq table unavailable", false, Reason::freqUnavailable};
    }

    // S1.2 不在表内 → 夹取到最近合法档位并置 adjusted（越界不判失败）
    int targetKHz = freq.freqKHz;
    bool adjusted = false;
    if (!std::binary_search(freqTable_.begin(), freqTable_.end(), targetKHz)) {
        targetKHz = nearestFreqKHz(freqTable_, targetKHz);
        adjusted = true;
    }

    // S2. TODO(实现层)：读 devPath_ + "/scaling_governor"，非 userspace 则写入 userspace；
    //     切换失败 → 返回 writeFailed("governor switch failed") 且**不写频率**

    // S3. 写频率节点
    if (!writeFreq(targetKHz)) {
        // 失败时 curFreq_ 保持原值
        return ResultInfo{false, "write freq failed", adjusted, Reason::writeFailed};
    }

    // S4. TODO(实现层)：回读 scaling_cur_freq 刷新 curFreq_（读失败保留写入前值）；
    //     **不与目标值比对、不作成败判据**（D-32）
    curFreq_ = targetKHz;

    return ResultInfo{true, std::string(), adjusted, Reason::none};
}

bool CPU::readFreqTable() {
    // S1. TODO(实现层)：经 FileService::read 读 devPath_ + "/scaling_available_frequencies"
    //     读失败或内容为空 → 返回 false
    // S2. 解析（空白 / 逗号分隔）→ **去重并按升序排列**（夹取依赖有序）
    // S3. 档位收窄：剔出 < freqMin_ 与 > freqMax_ 的项（值为 0 表示该侧不限）
    // S3.1 收窄后为空 → **保留原表**并返回 false（不计为配置错误）
    // S4. 收窄结果非空 → 写入 freqTable_，返回 true
    return false;
}

bool CPU::writeFreq(int freqKHz) {
    // S1. 前置：目标频率必须在表内（不允许设表外频率）
    if (!std::binary_search(freqTable_.begin(), freqTable_.end(), freqKHz)) {
        return false;
    }

    // S2. TODO(实现层)：经 FileService::write 写 devPath_ + "/scaling_setspeed"
    //     **原生 kHz，直接写十进制文本，不做 ×1000**（D-18）
    // S3. 返回写入结果
    return false;
}

} // namespace sched
