// SPDX-License-Identifier: MIT
// CPUFile —— CPU 设备文件 · System 子系统
//
// 职责：实现 CPU 侧的设备节点访问与解析——cpufreq 节点布局（scaling_*）、
//       档位表与当前频率的 kHz 直读直写（D-18）、governor 切 userspace、
//       以及由 /proc/stat 两帧差分得到利用率。
//       对上只暴露 kHz 语义与 [0,1] 利用率，不泄漏节点名与单位。
//
// 当前状态：仅有预处理部分，类内细节留白，接口与成员待补。

#ifndef SCHED_SYSTEM_CPUFILE_H
#define SCHED_SYSTEM_CPUFILE_H

#include <cstdint>
#include <string>
#include <vector>

#include "system/include/deviceFile.h"

namespace sched {

class CPUFile : deviceFile{
    public:
    private:
};

} // namespace sched

#endif // SCHED_SYSTEM_CPUFILE_H
