// SPDX-License-Identifier: MIT
// Json 实现：递归下降解析 + 紧凑序列化（详细设计 §4.4 / §4.6）
//
// 解析器只做「文本 → 值」一件事，不掺任何业务判断；失败一律返回 false 并给出
// 一句话原因（error 出参），不抛异常——与项目「热路径不用异常」的口径一致。
// 递归深度在上限处硬截断，避免恶意/损坏输入打穿调用栈。

#include "common/include/Json.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace sched {
namespace json {

namespace {

constexpr int kMaxDepth = 16;                       // 嵌套层数上限
constexpr std::size_t kMaxText = 1024u * 1024u;     // 单次解析文本上限（1 MiB）

bool isDigit(char ch) {
    return ch >= '0' && ch <= '9';
}

// 码点 → UTF-8
void appendUtf8(std::string& out, unsigned int cp) {
    if (cp <= 0x7Fu) {
        out.push_back(static_cast<char>(cp));
    } else if (cp <= 0x7FFu) {
        out.push_back(static_cast<char>(0xC0u | (cp >> 6)));
        out.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
    } else if (cp <= 0xFFFFu) {
        out.push_back(static_cast<char>(0xE0u | (cp >> 12)));
        out.push_back(static_cast<char>(0x80u | ((cp >> 6) & 0x3Fu)));
        out.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
    } else {
        out.push_back(static_cast<char>(0xF0u | (cp >> 18)));
        out.push_back(static_cast<char>(0x80u | ((cp >> 12) & 0x3Fu)));
        out.push_back(static_cast<char>(0x80u | ((cp >> 6) & 0x3Fu)));
        out.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
    }
}

// 字符串 → 带引号、已转义
void appendEscaped(std::string& out, const std::string& text) {
    out.push_back('"');
    for (const char ch : text) {
        const unsigned char c = static_cast<unsigned char>(ch);
        switch (c) {
        case '"':  out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\b': out += "\\b"; break;
        case '\f': out += "\\f"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:
            if (c < 0x20u) {
                char buf[8];
                std::snprintf(buf, sizeof buf, "\\u%04x", static_cast<unsigned int>(c));
                out += buf;
            } else {
                out.push_back(static_cast<char>(c));
            }
            break;
        }
    }
    out.push_back('"');
}

// 数字：整数值按整数输出（频率 408000 不会写成 408000.0），其余保留 10 位有效数字
void appendNumber(std::string& out, double value) {
    if (!std::isfinite(value)) {
        out += "null";
        return;
    }
    if (value == std::floor(value) && std::fabs(value) < 1e15) {
        char buf[32];
        std::snprintf(buf, sizeof buf, "%.0f", value);
        out += buf;
        return;
    }
    char buf[40];
    std::snprintf(buf, sizeof buf, "%.10g", value);
    out += buf;
}

class Parser {
public:
    explicit Parser(const std::string& text)
        : p_(text.data()), end_(text.data() + text.size()) {}

    bool parse(Value& out) {
        // 跳过 UTF-8 BOM（EF BB BF）
        if (static_cast<std::size_t>(end_ - p_) >= 3 &&
            static_cast<unsigned char>(p_[0]) == 0xEF &&
            static_cast<unsigned char>(p_[1]) == 0xBB &&
            static_cast<unsigned char>(p_[2]) == 0xBF) {
            p_ += 3;
        }
        skipWs();
        if (!parseValue(out, 0)) {
            return false;
        }
        skipWs();
        if (p_ != end_) {
            return fail("trailing characters");
        }
        return true;
    }

    const std::string& error() const { return error_; }

private:
    bool fail(const char* message) {
        if (error_.empty()) {
            error_ = message;
        }
        return false;
    }

    void skipWs() {
        while (p_ < end_) {
            const char c = *p_;
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
                ++p_;
            } else {
                break;
            }
        }
    }

    bool match(const char* literal) {
        const std::size_t n = std::strlen(literal);
        if (static_cast<std::size_t>(end_ - p_) < n) {
            return false;
        }
        if (std::memcmp(p_, literal, n) != 0) {
            return false;
        }
        p_ += n;
        return true;
    }

    bool parseValue(Value& out, int depth) {
        if (depth > kMaxDepth) {
            return fail("nesting too deep");
        }
        if (p_ >= end_) {
            return fail("unexpected end of text");
        }
        switch (*p_) {
        case '{': return parseObject(out, depth);
        case '[': return parseArray(out, depth);
        case '"': {
            std::string text;
            if (!parseString(text)) {
                return false;
            }
            out = Value::string(text);
            return true;
        }
        case 't':
            if (match("true")) {
                out = Value::boolean(true);
                return true;
            }
            return fail("bad literal");
        case 'f':
            if (match("false")) {
                out = Value::boolean(false);
                return true;
            }
            return fail("bad literal");
        case 'n':
            if (match("null")) {
                out = Value();
                return true;
            }
            return fail("bad literal");
        default:
            break;
        }
        if (*p_ == '-' || isDigit(*p_)) {
            double number = 0.0;
            if (!parseNumber(number)) {
                return false;
            }
            out = Value::number(number);
            return true;
        }
        return fail("unexpected character");
    }

    bool parseObject(Value& out, int depth) {
        ++p_;                                    // '{'
        out = Value::object();
        skipWs();
        if (p_ < end_ && *p_ == '}') {
            ++p_;
            return true;
        }
        while (true) {
            skipWs();
            std::string key;
            if (!parseString(key)) {
                return false;
            }
            skipWs();
            if (p_ >= end_ || *p_ != ':') {
                return fail("expected ':'");
            }
            ++p_;
            skipWs();
            Value child;
            if (!parseValue(child, depth + 1)) {
                return false;
            }
            out.set(key, std::move(child));      // 重复键：后者覆盖
            skipWs();
            if (p_ >= end_) {
                return fail("unterminated object");
            }
            if (*p_ == ',') {
                ++p_;
                skipWs();
                if (p_ < end_ && *p_ == '}') {
                    return fail("trailing comma");
                }
                continue;
            }
            if (*p_ == '}') {
                ++p_;
                return true;
            }
            return fail("expected ',' or '}'");
        }
    }

    bool parseArray(Value& out, int depth) {
        ++p_;                                    // '['
        out = Value::array();
        skipWs();
        if (p_ < end_ && *p_ == ']') {
            ++p_;
            return true;
        }
        while (true) {
            skipWs();
            Value child;
            if (!parseValue(child, depth + 1)) {
                return false;
            }
            out.push(std::move(child));
            skipWs();
            if (p_ >= end_) {
                return fail("unterminated array");
            }
            if (*p_ == ',') {
                ++p_;
                skipWs();
                if (p_ < end_ && *p_ == ']') {
                    return fail("trailing comma");
                }
                continue;
            }
            if (*p_ == ']') {
                ++p_;
                return true;
            }
            return fail("expected ',' or ']'");
        }
    }

    bool parseString(std::string& out) {
        if (p_ >= end_ || *p_ != '"') {
            return fail("expected string");
        }
        ++p_;
        out.clear();
        while (true) {
            if (p_ >= end_) {
                return fail("unterminated string");
            }
            const unsigned char c = static_cast<unsigned char>(*p_);
            if (c == '"') {
                ++p_;
                return true;
            }
            if (c == '\\') {
                ++p_;
                if (p_ >= end_) {
                    return fail("unterminated escape");
                }
                const char esc = *p_++;
                switch (esc) {
                case '"':  out.push_back('"'); break;
                case '\\': out.push_back('\\'); break;
                case '/':  out.push_back('/'); break;
                case 'b':  out.push_back('\b'); break;
                case 'f':  out.push_back('\f'); break;
                case 'n':  out.push_back('\n'); break;
                case 'r':  out.push_back('\r'); break;
                case 't':  out.push_back('\t'); break;
                case 'u': {
                    unsigned int cp = 0;
                    if (!parseHex4(cp)) {
                        return false;
                    }
                    if (cp >= 0xD800u && cp <= 0xDBFFu) {
                        // 高代理项：必须紧跟 \uDC00-\uDFFF
                        if (static_cast<std::size_t>(end_ - p_) < 2 || p_[0] != '\\' || p_[1] != 'u') {
                            return fail("lone high surrogate");
                        }
                        p_ += 2;
                        unsigned int low = 0;
                        if (!parseHex4(low)) {
                            return false;
                        }
                        if (low < 0xDC00u || low > 0xDFFFu) {
                            return fail("bad low surrogate");
                        }
                        cp = 0x10000u + ((cp - 0xD800u) << 10) + (low - 0xDC00u);
                    } else if (cp >= 0xDC00u && cp <= 0xDFFFu) {
                        return fail("lone low surrogate");
                    }
                    appendUtf8(out, cp);
                    break;
                }
                default:
                    return fail("bad escape");
                }
                continue;
            }
            if (c < 0x20u) {
                return fail("control character in string");
            }
            out.push_back(static_cast<char>(c));
            ++p_;
        }
    }

    bool parseHex4(unsigned int& out) {
        if (static_cast<std::size_t>(end_ - p_) < 4) {
            return fail("short \\u escape");
        }
        unsigned int value = 0;
        for (int i = 0; i < 4; ++i) {
            const unsigned char c = static_cast<unsigned char>(p_[i]);
            int digit = 0;
            if (c >= '0' && c <= '9') {
                digit = c - '0';
            } else if (c >= 'a' && c <= 'f') {
                digit = c - 'a' + 10;
            } else if (c >= 'A' && c <= 'F') {
                digit = c - 'A' + 10;
            } else {
                return fail("bad \\u escape");
            }
            value = (value << 4) | static_cast<unsigned int>(digit);
        }
        p_ += 4;
        out = value;
        return true;
    }

    bool parseNumber(double& out) {
        const char* const start = p_;
        if (p_ < end_ && *p_ == '-') {
            ++p_;
        }
        if (p_ >= end_) {
            return fail("bad number");
        }
        if (*p_ == '0') {
            ++p_;
            if (p_ < end_ && isDigit(*p_)) {
                return fail("leading zero");
            }
        } else if (isDigit(*p_)) {
            while (p_ < end_ && isDigit(*p_)) {
                ++p_;
            }
        } else {
            return fail("bad number");
        }
        if (p_ < end_ && *p_ == '.') {
            ++p_;
            if (p_ >= end_ || !isDigit(*p_)) {
                return fail("bad fraction");
            }
            while (p_ < end_ && isDigit(*p_)) {
                ++p_;
            }
        }
        if (p_ < end_ && (*p_ == 'e' || *p_ == 'E')) {
            ++p_;
            if (p_ < end_ && (*p_ == '+' || *p_ == '-')) {
                ++p_;
            }
            if (p_ >= end_ || !isDigit(*p_)) {
                return fail("bad exponent");
            }
            while (p_ < end_ && isDigit(*p_)) {
                ++p_;
            }
        }
        const std::string token(start, p_);
        char* stop = nullptr;
        const double value = std::strtod(token.c_str(), &stop);
        if (stop == nullptr || *stop != '\0') {
            return fail("bad number");
        }
        if (!std::isfinite(value)) {
            return fail("non-finite number");
        }
        out = value;
        return true;
    }

    const char* p_ = nullptr;
    const char* end_ = nullptr;
    std::string error_;
};

} // namespace

Value Value::boolean(bool value) {
    Value v;
    v.type_ = Type::Bool;
    v.bool_ = value;
    return v;
}

Value Value::number(double value) {
    Value v;
    v.type_ = Type::Number;
    v.number_ = value;
    return v;
}

Value Value::string(const std::string& value) {
    Value v;
    v.type_ = Type::String;
    v.string_ = value;
    return v;
}

Value Value::array() {
    Value v;
    v.type_ = Type::Array;
    return v;
}

Value Value::object() {
    Value v;
    v.type_ = Type::Object;
    return v;
}

bool Value::asBool(bool& out) const {
    if (type_ != Type::Bool) {
        return false;
    }
    out = bool_;
    return true;
}

bool Value::asNumber(double& out) const {
    if (type_ != Type::Number) {
        return false;
    }
    out = number_;
    return true;
}

bool Value::asInt(int& out) const {
    if (type_ != Type::Number || !std::isfinite(number_)) {
        return false;
    }
    if (number_ != std::floor(number_)) {
        return false;
    }
    if (number_ < -2147483648.0 || number_ > 2147483647.0) {
        return false;
    }
    out = static_cast<int>(number_);
    return true;
}

bool Value::asString(std::string& out) const {
    if (type_ != Type::String) {
        return false;
    }
    out = string_;
    return true;
}

const Value* Value::find(const std::string& key) const {
    if (type_ != Type::Object) {
        return nullptr;
    }
    for (const auto& item : object_) {
        if (item.first == key) {
            return &item.second;
        }
    }
    return nullptr;
}

std::size_t Value::size() const {
    switch (type_) {
    case Type::Array:  return array_.size();
    case Type::Object: return object_.size();
    case Type::String: return string_.size();
    default:           return 0;
    }
}

void Value::set(const std::string& key, Value value) {
    if (type_ == Type::Null) {
        type_ = Type::Object;            // 便于编码侧 `Value v; v.set(...)`
    }
    if (type_ != Type::Object) {
        return;                          // 误用：静默忽略，不破坏已有值
    }
    for (auto& item : object_) {
        if (item.first == key) {
            item.second = std::move(value);
            return;
        }
    }
    object_.emplace_back(key, std::move(value));
}

void Value::push(Value value) {
    if (type_ == Type::Null) {
        type_ = Type::Array;
    }
    if (type_ != Type::Array) {
        return;
    }
    array_.push_back(std::move(value));
}

std::string Value::dump() const {
    std::string out;
    write(out);
    return out;
}

void Value::write(std::string& out) const {
    switch (type_) {
    case Type::Null:
        out += "null";
        break;
    case Type::Bool:
        out += (bool_ ? "true" : "false");
        break;
    case Type::Number:
        appendNumber(out, number_);
        break;
    case Type::String:
        appendEscaped(out, string_);
        break;
    case Type::Array:
        out.push_back('[');
        for (std::size_t i = 0; i < array_.size(); ++i) {
            if (i != 0) {
                out.push_back(',');
            }
            array_[i].write(out);
        }
        out.push_back(']');
        break;
    case Type::Object:
        out.push_back('{');
        for (std::size_t i = 0; i < object_.size(); ++i) {
            if (i != 0) {
                out.push_back(',');
            }
            appendEscaped(out, object_[i].first);
            out.push_back(':');
            object_[i].second.write(out);
        }
        out.push_back('}');
        break;
    }
}

bool Value::parse(const std::string& text, Value& out, std::string* error) {
    if (text.size() > kMaxText) {
        if (error != nullptr) {
            *error = "text too large";
        }
        return false;
    }
    Parser parser(text);
    Value parsed;
    if (!parser.parse(parsed)) {
        if (error != nullptr) {
            *error = parser.error();
        }
        return false;
    }
    out = std::move(parsed);
    return true;
}

} // namespace json
} // namespace sched
