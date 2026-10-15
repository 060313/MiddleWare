// SPDX-License-Identifier: MIT
// FileService 实现（骨架 —— 待填充）
//
// TODO(实现层)：
//   - read()：ifstream 二进制读全文；打开失败回空串，不抛异常
//   - write()：ofstream 截断写；返回是否成功
//   - exists()：stat / access 探测

#include "common/include/FileService.h"

namespace sched {

std::string FileService::read(const std::string& path) {
    // TODO(实现层)
    (void)path;
    return {};
}

bool FileService::write(const std::string& path, const std::string& content) {
    // TODO(实现层)
    (void)path;
    (void)content;
    return false;
}

bool FileService::exists(const std::string& path) {
    // TODO(实现层)
    (void)path;
    return false;
}

} // namespace sched
