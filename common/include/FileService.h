// SPDX-License-Identifier: MIT
// FileService —— 公共基础设施类 · common 子系统 · public（详细设计 §3.3）
//
// 职责：无状态文件 I/O 工具，供 CPU / NPU / ConfigFile / ModelFile / ContractFile 读写文件。
// 设计要点：
//   - **全静态类**（§1）：不实例化、无构造与析构，仅以类名调用静态方法，故本类无属性；
//   - read 失败回空串、不抛异常；读法为「循环读到 EOF」，禁用 st_size
//     （/proc 下文件恒为 0，按大小读会得到空内容）。

#ifndef SCHED_COMMON_FILESERVICE_H
#define SCHED_COMMON_FILESERVICE_H

#include <string>

namespace sched {

class FileService {
public:
    // 全静态类：禁止实例化（构造/析构均不存在，§1）
    FileService() = delete;

    // 读全文：打开 → 循环读到 EOF → 返回；失败（含路径为空）回空串，不抛异常。
    static std::string read(const std::string& path);

    // 写全文：打开可写 → 写入 → 返回是否成功。
    static bool write(const std::string& path, const std::string& content);

    // 探测路径可访问性。
    static bool exists(const std::string& path);
};

} // namespace sched

#endif // SCHED_COMMON_FILESERVICE_H
