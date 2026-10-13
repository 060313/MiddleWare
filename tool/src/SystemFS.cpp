#include "SystemFS.h"

namespace sched {

std::optional<std::string> SystemFS::read(const std::string& path) {
    // TODO: std::ifstream 二进制读入并返回内容；打开失败返回 nullopt
    (void)path;
    return std::nullopt;
}

bool SystemFS::write(const std::string& path, const std::string& value) {
    // TODO: std::ofstream 截断写入；失败返回 false
    (void)path;
    (void)value;
    return false;
}

bool SystemFS::exists(const std::string& path) {
    // TODO: std::filesystem::exists（注意异常安全）
    (void)path;
    return false;
}

std::string SystemFS::tempDir() {
    // TODO: std::filesystem::temp_directory_path()，去掉末尾分隔符
    return std::string();
}

}  // namespace sched
