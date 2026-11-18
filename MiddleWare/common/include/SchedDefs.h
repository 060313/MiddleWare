// SPDX-License-Identifier: MIT
// SchedDefs —— 全项目宏定义（公共头文件，全项目唯一出处）
//
// 依据：详细设计 §4.3 / §4.3.2 / §4.3.3 / D-22。
// 五个路径一律以 #define 给出——预处理期字面量替换，**不是变量、也不是常量对象**
// （不占存储、无地址、运行期不可改写）。
//
// 配置覆盖规则（D-22）：
//   - CONFIG_PATH 可被命令行 `--config <路径>` 覆盖，覆盖后不再使用宏；
//   - 其余四个路径由配置文件的对应键覆盖（键留空即取宏展开值）。

#ifndef SCHED_COMMON_SCHEDDEFS_H
#define SCHED_COMMON_SCHEDDEFS_H

// 模型与契约的统一目录（§4.3.2「目录固定、命名成对」）。
// **注意结尾带 '/'**：拼完整路径时直接 字符串 + 文件名，不要再补斜杠。
#define MODEL_DIR "/usr/lib/schedd/models/"

// CPU 频率设备节点（cpufreq policy 目录，D-17：scaling_* 命名，非 devfreq）
#define CPU_FREQ_PATH "/sys/devices/system/cpu/cpu0/cpufreq"

// NPU 频率设备节点（devfreq 挂点，D-21：节点原生单位 Hz）
#define NPU_FREQ_PATH "/sys/class/devfreq/fdab0000.npu"

// CPU 累计 jiffies 统计文件（利用率差分基准，§3.12 getCPUInfo S2）
#define PROC_STAT_PATH "/proc/stat"

// 配置文件路径（§4.3，命令行可覆盖）
#define CONFIG_PATH "/etc/schedd/schedd.conf"

#endif // SCHED_COMMON_SCHEDDEFS_H
