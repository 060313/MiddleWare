// SPDX-License-Identifier: MIT
// POSIX 头的最小仿真件 —— **仅供 -fsyntax-only 语法检查**，不参与任何构建
//
// 用途：UDSService.cpp 依赖 AF_UNIX / epoll / eventfd，非 POSIX 平台（Windows）编不过。
//       本目录以同名头提供**签名一致**的声明，让 make check-posix 能在任意机器上
//       验证该文件的语法与类型使用；运行期行为仍须在 openEuler 容器 / 真机上验证。
// 口径：只声明本工程实际用到的符号，不做任何实现。

#ifndef SCHED_POSIXSHIM_SYS_SOCKET_H
#define SCHED_POSIXSHIM_SYS_SOCKET_H

#include <cstddef>
#include <sys/types.h>

#define AF_UNIX 1
#define SOCK_STREAM 1

struct sockaddr {
    unsigned short sa_family;
    char sa_data[14];
};

using socklen_t = unsigned int;
using ssize_t = long long;

int socket(int domain, int type, int protocol);
int bind(int fd, const sockaddr* addr, socklen_t len);
int listen(int fd, int backlog);
int accept(int fd, sockaddr* addr, socklen_t* len);
ssize_t recv(int fd, void* buf, std::size_t n, int flags);
ssize_t send(int fd, const void* buf, std::size_t n, int flags);

#endif // SCHED_POSIXSHIM_SYS_SOCKET_H
