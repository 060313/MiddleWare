#include "system/include/CPUFile.h"

#include <algorithm>
#include <sstream>
#include <string>

#include "common/include/FileService.h"
#include "common/include/FreqUtil.h"

namespace sched{
    bool openEulerCPUFile::parseProcStat(const std::string& content, uint64_t& total, uint64_t& idle){
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
} // namespace sched
