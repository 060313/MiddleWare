// SPDX-License-Identifier: MIT
// MessageCodec 实现（详细设计 §4.6 / D-13 / D-15 / D-27 / D-28 / D-33）

#include "service/include/MessageCodec.h"

#include <utility>

namespace sched {

namespace {

json::Value encodeCpuInfo(const CPUInfo& info) {
    json::Value out = json::Value::object();
    out.set("util", json::Value::number(info.util));
    out.set("cur_freq_khz", json::Value::number(info.curFreqKHz));
    json::Value table = json::Value::array();
    for (const int freq : info.freqTable) {
        table.push(json::Value::number(freq));
    }
    out.set("freq_table", std::move(table));
    return out;
}

json::Value encodeNpuInfo(const NPUInfo& info) {
    json::Value out = json::Value::object();
    out.set("util", json::Value::number(info.util));
    out.set("cur_freq_khz", json::Value::number(info.curFreqKHz));
    json::Value table = json::Value::array();
    for (const int freq : info.freqTable) {
        table.push(json::Value::number(freq));
    }
    out.set("freq_table", std::move(table));
    return out;
}

} // namespace

int reasonToCode(Reason reason) {
    switch (reason) {
    case Reason::none:              return 0;
    case Reason::serviceNotRunning: return 1005;
    case Reason::schedulerAbsent:   return 1006;
    case Reason::modelNotReady:     return 2001;
    case Reason::inferenceFailed:   return 2002;
    case Reason::deviceUnknown:     return 3001;
    case Reason::freqUnavailable:   return 3002;
    case Reason::writeFailed:       return 3003;
    }
    return 0;   // 不可达；兜底
}

std::string makeEnvelope(bool ok, int code, const std::string& message, const json::Value* data) {
    json::Value root = json::Value::object();
    root.set("ok", json::Value::boolean(ok));
    root.set("code", json::Value::number(code));
    root.set("message", json::Value::string(message));
    if (data != nullptr) {
        root.set("data", *data);
    } else {
        root.set("data", json::Value());   // null：失败时 data 恒为 null
    }
    return root.dump();
}

bool decodeTaskAsk(const std::string& payload, TaskInfo& out) {
    json::Value root;
    if (!json::Value::parse(payload, root) || !root.isObject()) {
        return false;
    }
    const json::Value* requestId = root.find("request_id");
    const json::Value* task = root.find("task");
    if (requestId == nullptr || task == nullptr || !task->isObject()) {
        return false;
    }

    std::string requestIdText;
    if (!requestId->asString(requestIdText) || requestIdText.empty()) {
        return false;
    }

    TaskInfo parsed;
    parsed.requestId = requestIdText;
    for (const auto& item : task->objectItems()) {
        double value = 0.0;
        if (!item.second.asNumber(value)) {
            return false;   // task 值必须为数值（D-28）
        }
        parsed.task[item.first] = value;
    }
    out = std::move(parsed);
    return true;
}

bool decodeSetModel(const std::string& payload, std::string& modelPath) {
    json::Value root;
    if (!json::Value::parse(payload, root) || !root.isObject()) {
        return false;   // 必须是 JSON 对象，不接受裸字符串（§4.6.4）
    }
    const json::Value* value = root.find("model_path");
    if (value == nullptr) {
        return false;
    }
    return value->asString(modelPath);   // 空串合法（= 停用推理）
}

json::Value encodeActionInfo(const ActionInfo& info) {
    json::Value out = json::Value::object();

    json::Value action = json::Value::object();
    action.set("device", json::Value::string(info.action.device));
    action.set("freq_khz", json::Value::number(info.action.freqKHz));
    out.set("action", std::move(action));

    json::Value result = json::Value::object();
    result.set("ok", json::Value::boolean(info.result.ok));
    result.set("adjusted", json::Value::boolean(info.result.adjusted));
    result.set("message", json::Value::string(info.result.message));
    out.set("result", std::move(result));

    return out;
}

json::Value encodeSnapshot(const SystemStateSnapshot& snapshot) {
    json::Value out = json::Value::object();

    json::Value system = json::Value::object();
    system.set("cpu", encodeCpuInfo(snapshot.system.cpuInfo));
    system.set("npu", encodeNpuInfo(snapshot.system.npuInfo));
    system.set("ucpu", json::Value::number(snapshot.system.ucpu));
    system.set("unpu", json::Value::number(snapshot.system.unpu));
    system.set("cpu_freq_khz", json::Value::number(snapshot.system.cpuFreqKHz));
    system.set("npu_freq_khz", json::Value::number(snapshot.system.npuFreqKHz));
    out.set("system", std::move(system));

    out.set("model", json::Value::string(snapshot.model));
    return out;
}

} // namespace sched
