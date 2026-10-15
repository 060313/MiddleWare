// SPDX-License-Identifier: MIT
// FileService —— 公共基础设施 · common 子系统（详见详细设计 §7.3）
//
// 职责：无状态文件 I/O 工具类，供 CPU / NPU 等边界类读写设备节点。
// 设计要点：无设计层属性；read 失败回空串、不抛异常。

#ifndef SCHED_COMMON_FILESERVICE_H
#define SCHED_COMMON_FILESERVICE_H

#include <string>

namespace sched {

class FileService {
public:
    FileService() = default;

    // 读全文。失败回空串，不抛异常。
    std::string read(const std::string& path);

    // 写全文。返回写入是否成功。
    bool write(const std::string& path, const std::string& content);

    // 探测路径可访问性。
    bool exists(const std::string& path);
};

} // namespace sched

#endif // SCHED_COMMON_FILESERVICE_H
