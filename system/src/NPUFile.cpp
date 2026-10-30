// SPDX-License-Identifier: MIT
// NPUFile 实现 —— 只留构造；读表模板在头文件（模板定义须在使用点可见），
// 本类只负责给出 devfreq 的节点名。

#include "system/include/NPUFile.h"

#include <string>

namespace sched {

    NPUFile::NPUFile(const std::string& freqPath, const std::string& statPath,
                    int freqMin, int freqMax)
        : deviceFile(freqPath, statPath, freqMin, freqMax) {}

} // namespace sched
