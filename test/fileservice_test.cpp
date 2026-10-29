// SPDX-License-Identifier: MIT
// FileService 单元测试（详细设计 §3.3）
//
// 重点覆盖三处「实现容易踩、设计已明令」的地方：
//   - read 必须循环读到 EOF：/proc 与 sysfs 下 st_size 恒为 0，按大小读会得到空串；
//   - 单次读取上限 1 MiB；
//   - 失败一律返回空串 / false，**不抛异常**。

#include <unistd.h>

#include <fstream>
#include <string>
#include <vector>

#include "common/include/FileService.h"
#include "test_util.h"

using sched::FileService;

namespace {

constexpr std::size_t kMaxFileSize = 1024 * 1024;

std::string tmpPath(const char* tag) {
    return std::string("/tmp/schedd_fs_") + tag + "_" + std::to_string(::getpid()) + ".txt";
}

} // namespace

TEST_CASE(read_proc_stat_non_empty) {
    // /proc/stat 的 st_size 恒为 0——能读到内容即证明走了「循环读到 EOF」而非按大小读
    const std::string content = FileService::read("/proc/stat");
    CHECK_FALSE(content.empty());
    CHECK_EQ(content.compare(0, 3, "cpu"), 0);
}

TEST_CASE(read_missing_file_returns_empty) {
    CHECK_TRUE(FileService::read("/tmp/schedd_definitely_not_here_9527").empty());
}

TEST_CASE(read_empty_path_returns_empty) {
    CHECK_TRUE(FileService::read("").empty());
}

TEST_CASE(read_directory_returns_empty_or_short) {
    // 目录不可按文件读：不崩、不抛即可（返回空串）
    const std::string content = FileService::read("/tmp");
    CHECK_TRUE(content.empty());
}

TEST_CASE(write_then_read_roundtrip) {
    const std::string path = tmpPath("roundtrip");
    const std::string payload = "hello schedd\nline2\n";

    CHECK_TRUE(FileService::write(path, payload));
    CHECK_TRUE(FileService::exists(path));
    CHECK_EQ(FileService::read(path), payload);

    ::unlink(path.c_str());
    CHECK_FALSE(FileService::exists(path));
}

TEST_CASE(write_empty_content_truncates) {
    const std::string path = tmpPath("empty");
    CHECK_TRUE(FileService::write(path, "something"));
    CHECK_TRUE(FileService::write(path, ""));

    const std::string content = FileService::read(path);
    CHECK_TRUE(content.empty());

    ::unlink(path.c_str());
}

TEST_CASE(write_to_missing_directory_fails) {
    CHECK_FALSE(FileService::write("/tmp/schedd_no_such_dir_9527/f.txt", "x"));
    CHECK_FALSE(FileService::write("", "x"));
}

TEST_CASE(read_is_capped_at_1mib) {
    // 写入 1 MiB + 100 字节，读取应恰好在 1 MiB 处截断（§3.3 S3）
    const std::string path = tmpPath("big");
    const std::string payload(kMaxFileSize + 100, 'x');
    CHECK_TRUE(FileService::write(path, payload));

    const std::string content = FileService::read(path);
    CHECK_EQ(content.size(), kMaxFileSize);

    ::unlink(path.c_str());
}

TEST_CASE(exists_handles_dir_and_empty_path) {
    CHECK_TRUE(FileService::exists("/tmp"));
    CHECK_TRUE(FileService::exists("/proc/stat"));
    CHECK_FALSE(FileService::exists("/tmp/schedd_definitely_not_here_9527"));
    CHECK_FALSE(FileService::exists(""));
}

TEST_MAIN()
