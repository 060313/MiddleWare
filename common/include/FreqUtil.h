// SPDX-License-Identifier: MIT
// 频率档位工具 · common 子系统 · public（header-only，无对应 .cpp）
//
// 职责：设备节点文本解析与档位夹取的公共计算，供 CPU / NPU 共用。
//   二者在**折算到 kHz 之后**档位表语义完全相同——D-18（cpufreq 原生 kHz）与
//   D-21（devfreq 原生 Hz）的差别只在读写端口的单位折算，不在算法本身，
//   故算法只留一份，避免两处各维护一份、日后改动走偏（此前 CPU.cpp / NPU.cpp 各有一份副本）。
//
// 设计要点：
//   - 表必须**已升序**：夹取与 binary_search 都依赖有序，由 readFreqTable 的「去重升序」保证；
//   - **空表返回 0**：`table.front()` 在空 vector 上是未定义行为；调用方
//     （CPU/NPU::setFreq）已先判空返回 3002，此处只做兜底，**不改变判定口径**；
//   - header-only 且函数 inline：common/src 下无需新增 .cpp，module.mk 的通配不受影响。

#ifndef SCHED_COMMON_FREQUTIL_H
#define SCHED_COMMON_FREQUTIL_H

#include <algorithm>
#include <cstdlib>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace sched {

// 在**已升序**的档位表中找距 targetKHz 最近的合法档位。
// 并列时取先出现者（即较小值，因表已升序）。空表返回 0。
inline int nearestFreqKHz(const std::vector<int>& table, int targetKHz) {
    if (table.empty()) return 0;
    const auto r = std::equal_range(table.begin(), table.end(), targetKHz);
    if (r.first != r.second) return targetKHz;          
    if (r.first == table.begin()) return table.front(); 
    if (r.first == table.end())   return table.back();  
    const int hi = *r.first;                            
    const int lo = *(r.first - 1);                      
    return (targetKHz - lo <= hi - targetKHz) ? lo : hi;
}


// 节点文本转整数（形如 "1800000\n"）；空串或非数字返回 false。
// **必须 try-catch**：std::stoll 对空串抛 invalid_argument、对超范围抛 out_of_range。
inline bool parseInt(const std::string& text, long long& out) {
    if (text.empty()) {
        return false;
    }
    try {
        std::size_t pos = 0;
        const long long value = std::stoll(text, &pos);
        if (pos == 0) {
            return false;
        }
        out = value;
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

// 解析档位节点文本（空白 / 逗号分隔的整数列表）→ 每项 ÷ divisor 折成 kHz → 去重升序。
// divisor：cpufreq 传 1（原生 kHz，D-18），devfreq 传 1000（原生 Hz，D-21）。
// 返回空表 = 节点读不到或不含任何数字。
inline std::vector<int> parseFreqTable(const std::string& text, int divisor) {
    std::vector<int> out;
    if (text.empty() || divisor <= 0) {
        return out;
    }

    std::string buf = text;
    for (char& c : buf) {
        if (c == ',' || c == '\n' || c == '\r' || c == '\t') {
            c = ' ';
        }
    }

    std::istringstream iss(buf);
    long long value = 0;
    while (iss >> value) {
        const long long kHz = value / divisor;
        if (kHz > 0) {
            out.push_back(static_cast<int>(kHz));
        }
    }

    // 内核节点不保证有序，而夹取依赖有序
    std::sort(out.begin(), out.end());
    out.erase(std::unique(out.begin(), out.end()), out.end());
    return out;
}

} // namespace sched

#endif // SCHED_COMMON_FREQUTIL_H
