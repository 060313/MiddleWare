#pragma once

// 极简 JSON DOM（解析 + 序列化）。
// 见《软件详细设计文档》6.3.1。
//
// 为什么自带一个而不是引第三方库：本项目对 JSON 的全部需求只有两处 ——
//   ① 读 model_contract.json（FeatureEncoder / ActionDecoder 的契约来源）；
//   ② 拼装 UDS 报文载荷（RequestCodec 的 encodeActionInfo / encodeSystemFeature）。
// 两者都只用到"对象 / 数组 / 字符串 / 数字 / 布尔 / null"六种值，
// 引入 nlohmann/json 会把编译时间与依赖面拉大一个量级，收益不匹配。
//
// 设计边界：
//   * 只覆盖 RFC 8259 的常用子集，不含 NaN / Infinity 字面量；
//   * 全量解析（一次性建 DOM），不做流式 / SAX；
//   * 数字统一以 double 承载 —— 契约与报文中的数值量级远小于 2^53，精度足够；
//   * 解析失败不抛异常，通过返回 false 表达（调用方进入降级分支）。

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace common {

class Json {
public:
    enum class Type { Null, Bool, Number, String, Array, Object };

    Json() = default;                      // 默认构造 = null
    Json(bool value);
    Json(double value);
    Json(int value);
    Json(const char* value);
    Json(std::string value);

    // 空数组 / 空对象（不能靠默认构造得到，null 与空的语义不同）
    static Json array();
    static Json object();

    Type type() const { return m_type; }
    bool isNull() const { return m_type == Type::Null; }
    bool isBool() const { return m_type == Type::Bool; }
    bool isNumber() const { return m_type == Type::Number; }
    bool isString() const { return m_type == Type::String; }
    bool isArray() const { return m_type == Type::Array; }
    bool isObject() const { return m_type == Type::Object; }

    // ---- 取值：类型不匹配一律返回 defaultValue，不抛异常 ----
    bool        asBool(bool defaultValue = false) const;
    double      asNumber(double defaultValue = 0.0) const;
    int         asInt(int defaultValue = 0) const;
    std::string asString(const std::string& defaultValue = std::string()) const;

    // ---- 访问：键 / 下标缺失时返回静态 null 对象，调用方可继续链式取值 ----
    bool has(const std::string& key) const;
    const Json& operator[](const std::string& key) const;
    const Json& operator[](std::size_t index) const;

    // 对象 = 键数，数组 = 元素数；其余类型返回 0
    std::size_t size() const;

    // ---- 构造（序列化侧）：set 为插入或覆盖，push 为数组追加 ----
    void set(const std::string& key, Json value);
    void push(Json value);

    // 供遍历（保持插入顺序，便于 dump 结果稳定可比对）
    const std::vector<std::pair<std::string, Json>>& items() const { return m_object; }
    const std::vector<Json>&                         elements() const { return m_array; }

    // 紧凑序列化（无缩进）：报文载荷走 UDS，不追求可读排版
    std::string dump() const;

    // 解析；成功返回 true 并覆盖 out，失败返回 false 且不动 out
    static bool parse(const std::string& text, Json& out);

private:
    Type                                      m_type = Type::Null;
    bool                                      m_bool = false;
    double                                    m_number = 0.0;
    std::string                               m_string;
    std::vector<Json>                         m_array;
    std::vector<std::pair<std::string, Json>> m_object;
};

}  // namespace common
