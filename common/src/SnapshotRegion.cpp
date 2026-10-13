#include "SnapshotRegion.h"

#include <new>

#include "StateSnapshot.h"

#ifndef _WIN32
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace common {

SnapshotRegion::~SnapshotRegion() { close(); }

bool SnapshotRegion::open(const std::string& name) {
    close();

    // 进程内对象：两个平台都可用的保底路径。
    // 返回 false —— 语义是"未落在共享内存上"，与 open() 的返回值约定一致。
    const auto attachLocal = [this]() -> bool {
        m_snap   = new StateSnapshot();
        m_shared = false;
        return false;
    };

#ifdef _WIN32
    // MinGW 无 shm_open；进程内对象对"单进程多线程"的 schedd 完全等价。
    (void)name;
    return attachLocal();
#else
    if (name.empty()) {
        return attachLocal();
    }

    const std::string path  = "/" + name;           // shm_open 的名字须以 '/' 开头
    const std::size_t bytes = sizeof(StateSnapshot);

    const int fd = ::shm_open(path.c_str(), O_RDWR | O_CREAT, 0666);
    if (fd < 0) {
        return attachLocal();
    }
    if (::ftruncate(fd, static_cast<off_t>(bytes)) != 0) {
        ::close(fd);
        return attachLocal();
    }
    void* const base = ::mmap(nullptr, bytes, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (base == MAP_FAILED) {
        ::close(fd);
        return attachLocal();
    }

    // 共享内存页是裸字节：显式构造一次，让 StateSnapshot 的默认值（seq = 0 等）生效。
    // 只归还内存页、不调用析构是安全的 —— StateSnapshot 由 atomic<uint64_t> 与标量组成，
    // 是 trivially destructible。
    m_snap   = new (base) StateSnapshot();
    m_map    = base;
    m_size   = bytes;
    m_fd     = fd;
    m_shared = true;
    return true;
#endif
}

void SnapshotRegion::close() {
#ifndef _WIN32
    if (m_map != nullptr) {
        ::munmap(m_map, m_size);
        m_map  = nullptr;
        m_size = 0;
    }
    if (m_fd >= 0) {
        ::close(m_fd);
        m_fd = -1;
    }
#endif
    if (!m_shared && m_snap != nullptr) {
        delete m_snap;
    }
    m_snap   = nullptr;
    m_shared = false;
}

}  // namespace common
