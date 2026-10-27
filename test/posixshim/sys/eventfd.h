// SPDX-License-Identifier: MIT
// POSIX 头的最小仿真件 —— 仅供 -fsyntax-only 语法检查，不参与任何构建

#ifndef SCHED_POSIXSHIM_SYS_EVENTFD_H
#define SCHED_POSIXSHIM_SYS_EVENTFD_H

#define EFD_NONBLOCK 04000

int eventfd(unsigned int initval, int flags);

#endif // SCHED_POSIXSHIM_SYS_EVENTFD_H
