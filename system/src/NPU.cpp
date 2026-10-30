// SPDX-License-Identifier: MIT
// NPU 实现（详细设计 §3.13）
//
// 与 CPU 方法集合同构（D-19），差异只在两处：
//   ① 无 procfs：不读 /proc/stat、不持差分基准，利用率取 devfreq `load`（单帧即得）；
//   ② D-21：devfreq 节点**原生单位 Hz** —— 读入 ÷1000、写出 ×1000，
//      表 / 当前值 / 上下限的比较与存储一律在 kHz 上进行（漏折算会把整表剔空）。
//
// 并发：同 CPU —— 监控线程 getNPUInfo / 服务线程 setFreq，
//       共享状态一律在 npuLock_ 下读写，**设备 I/O 一律在锁外**。

#include "system/include/NPU.h"

#include <algorithm>
#include <string>

#include "common/include/FileService.h"
#include "common/include/FreqUtil.h"

namespace sched {
    NPU::~NPU() = default;

    NPUInfo NPU::getNPUInfo() {
        getInfo();                              
        NPUInfo out;
        withNPULock([&](){ out = npuInfo_; }); 
        return out;
    }

    NPUInfo& NPU::getInfo() {
        bool needRead = withNPULock([&](){return freqTable_.empty();});
        if (needRead) readFreqTable(1000,[this](auto&& fn){ return withNPULock(fn); });

        // 读当前频率（锁外 I/O，原生 Hz → ÷1000）；读失败保留旧值
        long long readHz = 0;
        if (parseInt(FileService::read(freqPath_ + "/cur_freq"), readHz))
            withNPULock([&](){npuInfo_.curFreqKHz = static_cast<int>(readHz / 1000);});

        // load 节点由驱动可选导出，读失败或不可解析 → util 保持 0（不中断采集）
        long long load = 0;
        double util = 0.0;
        bool utilOK = false;
        if (parseInt(FileService::read(statPath_ + "/load"), load)){
                util = static_cast<double>(load) / 100.0;
                if (util < 0.0) {
                    util = 0.0;
                }
                if (util > 1.0) {
                    util = 1.0;
                }
                utilOK = true;
        }
        withNPULock([&](){if(utilOK) npuInfo_.util=util;});
        return npuInfo_;
    }

    ResultInfo NPU::setFreq(const Freq& freq) {
        bool needRead = withNPULock([&](){return freqTable_.empty();});
        if (needRead) readFreqTable(1000,[this](auto&& fn){ return withNPULock(fn); });

        std::vector<int> table;
        withNPULock([&](){table = freqTable_;});
        if (table.empty()) {
            return ResultInfo{false, "freq table unavailable", Reason::freqUnavailable};
        }

        int targetKHz = freq.freqKHz;
        if (!std::binary_search(table.begin(), table.end(), targetKHz)) {
            targetKHz = nearestFreqKHz(table, targetKHz);
        }

        // 仅 userspace 允许写 userspace/set_freq —— 先切策略
        if (FileService::read(freqPath_ + "/governor").find("userspace") != 0) {
            if (!FileService::write(freqPath_ + "/governor", "userspace")) {
                return ResultInfo{false, "governor switch failed", Reason::writeFailed};
            }
        }

        bool writeOK=FileService::write(freqPath_ + "/userspace/set_freq",std::to_string(static_cast<long long>(targetKHz) * 1000));
        if (!writeOK) {
            return ResultInfo{false, "write freq failed", Reason::writeFailed};
        }

        // 回读刷新真实值（D-32：不与目标值比对、不作成败判据）；读失败保留写入前的值
        long long readHz = 0;
        if (parseInt(FileService::read(freqPath_ + "/cur_freq"), readHz))
            withNPULock([&](){npuInfo_.curFreqKHz = static_cast<int>(readHz / 1000);});

        return ResultInfo{true, std::string(), Reason::none};
    }
} // namespace sched
