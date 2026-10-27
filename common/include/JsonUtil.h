// SPDX-License-Identifier: MIT
// JsonUtil —— 极简 JSON 解析与序列化（header-only，零第三方依赖）
//
// 设计依据：
//   - 契约文本（§4.4）与协议载荷（§4.6）都是 JSON，而项目不允许引入第三方库，
//     故自带一个够用的实现：只支持 对象 / 数组 / 字符串 / 数字 / 布尔 / null 六类；
//   - **解析全程不抛异常**，格式非法一律返回 false —— 失败码归调用方各自映射
//     （契约侧记为解析失败，协议侧记 1004），本类不参与业务判定；
//   - 数字统一存 double：契约的 raw_min / raw_max 既有整数也有小数，需要整数的
//     调用方自行取整（ConfigFile 那类严格整数判据不属本类职责）；
//   - 序列化按 std::map 的键序输出，应答字节可预测，便于对拍与桩测。

#ifndef SCHED_COMMON_JSONUTIL_H
#define SCHED_COMMON_JSONUTIL_H

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace sched {
namespace json {

class Value {
public:
    enum class Type { Null, Bool, Number, String, Array, Object };

    using Object = std::map<std::string, Value>;
    using Array = std::vector<Value>;

    Value() = default;

    static Value null() { return Value(); }
    static Value boolean(bool v) {
        Value out;
        out.type_ = Type::Bool;
        out.bool_ = v;
        return out;
    }
    static Value number(double v) {
        Value out;
        out.type_ = Type::Number;
        out.number_ = v;
        return out;
    }
    static Value string(const std::string& v) {
        Value out;
        out.type_ = Type::String;
        out.string_ = v;
        return out;
    }
    static Value array() {
        Value out;
        out.type_ = Type::Array;
        return out;
    }
    static Value object() {
        Value out;
        out.type_ = Type::Object;
        return out;
    }

    Type type() const { return type_; }
    bool isNull() const { return type_ == Type::Null; }
    bool isBool() const { return type_ == Type::Bool; }
    bool isNumber() const { return type_ == Type::Number; }
    bool isString() const { return type_ == Type::String; }
    bool isArray() const { return type_ == Type::Array; }
    bool isObject() const { return type_ == Type::Object; }

    // 取值：类型不符返回各自的零值（不判失败，判类型由 isXxx 先行完成）
    bool asBool() const { return bool_; }
    double asNumber() const { return number_; }
    const std::string& asString() const { return string_; }
    const Array& asArray() const { return array_; }
    const Object& asObject() const { return object_; }

    // 对象键访问；缺失或不是对象均返回 nullptr
    const Value* find(const std::string& key) const {
        if (type_ != Type::Object) {
            return nullptr;
        }
        const Object::const_iterator it = object_.find(key);
        return (it == object_.end()) ? nullptr : &(it->second);
    }

    void set(const std::string& key, const Value& value) {
        type_ = Type::Object;
        object_[key] = value;
    }

    void push(const Value& value) {
        type_ = Type::Array;
        array_.push_back(value);
    }

private:
    Type type_ = Type::Null;
    bool bool_ = false;
    double number_ = 0.0;
    std::string string_;
    Array array_;
    Object object_;
};

// 序列化：整数值的数字按整数形态输出（0 而非 0.0），其余走 %.17g
inline std::string stringify(const Value& value);

namespace detail {

inline void escapeString(const std::string& in, std::string& out) {
    out.push_back('"');
    for (const char ch : in) {
        switch (ch) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (static_cast<unsigned char>(ch) < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned char>(ch));
                    out += buf;
                } else {
                    out.push_back(ch);
                }
        }
    }
    out.push_back('"');
}

inline void stringifyNumber(double v, std::string& out) {
    if (std::isfinite(v) && v == std::floor(v) && std::fabs(v) < 1e15) {
        out += std::to_string(static_cast<long long>(v));
        return;
    }
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.17g", v);
    out += buf;
}

} // namespace detail

inline std::string stringify(const Value& value) {
    std::string out;
    switch (value.type()) {
        case Value::Type::Null:
            out += "null";
            break;
        case Value::Type::Bool:
            out += value.asBool() ? "true" : "false";
            break;
        case Value::Type::Number:
            detail::stringifyNumber(value.asNumber(), out);
            break;
        case Value::Type::String:
            detail::escapeString(value.asString(), out);
            break;
        case Value::Type::Array: {
            out.push_back('[');
            const Value::Array& items = value.asArray();
            for (std::size_t i = 0; i < items.size(); ++i) {
                if (i != 0) {
                    out.push_back(',');
                }
                out += stringify(items[i]);
            }
            out.push_back(']');
            break;
        }
        case Value::Type::Object: {
            out.push_back('{');
            const Value::Object& items = value.asObject();
            bool first = true;
            for (const Value::Object::value_type& item : items) {
                if (!first) {
                    out.push_back(',');
                }
                first = false;
                detail::escapeString(item.first, out);
                out.push_back(':');
                out += stringify(item.second);
            }
            out.push_back('}');
            break;
        }
    }
    return out;
}

// 递归下降解析；格式非法返回 false（出参不保证内容）
class Parser {
public:
    explicit Parser(const std::string& text) : text_(text) {}

    bool parse(Value& out) {
        skipWs();
        if (!parseValue(out, 0)) {
            return false;
        }
        skipWs();
        return pos_ == text_.size();   // 尾部有多余内容即非法
    }

private:
    static constexpr int kMaxDepth = 64;   // 防恶意嵌套把栈打爆

    void skipWs() {
        while (pos_ < text_.size()) {
            const char ch = text_[pos_];
            if (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r') {
                ++pos_;
            } else {
                break;
            }
        }
    }

    bool literal(const char* word) {
        const std::size_t n = std::strlen(word);
        if (text_.compare(pos_, n, word) != 0) {
            return false;
        }
        pos_ += n;
        return true;
    }

    bool parseValue(Value& out, int depth) {
        if (depth > kMaxDepth || pos_ >= text_.size()) {
            return false;
        }
        switch (text_[pos_]) {
            case '{': return parseObject(out, depth);
            case '[': return parseArray(out, depth);
            case '"': {
                std::string s;
                if (!parseString(s)) {
                    return false;
                }
                out = Value::string(s);
                return true;
            }
            case 't':
                if (!literal("true")) {
                    return false;
                }
                out = Value::boolean(true);
                return true;
            case 'f':
                if (!literal("false")) {
                    return false;
                }
                out = Value::boolean(false);
                return true;
            case 'n':
                if (!literal("null")) {
                    return false;
                }
                out = Value::null();
                return true;
            default: {
                double v = 0.0;
                if (!parseNumber(v)) {
                    return false;
                }
                out = Value::number(v);
                return true;
            }
        }
    }

    bool parseObject(Value& out, int depth) {
        ++pos_;   // '{'
        Value result = Value::object();
        skipWs();
        if (pos_ < text_.size() && text_[pos_] == '}') {
            ++pos_;
            out = result;
            return true;
        }
        while (pos_ < text_.size()) {
            skipWs();
            if (pos_ >= text_.size() || text_[pos_] != '"') {
                return false;
            }
            std::string key;
            if (!parseString(key)) {
                return false;
            }
            skipWs();
            if (pos_ >= text_.size() || text_[pos_] != ':') {
                return false;
            }
            ++pos_;
            skipWs();
            Value item;
            if (!parseValue(item, depth + 1)) {
                return false;
            }
            result.set(key, item);
            skipWs();
            if (pos_ >= text_.size()) {
                return false;
            }
            if (text_[pos_] == ',') {
                ++pos_;
                continue;
            }
            if (text_[pos_] == '}') {
                ++pos_;
                out = result;
                return true;
            }
            return false;
        }
        return false;
    }

    bool parseArray(Value& out, int depth) {
        ++pos_;   // '['
        Value result = Value::array();
        skipWs();
        if (pos_ < text_.size() && text_[pos_] == ']') {
            ++pos_;
            out = result;
            return true;
        }
        while (pos_ < text_.size()) {
            skipWs();
            Value item;
            if (!parseValue(item, depth + 1)) {
                return false;
            }
            result.push(item);
            skipWs();
            if (pos_ >= text_.size()) {
                return false;
            }
            if (text_[pos_] == ',') {
                ++pos_;
                continue;
            }
            if (text_[pos_] == ']') {
                ++pos_;
                out = result;
                return true;
            }
            return false;
        }
        return false;
    }

    // 解析字符串字面量（含 \uXXXX 转义，代理对以外的码点按 UTF-8 编码）
    bool parseString(std::string& out) {
        if (pos_ >= text_.size() || text_[pos_] != '"') {
            return false;
        }
        ++pos_;
        out.clear();
        while (pos_ < text_.size()) {
            const char ch = text_[pos_];
            if (ch == '"') {
                ++pos_;
                return true;
            }
            if (ch == '\\') {
                ++pos_;
                if (pos_ >= text_.size()) {
                    return false;
                }
                const char esc = text_[pos_++];
                switch (esc) {
                    case '"': out.push_back('"'); break;
                    case '\\': out.push_back('\\'); break;
                    case '/': out.push_back('/'); break;
                    case 'b': out.push_back('\b'); break;
                    case 'f': out.push_back('\f'); break;
                    case 'n': out.push_back('\n'); break;
                    case 'r': out.push_back('\r'); break;
                    case 't': out.push_back('\t'); break;
                    case 'u': {
                        unsigned code = 0;
                        if (!parseHex4(code)) {
                            return false;
                        }
                        appendUtf8(code, out);
                        break;
                    }
                    default: return false;
                }
                continue;
            }
            if (static_cast<unsigned char>(ch) < 0x20) {
                return false;   // 裸控制字符不允许出现在字符串里
            }
            out.push_back(ch);
            ++pos_;
        }
        return false;
    }

    bool parseHex4(unsigned& out) {
        if (pos_ + 4 > text_.size()) {
            return false;
        }
        out = 0;
        for (int i = 0; i < 4; ++i) {
            const char ch = text_[pos_ + i];
            out <<= 4;
            if (ch >= '0' && ch <= '9') {
                out |= static_cast<unsigned>(ch - '0');
            } else if (ch >= 'a' && ch <= 'f') {
                out |= static_cast<unsigned>(ch - 'a' + 10);
            } else if (ch >= 'A' && ch <= 'F') {
                out |= static_cast<unsigned>(ch - 'A' + 10);
            } else {
                return false;
            }
        }
        pos_ += 4;
        return true;
    }

    static void appendUtf8(unsigned code, std::string& out) {
        if (code < 0x80) {
            out.push_back(static_cast<char>(code));
        } else if (code < 0x800) {
            out.push_back(static_cast<char>(0xC0 | (code >> 6)));
            out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
        } else {
            out.push_back(static_cast<char>(0xE0 | (code >> 12)));
            out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
        }
    }

    // 严格 JSON 数字文法：无前导 '+'、多位整数不以 0 开头、小数点后至少一位数字
    bool parseNumber(double& out) {
        const std::size_t begin = pos_;
        if (pos_ < text_.size() && text_[pos_] == '-') {
            ++pos_;
        }
        const std::size_t intBegin = pos_;
        bool any = false;
        while (pos_ < text_.size() && isDigit(text_[pos_])) {
            ++pos_;
            any = true;
        }
        if (pos_ - intBegin > 1 && text_[intBegin] == '0') {
            return false;
        }
        if (pos_ < text_.size() && text_[pos_] == '.') {
            ++pos_;
            const std::size_t fracBegin = pos_;
            while (pos_ < text_.size() && isDigit(text_[pos_])) {
                ++pos_;
                any = true;
            }
            if (pos_ == fracBegin) {
                return false;
            }
        }
        if (!any) {
            return false;
        }
        if (pos_ < text_.size() && (text_[pos_] == 'e' || text_[pos_] == 'E')) {
            ++pos_;
            if (pos_ < text_.size() && (text_[pos_] == '-' || text_[pos_] == '+')) {
                ++pos_;
            }
            bool expAny = false;
            while (pos_ < text_.size() && isDigit(text_[pos_])) {
                ++pos_;
                expAny = true;
            }
            if (!expAny) {
                return false;
            }
        }
        out = std::strtod(text_.substr(begin, pos_ - begin).c_str(), nullptr);
        return true;
    }

    static bool isDigit(char ch) { return ch >= '0' && ch <= '9'; }

    const std::string& text_;
    std::size_t pos_ = 0;
};

inline bool parse(const std::string& text, Value& out) {
    Parser parser(text);
    return parser.parse(out);
}

} // namespace json
} // namespace sched

#endif // SCHED_COMMON_JSONUTIL_H
