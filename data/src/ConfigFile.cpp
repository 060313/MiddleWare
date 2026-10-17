// SPDX-License-Identifier: MIT
// ConfigFile 实现（详细设计 §4.5.2 / §4.3.4）
//
// 容错口径（§4.3.4）：**校验与取值同口径**——唯一失败判据是「节 / 键缺失」；
// 整数键留空或非法字面量一律按默认值处理，不判配置非法。

#include "data/include/ConfigFile.h"

#include <cctype>
#include <cstdlib>
#include <utility>
#include <vector>

#include "common/include/FileService.h"

namespace sched {

namespace {

// 去掉首尾空白（含 \r，兼容 CRLF 文本）
std::string trim(const std::string& s) {
    std::size_t begin = 0;
    std::size_t end = s.size();
    while (begin < end && std::isspace(static_cast<unsigned char>(s[begin])) != 0) {
        ++begin;
    }
    while (end > begin && std::isspace(static_cast<unsigned char>(s[end - 1])) != 0) {
        --end;
    }
    return s.substr(begin, end - begin);
}

// 按单个分隔符切分（保留空段）
std::vector<std::string> splitLines(const std::string& text) {
    std::vector<std::string> lines;
    std::string current;
    for (const char ch : text) {
        if (ch == '\n') {
            lines.push_back(current);
            current.clear();
        } else {
            current.push_back(ch);
        }
    }
    if (!current.empty()) {
        lines.push_back(current);
    }
    return lines;
}

// 严格整数解析：整串必须是十进制整数，否则视为非法字面量
bool parseStrictInt(const std::string& text, int& out) {
    if (text.empty()) {
        return false;
    }
    std::size_t i = 0;
    if (text[i] == '+' || text[i] == '-') {
        ++i;
    }
    if (i >= text.size()) {
        return false;
    }
    for (std::size_t k = i; k < text.size(); ++k) {
        if (std::isdigit(static_cast<unsigned char>(text[k])) == 0) {
            return false;
        }
    }
    out = std::atoi(text.c_str());
    return true;
}

} // namespace

bool ConfigFile::parse(const std::string& text,
                       std::map<std::string, std::map<std::string, std::string>>& out) {
    out.clear();

    std::string section;
    for (const std::string& rawLine : splitLines(text)) {
        const std::string line = trim(rawLine);

        // S3.1 跳过空行与注释行
        if (line.empty() || line[0] == '#' || line[0] == ';') {
            continue;
        }

        // S3.2 节头 [section]
        if (line.front() == '[') {
            const std::string::size_type close = line.find(']');
            if (close == std::string::npos) {
                continue;   // 节头格式非法 → 跳过该行（不整体失败）
            }
            section = trim(line.substr(1, close - 1));
            continue;
        }

        // S3.3 key = value（按第一个 '=' 切分）
        const std::string::size_type eq = line.find('=');
        if (eq == std::string::npos) {
            continue;       // S3.4 行格式非法 → 跳过该行
        }
        const std::string key = trim(line.substr(0, eq));
        const std::string value = trim(line.substr(eq + 1));
        if (key.empty()) {
            continue;
        }
        out[section][key] = value;   // 键存在即入表（值为空串也存，供 has 区分）
    }
    return true;
}

bool ConfigFile::getConfig(const std::string& configPath) {
    // S1. 路径非空
    if (configPath.empty()) {
        return false;
    }

    // S2. 读全文；失败时**已有解析结果保持不变**
    const std::string text = FileService::read(configPath);
    if (text.empty()) {
        return false;
    }

    // S3. 解析到临时映射，成功后整体替换（S4）
    std::map<std::string, std::map<std::string, std::string>> parsed;
    parse(text, parsed);
    values_ = std::move(parsed);
    path_ = configPath;
    return true;
}

bool ConfigFile::configValidate(const std::string& configPath) {
    // S1. 路径非空
    if (configPath.empty()) {
        return false;
    }

    // S2. 读入并解析到**临时映射**（不污染已生效内容）
    const std::string text = FileService::read(configPath);
    if (text.empty()) {
        return false;
    }
    std::map<std::string, std::map<std::string, std::string>> parsed;
    parse(text, parsed);

    // S3. 逐节逐键校验：三节必须存在，关键键必须存在（§4.3.1~§4.3.3）
    const char* const kSections[] = {"server", "agent", "system"};
    for (const char* const section : kSections) {
        if (parsed.find(section) == parsed.end()) {
            return false;
        }
    }

    const std::pair<const char*, const char*> kRequiredKeys[] = {
        {"server", "socket_path"},
        {"server", "max_conn"},
        {"agent", "model_file"},
        {"system", "cpu_freq_min"},
        {"system", "cpu_freq_max"},
        {"system", "npu_freq_min"},
        {"system", "npu_freq_max"},
        {"system", "cpu_freq_path"},
        {"system", "npu_freq_path"},
        {"system", "proc_stat_path"},
        {"system", "sample_interval_ms"},
    };
    for (const auto& item : kRequiredKeys) {
        const auto sectionIt = parsed.find(item.first);
        if (sectionIt == parsed.end() || sectionIt->second.find(item.second) == sectionIt->second.end()) {
            return false;
        }
    }

    // S4. 取值容错：整数留空 / 非法字面量一律按默认值处理，**不判配置非法**
    // S5. 全部通过
    return true;
}

std::string ConfigFile::getString(const std::string& section,
                                  const std::string& key,
                                  const std::string& def) const {
    // S1. 前置：节 / 键非空
    if (section.empty() || key.empty()) {
        return def;
    }

    // S2.2 命中且值非空返回该值；未命中或值为空 → def
    const auto sectionIt = values_.find(section);
    if (sectionIt == values_.end()) {
        return def;
    }
    const auto keyIt = sectionIt->second.find(key);
    if (keyIt == sectionIt->second.end() || keyIt->second.empty()) {
        return def;
    }
    return keyIt->second;
}

int ConfigFile::getInt(const std::string& section, const std::string& key, int def) const {
    // S1. 前置：节 / 键非空
    if (section.empty() || key.empty()) {
        return def;
    }

    // S2.3 命中且能解析为整数 → 该值；未命中、为空、非法字面量 → def（不报错）
    const auto sectionIt = values_.find(section);
    if (sectionIt == values_.end()) {
        return def;
    }
    const auto keyIt = sectionIt->second.find(key);
    if (keyIt == sectionIt->second.end()) {
        return def;
    }
    int value = 0;
    if (!parseStrictInt(keyIt->second, value)) {
        return def;
    }
    return value;
}

bool ConfigFile::has(const std::string& section, const std::string& key) const {
    // S1. 前置：节 / 键非空
    if (section.empty() || key.empty()) {
        return false;
    }

    // S2.1 键存在即为真（**值为空串也算存在**）
    const auto sectionIt = values_.find(section);
    if (sectionIt == values_.end()) {
        return false;
    }
    return sectionIt->second.find(key) != sectionIt->second.end();
}

} // namespace sched
