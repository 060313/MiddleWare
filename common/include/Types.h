#pragma once

// 公共数据类型：贯穿 Service / Agent / System 三个子系统的数据契约。
// 对应《软件详细设计文档》6.3 与 7.3。

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace common {

// 调度模式（见详细设计 6.5 状态图）
enum class SchedMode {
    Auto,    // DQN 推理调度
    Static,  // 固定规则降级调度（DQN 不可用时的保底）
    Off      // 不推理，仅返回当前状态摘要
};

const char* toString(SchedMode mode);

// UDS 报文类型（见详细设计 5.2）
enum class MessageType : int {
    Unknown  = 0,
    AskSched = 1,   // 请求推理调度（UC1）
    GetState = 2,   // 查询系统状态（UC4）
    SetMode  = 3    // 切换调度模式（UC6）
};

// ---- 推理引擎提交的任务特征 ----
struct TaskFeature {
    uint64_t taskId = 0;
    int      batchSize = 1;      // 批大小
    int      seqLen = 0;         // 序列长度
    int      priority = 0;       // 优先级
    double   arrivalTsMs = 0.0;  // 到达时间
};

// ---- 系统状态量（设备参数换算后的决策输入）----
struct SystemFeature {
    float ucpu = 0.0f;            // CPU 利用率 [0,1]
    std::array<float, 3> unpu{};  // NPU 三核负载 [0,1]
    int   cpuFreqKHz = 0;
    int   npuFreqKHz = 0;
    bool  stale = false;          // 采集失败时为 true（沿用上一快照）
};

// ---- 单设备目标动作（意图）----
struct DeviceAction {
    std::string device;     // "cpu" | "npu"
    int freqKHz = 0;        // 目标频率
    int batchSize = 0;      // 目标批大小（0 表示不改）
};

// ---- 决策结果 ----
struct Decision {
    int    actionIndex = -1;
    double qValue = 0.0;
    std::vector<DeviceAction> actions;
};

// ---- 动作下发后的逐设备结果 ----
struct ResultInfo {
    std::string device;
    bool ok = false;
    bool adjusted = false;      // 频率越界被夹取到最近合法档位
    int  appliedFreqKHz = 0;
    std::string message;
};

// ---- 回传推理引擎的应答 ----
struct ActionInfo {
    uint64_t taskId = 0;
    bool ok = false;
    int  actionIndex = -1;
    std::vector<ResultInfo> results;
    std::string message;
};

}  // namespace common
