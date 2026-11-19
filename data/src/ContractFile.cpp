// SPDX-License-Identifier: MIT
// ContractFile 实现（详细设计 §4.5.4 / §4.4 / D-25 / D-26）
//
// 解析口径（S3~S5）：
//   - 只判「格式 + 必需字段 + 字段类型」，**不做业务判断**——
//     device 不校验是否为 cpu/npu（非法标识留给 systemManager 回 3001，D-33），
//     动作频率也不与设备档位表比对（那是运行期 deviceUnknown / freqUnavailable 的事）；
//   - `input_fields` 的**数组顺序即模型输入位置**，解析时逐项保序写入 vector；
//   - 全部字段先落到临时 Contract，**校验全过才提交出参**，失败不污染调用方；
//   - 维度自洽（input_dim == 项数、output_dim == 动作数）不在此处判，
//     由 contractValidate 承担（三道校验的第 1、2 道，D-25）。
//
// 与训练侧的约定（契约冻结项）：字段名一律 snake_case（raw_min / freq_khz），
// C++ 侧字段为驼峰，映射在本文件内完成，不扩散到其他类。

#include "data/include/ContractFile.h"

#include <cmath>
#include <utility>

#include "common/include/FileService.h"
#include "common/include/Json.h"

namespace sched {

namespace {

// 取对象中的数值字段；缺键或类型不符均 false
bool numberField(const json::Value& object, const char* key, double& out) {
    const json::Value* value = object.find(key);
    if (value == nullptr) {
        return false;
    }
    return value->asNumber(out);
}

// 取对象中的字符串字段；缺键或类型不符均 false
bool stringField(const json::Value& object, const char* key, std::string& out) {
    const json::Value* value = object.find(key);
    if (value == nullptr) {
        return false;
    }
    return value->asString(out);
}

// input_fields 单项：name / source / raw_min / raw_max 必需，min / max 缺省 [0,1]
bool parseInputField(const json::Value& item, InputField& field) {
    if (!item.isObject()) {
        return false;
    }
    if (!stringField(item, "name", field.name) || field.name.empty()) {
        return false;
    }
    if (!stringField(item, "source", field.source)) {
        return false;
    }
    // source 是闭集：只有 task / system 两种取值（§4.4）
    if (field.source != "task" && field.source != "system") {
        return false;
    }
    if (!numberField(item, "raw_min", field.rawMin)) {
        return false;
    }
    if (!numberField(item, "raw_max", field.rawMax)) {
        return false;
    }
    // 量程写反几乎必是笔误：交给运行期会导致分母为负、归一化结果全错
    if (field.rawMin > field.rawMax) {
        return false;
    }

    // min / max 是归一化**之后**的目标区间，不承担标度作用（D-30）；缺省按 [0,1]
    field.min = 0.0;
    field.max = 1.0;
    const json::Value* lower = item.find("min");
    if (lower != nullptr && !lower->asNumber(field.min)) {
        return false;
    }
    const json::Value* upper = item.find("max");
    if (upper != nullptr && !upper->asNumber(field.max)) {
        return false;
    }
    if (field.min > field.max) {
        return false;
    }
    return true;
}

// actions 单项：{"device":"cpu","freq_khz":408000}
bool parseAction(const json::Value& item, ActionSpec& action) {
    if (!item.isObject()) {
        return false;
    }
    if (!stringField(item, "device", action.device) || action.device.empty()) {
        return false;
    }
    double freqKHz = 0.0;
    if (!numberField(item, "freq_khz", freqKHz)) {
        return false;
    }
    // 频率必须非负、且能装进 int（kHz；RK3588 上限 2.256 GHz，远在范围内）
    if (freqKHz < 0.0 || freqKHz > 2147483647.0) {
        return false;
    }
    action.freqKHz = static_cast<int>(std::llround(freqKHz));
    return true;
}

} // namespace

bool ContractFile::loadContract(const std::string& contractPath, Contract& out) {
    // S1. 路径非空
    if (contractPath.empty()) {
        return false;
    }

    // S2. 读全文；读失败与空文件同判（都没有可解析的内容）
    const std::string text = FileService::read(contractPath);
    if (text.empty()) {
        return false;
    }

    // S3. 解析 JSON；格式非法 → false（错误文本此处不落日志，交由调用方按 ready 判）
    json::Value root;
    if (!json::Value::parse(text, root)) {
        return false;
    }
    if (!root.isObject()) {
        return false;
    }

    // S4. 四个必需顶层字段必须齐全且类型正确
    const json::Value* inputDim = root.find("input_dim");
    const json::Value* inputFields = root.find("input_fields");
    const json::Value* outputDim = root.find("output_dim");
    const json::Value* actions = root.find("actions");
    if (inputDim == nullptr || inputFields == nullptr ||
        outputDim == nullptr || actions == nullptr) {
        return false;
    }

    Contract parsed;
    if (!inputDim->asInt(parsed.inputDim) || parsed.inputDim < 0) {
        return false;
    }
    if (!outputDim->asInt(parsed.outputDim) || parsed.outputDim < 0) {
        return false;
    }
    if (!inputFields->isArray() || !actions->isArray()) {
        return false;
    }

    parsed.inputFields.reserve(inputFields->size());
    for (const json::Value& item : inputFields->elements()) {
        InputField field;
        if (!parseInputField(item, field)) {
            return false;
        }
        parsed.inputFields.push_back(std::move(field));
    }

    parsed.actions.reserve(actions->size());
    for (const json::Value& item : actions->elements()) {
        ActionSpec action;
        if (!parseAction(item, action)) {
            return false;
        }
        parsed.actions.push_back(std::move(action));
    }

    // S5. 全部通过才提交（调用方持临时副本，失败时其内容不受影响）
    out = std::move(parsed);
    return true;
}

bool ContractFile::contractValidate(const std::string& contractPath) {
    // S1. 路径非空
    if (contractPath.empty()) {
        return false;
    }

    // S2. 解析到临时副本（不污染任何已生效内容）
    Contract tmp;
    if (!loadContract(contractPath, tmp)) {
        return false;
    }

    // S3.1 input_dim 与 input_fields 项数一致
    if (tmp.inputDim != static_cast<int>(tmp.inputFields.size())) {
        return false;
    }

    // S3.2 output_dim 与 actions 条数一致
    if (tmp.outputDim != static_cast<int>(tmp.actions.size())) {
        return false;
    }

    // S4. 全部一致
    return true;
}

} // namespace sched
