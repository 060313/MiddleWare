#include "OsMonitor.h"

namespace syscore {

bool OsMonitor::readCpuStat(unsigned long long& total, unsigned long long& idle) {
    // TODO: 解析 /proc/stat 首行 "cpu  user nice system idle iowait irq softirq ..."
    //       idle = idle + iowait；total = 各列之和
    (void)total;
    (void)idle;
    return false;
}

std::optional<std::vector<int>> OsMonitor::readAvailableFreqs(const std::string& devfreqPath) {
    // TODO: 读 <devfreqPath>/available_frequencies，按空白切分并转 int
    (void)devfreqPath;
    return std::nullopt;
}

std::optional<int> OsMonitor::readCurrentFreq(const std::string& devfreqPath) {
    // TODO: 读 <devfreqPath>/cur_freq 转 int
    (void)devfreqPath;
    return std::nullopt;
}

}  // namespace syscore
