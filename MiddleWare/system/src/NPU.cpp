// SPDX-License-Identifier: MIT
// NPU 实现（详细设计 §3.13 / D-19 / D-20 / D-21 / D-32）
//
// 与 CPU 方法集合同构，差异只在两处：
//   ① 无 procfs——不读 /proc/stat、不持差分基准，利用率取 devfreq `load`（单帧即得）；
//   ② **节点原生单位 Hz**（D-21）：读入后 ÷1000、写出前 ×1000，
//      表 / 当前值 / 上下限的比较与存储一律在 kHz 上进行。
//
// 节点 I/O 部分留 TODO，纯计算部分（差分、夹取、去重升序、收窄、单位折算）按设计实现。

#include "system/include/NPU.h"

#include <algorithm>
#include <cstdlib>
#include <string>

#include "common/include/SchedDefs.h"

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

NPU::NPU(const std::string& devPath, int freqMin, int freqMax)
    : freqMin_(freqMin), freqMax_(freqMax) {
    // S1. 路径为空则退化为宏定义默认（测试可注入伪 sysfs）
    devPath_ = devPath.empty() ? std::string(NPU_FREQ_PATH) : devPath;

    // S2. freqMin / freqMax 均为 0 表示不限；二者矛盾时按下限优先（readFreqTable S3 兜底）
    // S3. freqTable 空、curFreq = 0；**不置差分基准**（NPU 无 procfs）
    // S4. **不在构造期读任何文件**，故构造不可失败
}

NPU::~NPU() = default;
// 平凡析构：devfreq 节点的生命期归属驱动而非本对象

NPUInfo NPU::getNPUInfo() {
    // S1. 确保频率表已读；失败不阻断
    if (freqTable_.empty()) {
        readFreqTable();
    }

    // S2. TODO(实现层)：读 devPath_ + "/cur_freq"，**÷1000 折算为 kHz** 后写入 curFreq_

    // S3. TODO(实现层)：读 devPath_ + "/load"（0~100）→ util = 读值 / 100，夹取到 [0,1]；
    //     读失败或不可解析 → util = 0（该节点由驱动可选导出，缺失不应中断采集）
    double util = 0.0;

    // S4. 组装返回
    NPUInfo info;
    info.util = util;
    info.curFreqKHz = curFreq_;
    info.freqTable = freqTable_;
    return info;
}

ResultInfo NPU::setFreq(const Freq& freq) {
    // S1. 可承载性判定：档位表为空 → 3002
    if (freqTable_.empty()) {
        return ResultInfo{false, "freq table unavailable", false, Reason::freqUnavailable};
    }

    // S1.2 不在表内 → 夹取到最近合法档位并置 adjusted
    int targetKHz = freq.freqKHz;
    bool adjusted = false;
    if (!std::binary_search(freqTable_.begin(), freqTable_.end(), targetKHz)) {
        targetKHz = nearestFreqKHz(freqTable_, targetKHz);
        adjusted = true;
    }

    // S2. TODO(实现层)：读 devPath_ + "/governor"，非 userspace 则写入 userspace；
    //     切换失败 → writeFailed("governor switch failed") 且不写频率

    // S3. 写频率节点（writeFreq 内部完成 kHz → Hz 折算）
    if (!writeFreq(targetKHz)) {
        return ResultInfo{false, "write freq failed", adjusted, Reason::writeFailed};
    }

    // S4. TODO(实现层)：回读 cur_freq 刷新 curFreq_（读失败保留写入前值）；
    //     **不与目标值比对、不作成败判据**（D-32）
    curFreq_ = targetKHz;

    return ResultInfo{true, std::string(), adjusted, Reason::none};
}

bool NPU::readFreqTable() {
    // S1. TODO(实现层)：经 FileService::read 读 devPath_ + "/available_frequencies"
    //     （单位 **Hz**）→ 每个值 **÷1000 折算为 kHz**
    // S2. 去重并按升序排列
    // S3. 档位收窄：剔出 < freqMin_ 与 > freqMax_ 的项（0 表示该侧不限）——
    //     **比较必须在 kHz 上进行**，否则 kHz 量级的上下限会把整表剔空
    // S3.1 收窄后为空 → 保留原表并返回 false（不计为配置错误）
    // S4. 收窄结果非空 → 写入 freqTable_，返回 true
    return false;
}

bool NPU::writeFreq(int freqKHz) {
    // S1. 前置：目标频率必须在表内
    if (!std::binary_search(freqTable_.begin(), freqTable_.end(), freqKHz)) {
        return false;
    }

    // S2. TODO(实现层)：经 FileService::write 写 devPath_ + "/userspace/set_freq"，
    //     写出前 **×1000 折算为 Hz**（D-21：漏折算会写入一个偏小 1000 倍的值）
    // S3. 返回写入结果
    return false;
}

} // namespace sched
