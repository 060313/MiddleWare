// SPDX-License-Identifier: MIT
// CPU 实现（详细设计 §3.12）
//
// 设计依据（正文不再重复引用）：
//   - D-17：节点用 cpufreq 语义（scaling_*），均在 policy 目录下，**无 userspace/ 子目录**；
//   - D-18：这些节点**原生即 kHz** —— 读入直解析、写出直写，**不做 ×1000**；
//   - D-32：setFreq 的 ok 只取写入结果，回读仅刷新 cpuInfo_.curFreqKHz，**不与目标值比对**。
//
// 并发：本对象跨线程共享（监控线程 getCPUInfo / 服务线程 setFreq），
//       共享状态一律在 cpuLock_ 下读写，**设备 I/O 一律在锁外**。

#include "system/include/CPU.h"

#include <algorithm>
#include <sstream>
#include <string>

#include "common/include/FileService.h"
#include "common/include/FreqUtil.h"

namespace sched {

    //初始化
    CPU::~CPU() = default;

    CPUInfo CPU::getCPUInfo() {
        getInfo();                              
        CPUInfo out;
        withCPULock([&](){ out = cpuInfo_; }); 
        return out;
    }

    CPUInfo& CPU::getInfo(){
        bool needRead = withCPULock([&](){return freqTable_.empty();});
        if (needRead) readFreqTable(1, [this](auto&& fn){ return withCPULock(fn); });

        uint64_t total = 0; uint64_t idle = 0;
        const bool statOk = parseProcStat(FileService::read(statPath_), total, idle);

        //计算利用率：差分基准的「读」与「写」必须在同一把锁内 ——
        // 否则两个线程会拿同一份基准重复差分（利用率偏低），且构成数据竞争
        if (statOk) {
            withCPULock([&](){
                if (hasPrevSample_ && total >= prevTotal_ && idle >= prevIdle_) {
                    const uint64_t dTotal = total - prevTotal_;
                    const uint64_t dIdle = idle - prevIdle_;
                    cpuInfo_.util = (dTotal > 0)
                                  ? (1.0 - static_cast<double>(dIdle) / dTotal)
                                  : 0.0;
                }
                prevTotal_ = total;
                prevIdle_ = idle;
                hasPrevSample_ = true;
            });
        }

        // 读当前频率（锁外 I/O，原生 kHz 不折算）；读失败保留旧值
        long long readKHz = 0;
        if (parseInt(FileService::read(freqPath_ + "/scaling_cur_freq"), readKHz)) {
            withCPULock([&](){cpuInfo_.curFreqKHz = static_cast<int>(readKHz);});
        }

        // 引用返回以支持协变（基类为 deviceInfo&）；调用方应立即拷贝，勿跨帧持有
        return cpuInfo_;

    }

    ResultInfo CPU::setFreq(const Freq& freq) {
        bool needRead = withCPULock([&](){return freqTable_.empty();});
        if (needRead) readFreqTable(1, [this](auto&& fn){ return withCPULock(fn); });

        std::vector<int> table = withCPULock([&](){return freqTable_;});
        if (table.empty())
            return ResultInfo{false, "freq table unavailable", Reason::freqUnavailable};

        int targetKHz = freq.freqKHz;
        if (!std::binary_search(table.begin(), table.end(), targetKHz)) {
            targetKHz = nearestFreqKHz(table, targetKHz);
        }

        // 非 userspace 下内核拒写 scaling_setspeed，硬写只会回读到旧值 —— 先切策略
        if (FileService::read(freqPath_ + "/scaling_governor").find("userspace") != 0) {
            if (!FileService::write(freqPath_ + "/scaling_governor", "userspace")) {
                return ResultInfo{false, "governor switch failed", Reason::writeFailed};
            }
        }

        bool writeOK=FileService::write(freqPath_ + "/scaling_setspeed", std::to_string(targetKHz));
        if (!writeOK) {
            return ResultInfo{false, "write freq failed", Reason::writeFailed};
        }

        // 回读刷新真实值（D-32：不与目标值比对、不作成败判据）；读失败保留写入前的值
        long long readKHz = 0;
        if (parseInt(FileService::read(freqPath_ + "/scaling_cur_freq"), readKHz)) {
            withCPULock([&](){cpuInfo_.curFreqKHz = static_cast<int>(readKHz);});
        }

        return ResultInfo{true, std::string(), Reason::none};
    }

} // namespace sched
