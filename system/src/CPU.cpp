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

//操作系统内的原始数据中不包含利用率，需要重新计算
//用于从操作系统文件中按格式提取并处理CPU参数
bool parseProcStat(const std::string& content, uint64_t& total, uint64_t& idle) {

    //提取文件第一行
    const std::size_t eol = content.find('\n');
    const std::string line = content.substr(0, eol);
    if (line.size() < 4 || line.compare(0, 4, "cpu ") != 0) {
        return false;
    }

    //格式转换
    std::istringstream iss(line);
    std::string label;
    iss >> label;
    uint64_t fields[10] = {0};
    int n = 0;
    while (n < 10 && (iss >> fields[n])) {
        ++n;
    }
    if (n < 4) {
        return false;
    }

    //数据计算
    total = 0;
    for (int i = 0; i < n && i < 8; ++i) {
        total += fields[i];
    }
    idle = fields[3] + ((n > 4) ? fields[4] : 0);
    return true;
}

//初始化
CPU::CPU(const std::string& freqPath, const std::string& statPath, int freqMin, int freqMax)
    : freqPath_(freqPath),statPath_(statPath),freqMin_(freqMin), freqMax_(freqMax) {}

CPU::~CPU() = default;

// 处理器只接受档位表内频率
// 读档位表：解析 → 去重升序 → 收窄 → 持锁替换 freqTable_（唯一写入点）
bool CPU::readFreqTable() {
    std::vector<int> table = parseFreqTable(FileService::read(freqPath_ + "/scaling_available_frequencies"), 1);
    if (table.empty()) return false;

    std::vector<int> narrowed;   // 收窄：0 表示该侧不限
    for (const int f : table) {
        if ((freqMin_ > 0 && f < freqMin_) || (freqMax_ > 0 && f > freqMax_)) {
            continue;
        }
        narrowed.push_back(f);
    }

    // 收窄后为空 → 退化为未收窄的整表（§3.12 S3.1），不计为配置错误
    const bool empty = narrowed.empty();
    withCPULock([&](){freqTable_ = empty ? std::move(table) : std::move(narrowed);});
    return !empty;
}

bool CPU::writeFreq(int freqKHz) {
    std::vector<int> table;
    withCPULock([&](){table=freqTable_;});
    if (!std::binary_search(table.begin(), table.end(), freqKHz)) return false;
    return FileService::write(freqPath_ + "/scaling_setspeed", std::to_string(freqKHz));
}


CPUInfo CPU::getCPUInfo() {
    bool needRead = withCPULock([&](){return freqTable_.empty();});
    if (needRead) readFreqTable();

    uint64_t total = 0; uint64_t idle = 0;
    const bool statOk = parseProcStat(FileService::read(statPath_), total, idle);

    //计算利用率
    double util = 0.0;
    bool utilOK=false;
    if (statOk) {
        if (hasPrevSample_ && total >= prevTotal_ && idle >= prevIdle_) {
            const uint64_t dTotal = total - prevTotal_;
            const uint64_t dIdle = idle - prevIdle_;
            util = (dTotal > 0) ? (1.0 - static_cast<double>(dIdle) / dTotal) : 0.0;
            utilOK=true;
        }
        withCPULock([&](){
            prevTotal_ = total;
            prevIdle_ = idle;
            hasPrevSample_ = true;
        });
        
    }    
    withCPULock([&](){if(utilOK) cpuInfo_.util=util;});

    // 读当前频率（锁外 I/O，原生 kHz 不折算）；读失败保留旧值
    long long readKHz = 0;
    if (parseInt(FileService::read(freqPath_ + "/scaling_cur_freq"), readKHz)) {
        withCPULock([&](){cpuInfo_.curFreqKHz = static_cast<int>(readKHz);});
    }

    return withCPULock([&](){return cpuInfo_;});

}

ResultInfo CPU::setFreq(const Freq& freq) {
    bool needRead = withCPULock([&](){return freqTable_.empty();});
    if (needRead) readFreqTable();

    std::vector<int> table = withCPULock([&](){return freqTable_;});
    if (table.empty())
        return ResultInfo{false, "freq table unavailable", false, Reason::freqUnavailable};

    int targetKHz = freq.freqKHz;
    bool adjusted = false;
    if (!std::binary_search(table.begin(), table.end(), targetKHz)) {
        targetKHz = nearestFreqKHz(table, targetKHz);
        adjusted = true;
    }

    // 非 userspace 下内核拒写 scaling_setspeed，硬写只会回读到旧值 —— 先切策略
    if (FileService::read(freqPath_ + "/scaling_governor").find("userspace") != 0) {
        if (!FileService::write(freqPath_ + "/scaling_governor", "userspace")) {
            return ResultInfo{false, "governor switch failed", adjusted, Reason::writeFailed};
        }
    }

    if (!writeFreq(targetKHz)) {
        return ResultInfo{false, "write freq failed", adjusted, Reason::writeFailed};
    }

    // 回读刷新真实值（D-32：不与目标值比对、不作成败判据）；读失败保留写入前的值
    long long readKHz = 0;
    if (parseInt(FileService::read(freqPath_ + "/scaling_cur_freq"), readKHz)) {
        withCPULock([&](){cpuInfo_.curFreqKHz = static_cast<int>(readKHz);});
    }

    return ResultInfo{true, std::string(), adjusted, Reason::none};
}

} // namespace sched
