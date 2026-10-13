#include "Json.h"

#include <cmath>
#include <cstdio>
#include <stdexcept>

namespace common {
namespace {

const Json& nullValue() {
    static const Json kNull;   // C++11 起静态局部变量初始化线程安全
    return kNull;
}

bool isDigit(char c) { return c >= '0' && c <= '9'; }

// 把码点写成 UTF-8 追加到 out（\uXXXX 转义还原用）
void appendUtf8(unsigned int cp, std::string& out) {
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

// 整数一律不带小数点输出；浮点用 %.9g —— double 的十进制往返需要 17 位有效数字，
// 但契约里的量都是 0~1 的比值与档位整数，9 位足以无损表达且不会出现 0.25000000001 之类噪声。
std::string formatNumber(double v) {
    if (!std::isfinite(v)) {
        return "0";   // 非有限值无法合法表达为 JSON 数字，退化为 0
    }
    if (v >= -9007199254740992.0 && v <= 9007199254740992.0) {
        const double rounded = (v < 0.0) ? std::ceil(v - 0.5) : std::floor(v + 0.5);
        if (rounded == v) {
            char buf[32];
            std::snprintf(buf, sizeof(buf), "%lld", static_cast<long long>(v));
            return buf;
        }
    }
    char buf[40];
    std::snprintf(buf, sizeof(buf), "%.9g", v);
    return buf;
}

void dumpString(const std::string& s, std::string& out) {
    out.push_back('"');
    for (char rawChar : s) {
        const unsigned char c = static_cast<unsigned char>(rawChar);
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b";  break;
            case '\f': out += "\\f";  break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if (c < 0x20u) {
                    char esc[8];
                    std::snprintf(esc, sizeof(esc), "\\u%04x", static_cast<unsigned>(c));
                    out += esc;
                } else {
                    out.push_back(rawChar);   // UTF-8 字节原样直通
                }
                break;
        }
    }
    out.push_back('"');
}

void dumpValue(const Json& value, std::string& out) {
    switch (value.type()) {
        case Json::Type::Null:
            out += "null";
            return;
        case Json::Type::Bool:
            out += value.asBool() ? "true" : "false";
            return;
        case Json::Type::Number:
            out += formatNumber(value.asNumber());
            return;
        case Json::Type::String:
            dumpString(value.asString(), out);
            return;
        case Json::Type::Array: {
            out.push_back('[');
            const std::vector<Json>& elems = value.elements();
            for (std::size_t i = 0; i < elems.size(); ++i) {
                if (i != 0) {
                    out.push_back(',');
                }
                dumpValue(elems[i], out);
            }
            out.push_back(']');
            return;
        }
        case Json::Type::Object: {
            out.push_back('{');
            const std::vector<std::pair<std::string, Json>>& items = value.items();
            for (std::size_t i = 0; i < items.size(); ++i) {
                if (i != 0) {
                    out.push_back(',');
                }
                dumpString(items[i].first, out);
                out.push_back(':');
                dumpValue(items[i].second, out);
            }
            out.push_back('}');
            return;
        }
    }
}

// ---------------------------------------------------------------- 递归下降解析

class Parser {
public:
    explicit Parser(const std::string& text) : m_text(text) {}

    bool parse(Json& out) {
        skipWs();
        Json value;
        if (!parseValue(value)) {
            return false;
        }
        skipWs();
        if (m_pos != m_text.size()) {
            return false;   // 尾随垃圾：判失败，避免"半截报文"被当合法
        }
        out = std::move(value);
        return true;
    }

private:
    void skipWs() {
        while (m_pos < m_text.size()) {
            const char c = m_text[m_pos];
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
                ++m_pos;
                continue;
            }
            break;
        }
    }

    bool expect(const char* literal) {
        for (const char* p = literal; *p != '\0'; ++p) {
            if (m_pos >= m_text.size() || m_text[m_pos] != *p) {
                return false;
            }
            ++m_pos;
        }
        return true;
    }

    bool parseHex4(unsigned int& cp) {
        if (m_pos + 4 > m_text.size()) {
            return false;
        }
        cp = 0;
        for (int i = 0; i < 4; ++i) {
            const char c = m_text[m_pos++];
            unsigned int nibble = 0;
            if (c >= '0' && c <= '9') {
                nibble = static_cast<unsigned int>(c - '0');
            } else if (c >= 'a' && c <= 'f') {
                nibble = static_cast<unsigned int>(c - 'a') + 10u;
            } else if (c >= 'A' && c <= 'F') {
                nibble = static_cast<unsigned int>(c - 'A') + 10u;
            } else {
                return false;
            }
            cp = (cp << 4) | nibble;
        }
        return true;
    }

    bool parseString(std::string& out) {
        ++m_pos;   // 跳过起始引号
        out.clear();
        while (m_pos < m_text.size()) {
            const char c = m_text[m_pos++];
            if (c == '"') {
                return true;
            }
            if (c != '\\') {
                out.push_back(c);
                continue;
            }
            if (m_pos >= m_text.size()) {
                return false;
            }
            const char esc = m_text[m_pos++];
            switch (esc) {
                case '"':  out.push_back('"');  break;
                case '\\': out.push_back('\\'); break;
                case '/':  out.push_back('/');  break;
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
                        // 高位代理：必须紧跟低位代理，否则视为非法
                        if (m_pos + 1 >= m_text.size() || m_text[m_pos] != '\\' ||
                            m_text[m_pos + 1] != 'u') {
                            return false;
                        }
                        m_pos += 2;
                        unsigned int low = 0;
                        if (!parseHex4(low)) {
                            return false;
                        }
                        if (low < 0xDC00u || low > 0xDFFFu) {
                            return false;
                        }
                        cp = 0x10000u + ((cp - 0xD800u) << 10) + (low - 0xDC00u);
                    } else if (cp >= 0xDC00u && cp <= 0xDFFFu) {
                        return false;   // 孤立低位代理
                    }
                    appendUtf8(cp, out);
                    break;
                }
                default:
                    return false;
            }
        }
        return false;   // 引号未闭合
    }

    bool parseNumber(Json& out) {
        const std::size_t start = m_pos;
        if (m_pos < m_text.size() && m_text[m_pos] == '-') {
            ++m_pos;
        }
        if (m_pos >= m_text.size() || !isDigit(m_text[m_pos])) {
            return false;
        }
        if (m_text[m_pos] == '0') {
            ++m_pos;
        } else {
            while (m_pos < m_text.size() && isDigit(m_text[m_pos])) {
                ++m_pos;
            }
        }
        if (m_pos < m_text.size() && m_text[m_pos] == '.') {
            ++m_pos;
            if (m_pos >= m_text.size() || !isDigit(m_text[m_pos])) {
                return false;
            }
            while (m_pos < m_text.size() && isDigit(m_text[m_pos])) {
                ++m_pos;
            }
        }
        if (m_pos < m_text.size() && (m_text[m_pos] == 'e' || m_text[m_pos] == 'E')) {
            ++m_pos;
            if (m_pos < m_text.size() && (m_text[m_pos] == '+' || m_text[m_pos] == '-')) {
                ++m_pos;
            }
            if (m_pos >= m_text.size() || !isDigit(m_text[m_pos])) {
                return false;
            }
            while (m_pos < m_text.size() && isDigit(m_text[m_pos])) {
                ++m_pos;
            }
        }
        try {
            out = Json(std::stod(m_text.substr(start, m_pos - start)));
        } catch (const std::exception&) {
            return false;   // 溢出 / 非数
        }
        return true;
    }

    bool parseArray(Json& out) {
        ++m_pos;   // '['
        Json arr = Json::array();
        skipWs();
        if (m_pos < m_text.size() && m_text[m_pos] == ']') {
            ++m_pos;
            out = std::move(arr);
            return true;
        }
        while (true) {
            skipWs();
            Json element;
            if (!parseValue(element)) {
                return false;
            }
            arr.push(std::move(element));
            skipWs();
            if (m_pos >= m_text.size()) {
                return false;
            }
            if (m_text[m_pos] == ',') {
                ++m_pos;
                continue;
            }
            if (m_text[m_pos] == ']') {
                ++m_pos;
                out = std::move(arr);
                return true;
            }
            return false;
        }
    }

    bool parseObject(Json& out) {
        ++m_pos;   // '{'
        Json obj = Json::object();
        skipWs();
        if (m_pos < m_text.size() && m_text[m_pos] == '}') {
            ++m_pos;
            out = std::move(obj);
            return true;
        }
        while (true) {
            skipWs();
            if (m_pos >= m_text.size() || m_text[m_pos] != '"') {
                return false;
            }
            std::string key;
            if (!parseString(key)) {
                return false;
            }
            skipWs();
            if (m_pos >= m_text.size() || m_text[m_pos] != ':') {
                return false;
            }
            ++m_pos;
            skipWs();
            Json value;
            if (!parseValue(value)) {
                return false;
            }
            obj.set(key, std::move(value));   // 重复键以后者为准
            skipWs();
            if (m_pos >= m_text.size()) {
                return false;
            }
            if (m_text[m_pos] == ',') {
                ++m_pos;
                continue;
            }
            if (m_text[m_pos] == '}') {
                ++m_pos;
                out = std::move(obj);
                return true;
            }
            return false;
        }
    }

    bool parseValue(Json& out) {
        if (m_pos >= m_text.size()) {
            return false;
        }
        switch (m_text[m_pos]) {
            case '{': return parseObject(out);
            case '[': return parseArray(out);
            case '"': {
                std::string s;
                if (!parseString(s)) {
                    return false;
                }
                out = Json(std::move(s));
                return true;
            }
            case 't':
                if (!expect("true")) {
                    return false;
                }
                out = Json(true);
                return true;
            case 'f':
                if (!expect("false")) {
                    return false;
                }
                out = Json(false);
                return true;
            case 'n':
                if (!expect("null")) {
                    return false;
                }
                out = Json();
                return true;
            default:
                return parseNumber(out);
        }
    }

    const std::string& m_text;
    std::size_t        m_pos = 0;
};

}  // namespace

// ---------------------------------------------------------------- 构造

Json::Json(bool value) : m_type(Type::Bool), m_bool(value) {}

Json::Json(double value) : m_type(Type::Number), m_number(value) {}

Json::Json(int value) : m_type(Type::Number), m_number(static_cast<double>(value)) {}

Json::Json(const char* value) : m_type(Type::String), m_string(value != nullptr ? value : "") {}

Json::Json(std::string value) : m_type(Type::String), m_string(std::move(value)) {}

Json Json::array() {
    Json j;
    j.m_type = Type::Array;
    return j;
}

Json Json::object() {
    Json j;
    j.m_type = Type::Object;
    return j;
}

// ---------------------------------------------------------------- 取值

bool Json::asBool(bool defaultValue) const {
    switch (m_type) {
        case Type::Bool:   return m_bool;
        case Type::Number: return m_number != 0.0;
        default:           return defaultValue;
    }
}

double Json::asNumber(double defaultValue) const {
    switch (m_type) {
        case Type::Number: return m_number;
        case Type::Bool:   return m_bool ? 1.0 : 0.0;
        default:           return defaultValue;
    }
}

int Json::asInt(int defaultValue) const {
    switch (m_type) {
        case Type::Number: {
            if (!std::isfinite(m_number) || m_number < -2147483648.0 ||
                m_number > 2147483647.0) {
                return defaultValue;   // 越 int 表示范围：判非法而非截断
            }
            return static_cast<int>(m_number);
        }
        case Type::Bool: return m_bool ? 1 : 0;
        default:         return defaultValue;
    }
}

std::string Json::asString(const std::string& defaultValue) const {
    return (m_type == Type::String) ? m_string : defaultValue;
}

// ---------------------------------------------------------------- 访问

bool Json::has(const std::string& key) const {
    if (m_type != Type::Object) {
        return false;
    }
    for (const auto& kv : m_object) {
        if (kv.first == key) {
            return true;
        }
    }
    return false;
}

const Json& Json::operator[](const std::string& key) const {
    if (m_type == Type::Object) {
        for (const auto& kv : m_object) {
            if (kv.first == key) {
                return kv.second;
            }
        }
    }
    return nullValue();
}

const Json& Json::operator[](std::size_t index) const {
    if (m_type == Type::Array && index < m_array.size()) {
        return m_array[index];
    }
    return nullValue();
}

std::size_t Json::size() const {
    if (m_type == Type::Array) {
        return m_array.size();
    }
    if (m_type == Type::Object) {
        return m_object.size();
    }
    return 0;
}

// ---------------------------------------------------------------- 构造（序列化侧）

void Json::set(const std::string& key, Json value) {
    if (m_type != Type::Object) {
        m_type = Type::Object;
        m_bool = false;
        m_number = 0.0;
        m_string.clear();
        m_array.clear();
        m_object.clear();
    }
    for (auto& kv : m_object) {
        if (kv.first == key) {
            kv.second = std::move(value);
            return;
        }
    }
    m_object.emplace_back(key, std::move(value));
}

void Json::push(Json value) {
    if (m_type != Type::Array) {
        m_type = Type::Array;
        m_bool = false;
        m_number = 0.0;
        m_string.clear();
        m_array.clear();
        m_object.clear();
    }
    m_array.push_back(std::move(value));
}

// ---------------------------------------------------------------- 序列化 / 解析

std::string Json::dump() const {
    std::string out;
    dumpValue(*this, out);
    return out;
}

bool Json::parse(const std::string& text, Json& out) {
    Json parsed;
    Parser parser(text);
    if (!parser.parse(parsed)) {
        return false;
    }
    out = std::move(parsed);
    return true;
}

}  // namespace common
