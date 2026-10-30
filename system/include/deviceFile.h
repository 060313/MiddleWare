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
#include <utility>
#include <vector>

#include "common/include/FileService.h"
#include "common/include/FreqUtil.h"
#include "common/include/SchedTypes.h"

namespace sched {

class deviceFile {
    public:
        deviceFile(std::string freqPath,std::string statPath,int freqMin,int freqMax);
        virtual ~deviceFile()=default;
    protected:
        virtual deviceInfo& getInfo()=0;
        virtual ResultInfo setFreq(const Freq& freq)=0;

        // 读档位表：freqNode 为相对 freqPath_ 的节点名（cpufreq 用 scaling_available_frequencies，
        // devfreq 用 available_frequencies）；divisor 为原生单位到 kHz 的除数；
        // withLock 为派生类提供的加锁手段
        // 模板定义必须在使用点可见，故置于头文件（放在 .cpp 里调用点看不到，链接期缺符号）
        template<typename LockFn>
        bool readFreqTable(int divisor,const std::string& freqNode, LockFn &&withLock){
            std::vector<int> table = parseFreqTable(FileService::read(freqPath_ +"/"+freqNode), divisor);
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
            withLock([&](){freqTable_ = empty ? std::move(table) : std::move(narrowed);});
            return !empty;
        }

        std::vector<int> freqTable_; 
        std::string freqPath_;
        std::string statPath_;
        int freqMin_ = 0;               
        int freqMax_ = 0;
};

} // namespace sched

#endif // SCHED_SYSTEM_DEVICEFILE_H
