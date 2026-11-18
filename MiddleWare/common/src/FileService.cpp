// SPDX-License-Identifier: MIT
// FileService 实现（详细设计 §3.3）
//
// 算法要点：
//   - read 必须**循环读到 EOF**：/proc（如 /proc/stat）与 sysfs 下 st_size 恒为 0，
//     按文件大小分配缓冲会读到空内容；
//   - 单文件读取上限 1 MiB，防上游误传大文件把内存打满；
//   - 一律不抛异常：失败回空串 / false，由调用方按「零值兜底」处理（D-16）。

#include "common/include/FileService.h"

#include <cstddef>
#include <fstream>

namespace sched {

namespace {
constexpr std::size_t kChunkSize = 4096;              // 每次读取块大小
constexpr std::size_t kMaxFileSize = 1024 * 1024;     // 单次读取上限 1 MiB
} // namespace

std::string FileService::read(const std::string& path) {
    // S1. 前置：路径非空
    if (path.empty()) {
        return std::string();
    }

    // S2. 打开二进制流（sysfs 节点虽为文本，按字节读最稳）
    std::ifstream ifs(path, std::ios::in | std::ios::binary);
    if (!ifs.is_open()) {
        return std::string();
    }

    // S3. 循环读到 EOF（不看 st_size）
    std::string content;
    char buf[kChunkSize];
    while (ifs.good() && content.size() < kMaxFileSize) {
        ifs.read(buf, static_cast<std::streamsize>(kChunkSize));
        const std::streamsize got = ifs.gcount();
        if (got <= 0) {
            break;
        }
        content.append(buf, static_cast<std::size_t>(got));
    }

    // S4. 返回；空文件即空串，同属「读取失败」语义
    return content;
}

bool FileService::write(const std::string& path, const std::string& content) {
    // S1. 前置：路径非空
    if (path.empty()) {
        return false;
    }

    // S2. 打开可写（截断）→ 写入 → 关闭
    std::ofstream ofs(path, std::ios::out | std::ios::binary | std::ios::trunc);
    if (!ofs.is_open()) {
        return false;
    }
    ofs.write(content.data(), static_cast<std::streamsize>(content.size()));
    ofs.flush();

    // S3. 返回写入结果（失败可能是磁盘满 / 节点拒绝写入）
    return ofs.good();
}

bool FileService::exists(const std::string& path) {
    // S1. 前置：路径非空
    if (path.empty()) {
        return false;
    }

    // S2. 以「能否打开」判可访问性（不区分文件 / 目录 / 节点）
    std::ifstream ifs(path, std::ios::in | std::ios::binary);
    return ifs.is_open();
}

} // namespace sched
