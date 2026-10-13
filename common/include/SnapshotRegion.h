#pragma once

// 状态快照的存放区域 —— 回答"m_snap 这个指针到底由谁分配、指向哪"。
// 见《软件详细设计文档》7.4。
//
//   · Linux（openEuler / aarch64）：shm_open + ftruncate + mmap，
//     落 /dev/shm/<name>，进程外（调试工具、未来的 schedctl 直读）可见；
//   · Windows（MinGW，x86 仿真）：MinGW 未提供 POSIX 共享内存，自动退化为
//     进程内对象。schedd 本就是单进程多线程，进程内对象 + seqlock 已提供
//     同等的线程间一致性，仿真阶段行为不受影响。
//
// 职责边界：本类只管"内存从哪来"，不参与 seqlock 协议（那是 StateSnapshot 的事）。

#include <cstddef>
#include <string>

namespace common {

struct StateSnapshot;

class SnapshotRegion {
public:
    SnapshotRegion() = default;
    ~SnapshotRegion();

    SnapshotRegion(const SnapshotRegion&) = delete;
    SnapshotRegion& operator=(const SnapshotRegion&) = delete;

    // 挂接（或创建）名为 name 的快照区域；name 为空 → 直接用进程内对象。
    // 返回值：true = 确实落在共享内存上；false = 已自动退化为进程内对象。
    // 两种情况下 get() 都非空 —— 调用方【无需】把它当作失败路径处理，
    // 返回值仅用于日志/诊断（例如"仿真环境未启用共享内存"）。
    bool open(const std::string& name);

    // 解除映射 / 释放进程内对象；可重复调用。
    void close();

    StateSnapshot* get() const { return m_snap; }

    // 当前是否真的落在共享内存上
    bool shared() const { return m_shared; }

private:
    StateSnapshot* m_snap   = nullptr;
    void*          m_map    = nullptr;   // mmap 基址（仅 Linux 共享路径）
    std::size_t    m_size   = 0;
    int            m_fd     = -1;
    bool           m_shared = false;
};

}  // namespace common
