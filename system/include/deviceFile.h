// SPDX-License-Identifier: MIT
// deviceFile —— 设备文件封装 · System 子系统
//
// 职责：封装一台处理器的设备节点布局与解析过程——节点路径、单位倍数、
//       利用率来源均在其内部持有，对外只暴露 kHz 语义；是 CPU / NPU 之下
//       唯一直接接触设备文件的类。
//

#ifndef SCHED_SYSTEM_DEVICEFILE_H
#define SCHED_SYSTEM_DEVICEFILE_H

#include <string>

#include "common/include/SchedTypes.h"

namespace sched {

class deviceFile {
    public:
        deviceFile(std::string freqPath,std::string statPath,int freqMin,int freqmax);
        virtual ~deviceFile()=default;
    protected:
        virtual deviceInfo& getInfo()=0;
        virtual ResultInfo setFreq(const Freq& freq)=0;

        template<typename LockFn>
        bool readFreqTable(int divisor, LockFn &&withLock);
        std::vector<int> freqTable_; 
        std::string freqPath_;
        std::string statPath_;
        int freqMin_ = 0;               
        int freqMax_ = 0;
};

} // namespace sched

#endif // SCHED_SYSTEM_DEVICEFILE_H
