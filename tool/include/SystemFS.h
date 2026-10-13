#pragma once

// 跨平台文件读写封装（procfs / sysfs / debugfs / devfreq）。
// 既有代码资产，见《软件详细设计文档》5.4。

#include <optional>
#include <string>

namespace sched {

class SystemFS {
public:
    // 读取整个文件；失败返回 nullopt
    static std::optional<std::string> read(const std::string& path);

    // 覆盖写入；失败返回 false
    static bool write(const std::string& path, const std::string& value);

    static bool exists(const std::string& path);

    // 平台临时目录（VirtualNpu 在 x86 仿真时使用）
    static std::string tempDir();
};

}  // namespace sched
