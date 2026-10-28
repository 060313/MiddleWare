// SPDX-License-Identifier: MIT
// NPUFile —— NPU 设备文件 · System 子系统
//
// 职责：实现 NPU 侧的设备节点访问与解析——devfreq 节点布局（available_frequencies /
//       cur_freq / userspace/set_freq）、Hz 与 kHz 的边界折算（D-21）、
//       governor 切 userspace，以及由 load 节点单帧得到利用率。
//       对上只暴露 kHz 语义与 [0,1] 利用率，不泄漏节点名与单位。
//

#ifndef SCHED_SYSTEM_NPUFILE_H
#define SCHED_SYSTEM_NPUFILE_H

#include <cstdint>
#include <string>
#include <vector>

#include "system/include/deviceFile.h"

namespace sched {

class NPUFile : deviceFile{

};

} // namespace sched

#endif // SCHED_SYSTEM_NPUFILE_H
