#include "SystemFS.h"

#include <filesystem>
#include <fstream>
#include <system_error>

namespace sched {

namespace {

std::filesystem::path toPath(const std::string& path) {
    return std::filesystem::u8path(path);
}

}  // namespace

std::optional<std::string> SystemFS::read(const std::string& path) {
    const std::filesystem::path p = toPath(path);
    std::ifstream fin(p.c_str(), std::ios::in | std::ios::binary);
    if (!fin) return std::nullopt;

    std::string content;
    for (std::string line; std::getline(fin, line); ) {
        if (!line.empty() && line.back() == '\r') line.pop_back();  // 兼容 CRLF 文本
        content += line;
        content += '\n';
    }

    // 剥掉末尾换行：sysfs 的值通常自带一个结尾 '\n'，若原样返回，
    // "powersave\n" 与 "powersave" 的字符串比较会永远不相等。
    while (!content.empty() && (content.back() == '\n' || content.back() == '\r')) {
        content.pop_back();
    }
    return content;
}

bool SystemFS::write(const std::string& path, const std::string& value) {
    const std::filesystem::path p = toPath(path);
    std::ofstream fout(p.c_str(), std::ios::binary | std::ios::out | std::ios::trunc);
    if (!fout) return false;

    fout.write(value.data(), static_cast<std::streamsize>(value.size()));
    fout.close();
    // sysfs 写入非法值时错误常常到 flush/close 阶段才暴露，所以必须查 close 之后的状态
    return static_cast<bool>(fout);
}

bool SystemFS::exists(const std::string& path) {
    std::error_code ec;
    return std::filesystem::exists(toPath(path), ec);   // 不抛异常的版本
}

std::string SystemFS::tempDir() {
    std::error_code ec;
    const std::filesystem::path p = std::filesystem::temp_directory_path(ec);
    if (ec) return std::string(".");                    // 极罕见：拿不到就退到当前目录

    std::string s = p.string();                         // UTF-8
    while (s.size() > 1 && (s.back() == '/' || s.back() == '\\')) {
        s.pop_back();   // 去掉末尾分隔符，避免拼出 "...Temp//virtual_npu"
    }
    return s;
}

}  // namespace sched
