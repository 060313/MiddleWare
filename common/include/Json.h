// SPDX-License-Identifier: MIT
// Json —— 极简 JSON 值 / 解析器 / 序列化器（详细设计 §4.4 / §4.6 的实现层工具）
//
// 为什么手写、不引第三方：全项目只用到两处 JSON——「解析模型契约」与「组装四键外壳
// + 解析报文载荷」。引一个单头库（如 nlohmann/json，2 万余行）违背 §3.2「最小依赖」
// 原则，且评审时无法说清依赖边界。本实现只覆盖 RFC 8259 的严格子集。
//
// 故意收紧的边界（都是为了「坏输入不打死解析器」）：
//   - 最大嵌套 16 层：业务报文最多 3 层，契约最多 3 层；
//   - 最大文本 1 MiB：协议单帧上限 64 KiB（§4.6.1），契约远小于此；
//   - 拒绝 NaN / Infinity / 前导零 / 裸控制字符 / 未配对代理项——这些正是
//     Python `json.dumps(allow_nan=True)`、手工编辑会吐出来的「伪 JSON」；
//   - 允许文件头 UTF-8 BOM：Windows 记事本另存的契约会带；
//   - 数字一律按 double 收，整型语义由 asInt() 把关（9.0 视为 9）。
//
// 本文件不属于 §1 的 13 个设计类，属实现层工具（与 ConfigFile 等同层，不进类图）。

#ifndef SCHED_COMMON_JSON_H
#define SCHED_COMMON_JSON_H

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace sched {
namespace json {

class Value {
public:
    enum class Type { Null, Bool, Number, String, Array, Object };

    Value() = default;

    // ---- 构造（编码侧用）----
    static Value boolean(bool value);
    static Value number(double value);
    static Value string(const std::string& value);
    static Value array();
    static Value object();

    // ---- 类型判别 ----
    Type type() const { return type_; }
    bool isNull() const { return type_ == Type::Null; }
    bool isBool() const { return type_ == Type::Bool; }
    bool isNumber() const { return type_ == Type::Number; }
    bool isString() const { return type_ == Type::String; }
    bool isArray() const { return type_ == Type::Array; }
    bool isObject() const { return type_ == Type::Object; }

    // ---- 取值（类型不符一律 false，不做隐式转换）----
    bool asBool(bool& out) const;
    bool asNumber(double& out) const;
    // 仅接受「数值上等价于整数」的数（9 与 9.0 均可），且在 int 范围内
    bool asInt(int& out) const;
    bool asString(std::string& out) const;

    // ---- 对象 / 数组访问（只读）----
    // 非 Object 或键不存在 → nullptr；重复键取最后一次的值
    const Value* find(const std::string& key) const;
    const std::vector<Value>& elements() const { return array_; }
    // 遍历对象（仅 Object 有效，返回 (键, 值) 有序对）；非 Object 返回空
    const std::vector<std::pair<std::string, Value>>& objectItems() const { return object_; }
    std::size_t size() const;

    // ---- 构造（编码侧：Null 会自动升级为 Object / Array）----
    // 已存在同名键则覆盖；若当前不是 Object 且不是 Null，调用被忽略（防御误用）
    void set(const std::string& key, Value value);
    void push(Value value);

    // ---- 序列化（紧凑、无额外空白；非有限数输出 null）----
    std::string dump() const;

    // ---- 解析：失败时 out 不变，error 可选 ----
    static bool parse(const std::string& text, Value& out, std::string* error = nullptr);

private:
    void write(std::string& out) const;

    Type type_ = Type::Null;
    bool bool_ = false;
    double number_ = 0.0;
    std::string string_;
    std::vector<Value> array_;
    std::vector<std::pair<std::string, Value>> object_;   // 保序：契约字段顺序有意义
};

} // namespace json
} // namespace sched

#endif // SCHED_COMMON_JSON_H
