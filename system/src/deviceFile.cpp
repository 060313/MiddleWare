#include "system/include/deviceFile.h"

#include <algorithm>
#include <sstream>
#include <string>

#include "common/include/FileService.h"
#include "common/include/FreqUtil.h"

namespace sched{
    
    deviceFile(std::string freqPath,std::string statPath,int freqMin,int freqmax)
                : freqPath_(freqPath){}


    template<typename LockFn>
    bool deviceFile::readFreqTable(int divisor, LockFn &&withLock){
        std::vector<int> table = parseFreqTable(FileService::read(freqPath_ + "/scaling_available_frequencies"), divisor);
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
}