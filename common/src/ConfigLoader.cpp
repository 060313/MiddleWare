#include "ConfigLoader.h"

#include <filesystem>
#include <fstream>
#include <stdexcept>

namespace common {
namespace {

// path 以 UTF-8 字节串传入；Windows 上 fs::path 走宽字符 API 才能正确解释中文路径
// （与 tool/SystemFS.cpp 同一处加固，理由见该文件注释）。
std::filesystem::path toPath(const std::string& path) {
    return std::filesystem::u8path(path);
}

bool isSpace(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\v' || c == '\f';
}

std::string trim(const std::string& s) {
    std::size_t begin = 0;
    std::size_t end = s.size();
    while (begin < end && isSpace(s[begin])) {
        ++begin;
    }
    while (end > begin && isSpace(s[end - 1])) {
        --end;
    }
    return s.substr(begin, end - begin);
}

// 编辑器保存的 UTF-8 文件可能带 BOM；带 BOM 时首个键名会变成 "\xEF\xBB\xBFsocket_path"，
// 表现为"配置文件明明写了却读不出"，故在首行剥掉。
void stripBom(std::string& line) {
    if (line.size() >= 3 && static_cast<unsigned char>(line[0]) == 0xEF &&
        static_cast<unsigned char>(line[1]) == 0xBB &&
        static_cast<unsigned char>(line[2]) == 0xBF) {
        line.erase(0, 3);
    }
}

}  // namespace

bool ConfigLoader::load(const std::string& path) {
    std::ifstream fin(toPath(path), std::ios::binary);
    if (!fin) {
        // 文件不存在 / 不可读：保持 m_items 原内容，由调用方使用默认值继续。
        // 这条语义与 §7.2 一致——"配置缺失不阻塞启动"。
        return false;
    }

    std::map<std::string, std::map<std::string, std::string>> parsed;
    std::string section;

    std::string raw;
    bool firstLine = true;
    while (std::getline(fin, raw)) {
        if (firstLine) {
            firstLine = false;
            stripBom(raw);
        }
        const std::string line = trim(raw);
        if (line.empty() || line[0] == '#') {
            continue;   // 空行与整行注释
        }
        if (line.front() == '[' && line.back() == ']') {
            section = trim(line.substr(1, line.size() - 2));
            continue;
        }
        const std::size_t eq = line.find('=');
        if (eq == std::string::npos) {
            continue;   // 无法识别的行：跳过而不整体失败，避免一行笔误让所有配置失效
        }
        const std::string key = trim(line.substr(0, eq));
        const std::string value = trim(line.substr(eq + 1));
        if (key.empty()) {
            continue;
        }
        // 值为空串是合法状态：schedd.conf 的四个频率上下限正是靠"留空"表达
        // "未配置，启动时从硬件读取"（§7.2）。
        parsed[section][key] = value;
    }

    m_items.swap(parsed);
    return true;
}

std::string ConfigLoader::getString(const std::string& section,
                                    const std::string& key,
                                    const std::string& defaultValue) const {
    const auto sectionIt = m_items.find(section);
    if (sectionIt == m_items.end()) {
        return defaultValue;
    }
    const auto keyIt = sectionIt->second.find(key);
    if (keyIt == sectionIt->second.end()) {
        return defaultValue;
    }
    return keyIt->second;
}

int ConfigLoader::getInt(const std::string& section,
                         const std::string& key,
                         int defaultValue) const {
    if (!has(section, key)) {
        return defaultValue;
    }
    const std::string raw = getString(section, key, std::string());
    if (raw.empty()) {
        return defaultValue;
    }
    try {
        std::size_t consumed = 0;
        const int value = std::stoi(raw, &consumed);
        // 尾随垃圾（如 "500ms"）判为非法：宁可退回默认值，也不要静默取到半个数。
        if (consumed != raw.size()) {
            return defaultValue;
        }
        return value;
    } catch (const std::exception&) {
        return defaultValue;   // 非数字 / 越 int 范围
    }
}

bool ConfigLoader::has(const std::string& section, const std::string& key) const {
    const auto sectionIt = m_items.find(section);
    if (sectionIt == m_items.end()) {
        return false;
    }
    return sectionIt->second.find(key) != sectionIt->second.end();
}

void ConfigLoader::clear() {
    m_items.clear();
}

}  // namespace common
