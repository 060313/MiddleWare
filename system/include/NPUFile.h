// SPDX-License-Identifier: MIT
// NPUFile —— NPU 设备文件 · System 子系统
//
// 职责：实现 NPU 侧的设备节点访问与解析——devfreq 节点布局（available_frequencies /
//       cur_freq / userspace/set_freq）、档位表与当前频率的 Hz→kHz 折算（D-21）、
//       以及读 load 节点（0~100 → [0,1]）得到利用率。
//       对上只暴露 kHz 语义与 [0,1] 利用率，不泄漏节点名与单位。
//
// 与 CPUFile 的差异：无 procfs —— 不做两帧差分；利用率取 devfreq `load`，单帧即得。

#ifndef SCHED_SYSTEM_NPUFILE_H
#define SCHED_SYSTEM_NPUFILE_H

#include <string>

#include "system/include/deviceFile.h"

namespace sched {

class NPUFile : public deviceFile {
    public:
        // npuPath：devfreq 挂点；freqMin / freqMax：档位收窄（kHz，0 表不限）
        NPUFile(const std::string& freqPath,const std::string& statPath,int freqMin = 0,int freqMax = 0);
        ~NPUFile() override = default;
    protected:
        template<typename LockFn>
        bool readFreqTable(int divisor,LockFn &&withLock){
            return deviceFile::readFreqTable(divisor,freqNode,withLock);
        }
        std::string freqNode = "available_frequencies";
};

} // namespace sched

#endif // SCHED_SYSTEM_NPUFILE_H
