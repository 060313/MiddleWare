// SPDX-License-Identifier: MIT
// deviceFile 实现 —— 只留构造；读表模板已移入头文件（模板定义须在使用点可见）

#include "system/include/deviceFile.h"

#include <utility>

namespace sched{

deviceFile::deviceFile(std::string freqPath, std::string statPath, int freqMin, int freqMax)
    : freqPath_(std::move(freqPath)),
      statPath_(std::move(statPath)),
      freqMin_(freqMin),
      freqMax_(freqMax) {}

} // namespace sched
