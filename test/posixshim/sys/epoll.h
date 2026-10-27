// SPDX-License-Identifier: MIT
// POSIX 头的最小仿真件 —— 仅供 -fsyntax-only 语法检查，不参与任何构建

#ifndef SCHED_POSIXSHIM_SYS_EPOLL_H
#define SCHED_POSIXSHIM_SYS_EPOLL_H

#define EPOLLIN 0x001
#define EPOLL_CTL_ADD 1
#define EPOLL_CTL_DEL 2
#define EPOLL_CTL_MOD 3

union epoll_data {
    void* ptr;
    int fd;
    unsigned int u32;
    unsigned long long u64;
};

struct epoll_event {
    unsigned int events;
    epoll_data data;
};

int epoll_create1(int flags);
int epoll_ctl(int epfd, int op, int fd, epoll_event* event);
int epoll_wait(int epfd, epoll_event* events, int maxevents, int timeout);

#endif // SCHED_POSIXSHIM_SYS_EPOLL_H
