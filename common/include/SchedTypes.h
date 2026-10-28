// SPDX-License-Identifier: MIT
// SchedTypes —— 跨类共享的数据类型（设计层数据类型，非设计类）
//
// 依据：详细设计 §3.9（ActionInfo / ResultInfo / Reason）、§3.11（SystemInfo）、
//       §3.6 末（SystemStateSnapshot）、§4.4（Contract）、D-13 / D-14 / D-15 / D-33。
//
// 设计原则：
//   - 只放「跨类传递」的纯数据类型，不放业务逻辑；
//   - 不放任何设计层属性（属性一律落在各自的类里）；
//   - 一律值语义（可复制、可按值传返），故 §2 / §3 的「按值返回」写法成立；
//   - 唯一例外是文件末尾两个极小的构造辅助函数（不持有状态）。

#ifndef SCHED_COMMON_SCHEDTYPES_H
#define SCHED_COMMON_SCHEDTYPES_H

#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace sched {

// ---------------------------------------------------------------------------
// 失败原因（D-33）：**程序判定依据**，由最靠近失败点的类填写；
// serviceInterface::handleFrame 按唯一映射表转成协议码（§4.6.3），
// 绝不靠 message 文本推断。
// ---------------------------------------------------------------------------
enum class Reason {
    none = 0,          // 成功                          -> 0
    serviceNotRunning, // 服务未启动                     -> 1005
    schedulerAbsent,   // 调度链未装配                   -> 1006
    modelNotReady,     // 推理停用 / 会话未就绪           -> 2001
    inferenceFailed,   // 推理失败（空结果 / 超时取消）   -> 2002
    deviceUnknown,     // 设备标识非法或未创建            -> 3001
    freqUnavailable,   // 档位表不可用                   -> 3002
    writeFailed        // 写频率节点失败（含 governor）    -> 3003
};

// ---------------------------------------------------------------------------
// 单设备动作结果（§3.9 字段定义）
// ---------------------------------------------------------------------------
struct ResultInfo {
    bool ok = false;                       // 本次操作是否成功
    std::string message;                   // 人读文本，**不参与程序判定**
    bool adjusted = false;                 // 目标频率被夹取到最近合法档位（不判失败）
    Reason reason = Reason::none;          // 失败类别；ok == true 时恒为 none
};

// ---------------------------------------------------------------------------
// 设计层动作类型（D-14）：目标设备标识 + 目标频率（kHz）
//
// 一次调度决策**只在其中一台处理器上设一次频率**（D-13），故本类型是**一元**的。
// device 取 "cpu" / "npu"；空串表示「未选中任何处理器」（零值）。
// 频率单位**全链路 kHz**，换算只在设备边界发生（D-18 / D-21）。
// ---------------------------------------------------------------------------
struct Freq {
    std::string device;                    // "cpu" / "npu"；空串 = 未选中
    int freqKHz = 0;                       // 目标频率（kHz）
};

// ---------------------------------------------------------------------------
// 一次调度动作及其结果的载体（§3.9 字段定义，D-13：恒含「一条动作 + 一个结果」）
//
// ok / message / reason 是 result 对应字段的**只读镜像**，只为协议层直读方便；
// 一律经 makeActionInfo() / makeFailure() 构造，不要在别处分别赋值。
// ---------------------------------------------------------------------------
struct ActionInfo {
    Freq action;                           // 本次唯一动作；无动作时为零值
    ResultInfo result;                     // 本次动作的执行结果
    bool ok = false;                       // = result.ok
    std::string message;                   // = result.message
    Reason reason = Reason::none;          // = result.reason
};

// ---------------------------------------------------------------------------
// 任务请求：推理引擎经 UDS 提交的调度请求（§4.6.4 / D-28）
//
// **不是固定字段 struct**：task 是「命名特征表」，键集由契约 input_fields 中
// source == "task" 的项决定；值为**原始物理量**，归一化由 decisionMaker 完成。
// ---------------------------------------------------------------------------
struct TaskInfo {
    std::string requestId;                 // 请求标识，用于应答一一对应
    std::map<std::string, double> task;    // 命名特征表（键由契约决定）
};

// ---------------------------------------------------------------------------
// 设备级状态（§3.11 / D-15：CPUInfo 与 NPUInfo 并列，无继承、互不包含）
// 不设有效性字段：缓存中的值即视为有效，读失败按零值兜底。
// ---------------------------------------------------------------------------
struct deviceInfo{
    double util = 0.0;                     // 利用率 [0,1]（/proc/stat 两帧差分）
    int curFreqKHz = 0;                    // 当前频率（kHz）
};

struct CPUInfo :public deviceInfo{};
struct NPUInfo :public deviceInfo{};

// ---------------------------------------------------------------------------
// 系统级状态（D-15）：**具体结构体（值语义）**，聚合两份设备状态 + 四个特征分量
// 由 systemManager::refreshSystemInfo 组装、askSystemInfo 纯读缓存。
// ---------------------------------------------------------------------------
struct SystemInfo {
    double ucpu = 0.0;                     // 特征：CPU 利用率 [0,1]
    double unpu = 0.0;                     // 特征：NPU 利用率 [0,1]（单值，口径同 NPUInfo）
    int cpuFreqKHz = 0;                    // 特征：CPU 当前频率（kHz）
    int npuFreqKHz = 0;                    // 特征：NPU 当前频率（kHz）
};

// ---------------------------------------------------------------------------
// 对外应答载荷（§3.6 末）：GetState 的 data 位，**不属于系统状态类型**（D-15）
// **不设 ok**——成败由协议外层四键外壳表达，同一语义不在两层各存一份。
// ---------------------------------------------------------------------------
struct SystemStateSnapshot {
    SystemInfo system;                     // monitor->getSystemInfo() 的缓存副本
    std::string model;                     // 当前目标模型名；空串 = 推理已停用
};

// ---------------------------------------------------------------------------
// 决策产出（D-13）：承载**唯一**的 Freq；valid == false 表示未选中任何处理器
// （由 schedManager 映射为 2002，并计入连续推理失败）。
// ---------------------------------------------------------------------------
struct Decision {
    Freq action;                           // 唯一动作；无效时为零值
    bool valid = false;                    // 是否选中了处理器
};

// ---------------------------------------------------------------------------
// 模型契约（§4.4）：表格法——状态定义与动作定义都在契约里，.onnx 只承载网络参数
// ---------------------------------------------------------------------------
struct InputField {
    std::string name;                      // 分量名（task 的键名 / SystemInfo 的分量名）
    std::string source;                    // "task" / "system"
    double rawMin = 0.0;                   // 原始物理量程下界（归一化分母的唯一来源）
    double rawMax = 0.0;                   // 原始物理量程上界
    double min = 0.0;                      // 归一化**之后**的目标区间下界（只作仿射目标）
    double max = 1.0;                      // 归一化**之后**的目标区间上界
};

struct ActionSpec {
    std::string device;                    // "cpu" / "npu"
    int freqKHz = 0;                       // 目标频率（kHz，写绝对频率而非档位号）
};

struct Contract {
    int inputDim = 0;                      // 输入向量维度
    std::vector<InputField> inputFields;   // **数组顺序即模型输入位置，唯一真相源**
    int outputDim = 0;                     // 动作数
    std::vector<ActionSpec> actions;       // 动作编号 -> （处理器 + 目标频率）
};

// ---------------------------------------------------------------------------
// 构造辅助（不持有状态，只为保证 ActionInfo 的三个镜像字段不漂移）
// ---------------------------------------------------------------------------
inline ActionInfo makeActionInfo(const Freq& action, const ResultInfo& result) {
    ActionInfo info;
    info.action = action;
    info.result = result;
    info.ok = result.ok;
    info.message = result.message;
    info.reason = result.reason;
    return info;
}

// 无动作的失败返回（taskAsk 的 S2 / S3 / S4 失败出口都用它）
inline ActionInfo makeFailure(Reason reason, std::string message) {
    ResultInfo result;
    result.ok = false;
    result.adjusted = false;
    result.reason = reason;
    result.message = std::move(message);
    return makeActionInfo(Freq{}, result);
}

} // namespace sched

#endif // SCHED_COMMON_SCHEDTYPES_H
