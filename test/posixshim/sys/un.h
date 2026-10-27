// SPDX-License-Identifier: MIT
// POSIX 头的最小仿真件 —— 仅供 -fsyntax-only 语法检查，不参与任何构建

#ifndef SCHED_POSIXSHIM_SYS_UN_H
#define SCHED_POSIXSHIM_SYS_UN_H

#include <sys/socket.h>

struct sockaddr_un {
    unsigned short sun_family;
    char sun_path[108];
};

#endif // SCHED_POSIXSHIM_SYS_UN_H
