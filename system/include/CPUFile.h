// SPDX-License-Identifier: MIT
// CPUFile —— CPU 设备文件 · System 子系统
//
// 职责：实现 CPU 侧的设备节点访问与解析——cpufreq 节点布局（scaling_*）、
//       档位表与当前频率的 kHz 直读直写（D-18）、governor 切 userspace，
//       以及由 /proc/stat 两帧差分得到利用率。
//       对上只暴露 kHz 语义与 [0,1] 利用率，不泄漏节点名与单位。
//
// 当前状态：仅有预处理与类骨架，方法体留白待补。

#ifndef SCHED_SYSTEM_CPUFILE_H
#define SCHED_SYSTEM_CPUFILE_H

#include <cstdint>
#include <string>
#include <vector>

#include "common/include/SchedDefs.h"
#include "common/include/SchedTypes.h"
#include "system/include/deviceFile.h"

namespace sched {

class openEulerCPUFile : public deviceFile {
public:
    openEulerCPUFile(const std::string& freqPath = std::string(CPU_FREQ_PATH),
            const std::string& statPath = std::string(CPU_STAT_PATH),
            int freqMin = 0,
            int freqMax = 0);
    ~openEulerCPUFile();

protected:

    bool parseProcStat(const std::string& content, uint64_t& total, uint64_t& idle);

    uint64_t prevTotal_ = 0;      
    uint64_t prevIdle_ = 0;       
    bool hasPrevSample_ = false;  
};

} // namespace sched

#endif // SCHED_SYSTEM_CPUFILE_H
