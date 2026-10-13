#pragma once

// 读取 procfs / sysfs 原始参数（子系统内部，类可见性 internal）。
// 见《软件详细设计文档》5.4。

#include <optional>
#include <string>
#include <vector>

namespace syscore {

class OsMonitor {
public:
    OsMonitor() = default;
    ~OsMonitor() = default;

    // /proc/stat 首行累计值（total、idle），单位 jiffies
    bool readCpuStat(unsigned long long& total, unsigned long long& idle);

    // devfreq 可用频率档位（kHz），来源 available_frequencies
    std::optional<std::vector<int>> readAvailableFreqs(const std::string& devfreqPath);

    // devfreq 当前频率（kHz），来源 cur_freq
    std::optional<int> readCurrentFreq(const std::string& devfreqPath);
};

}  // namespace syscore
