// SPDX-License-Identifier: MIT
// MessageCodec —— 报文编解码工具（详细设计 §4.6 的实现层，非设计类）
//
// 把 §4.6 的「四键外壳 / 错误码映射 / 三类报文载荷编解码」抽成纯函数，供
// serviceInterface::handleFrame 调用。这样编解码不依赖对象图，可以独立单测
// （与 ConfigFile 等同层，不计入 §1 的 13 个设计类）。
//
// 字段命名冻结为 snake_case（freq_khz / input_dim / model_path），与训练侧契约一致。

#ifndef SCHED_SERVICE_MESSAGECODEC_H
#define SCHED_SERVICE_MESSAGECODEC_H

#include <string>

#include "common/include/Json.h"
#include "common/include/SchedTypes.h"

namespace sched {

// Reason → 协议码（D-33 的唯一映射点；§4.6.3）
int reasonToCode(Reason reason);

// 四键恒在外壳（§4.6.2）：ok/code/message/data 恒在；data 为 null 时输出 "data":null
std::string makeEnvelope(bool ok, int code, const std::string& message, const json::Value* data);

// TaskAsk 请求解码（§4.6.4 / D-28）：非对象 / 键缺失 / 类型不符 → false
bool decodeTaskAsk(const std::string& payload, TaskInfo& out);

// SetModel 请求解码：必须是 JSON 对象；model_path 允许空串（= 停用推理）
bool decodeSetModel(const std::string& payload, std::string& modelPath);

// 应答载荷编码：ActionInfo（TaskAsk 成功时的 data 位，一条动作 + 一个结果，D-13）
json::Value encodeActionInfo(const ActionInfo& info);

// 应答载荷编码：SystemStateSnapshot（GetState 成功时的 data 位，{system, model}，D-15）
json::Value encodeSnapshot(const SystemStateSnapshot& snapshot);

} // namespace sched

#endif // SCHED_SERVICE_MESSAGECODEC_H
