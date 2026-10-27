// SPDX-License-Identifier: MIT
// POSIX 头的最小仿真件 —— 仅供 -fsyntax-only 语法检查，不参与任何构建
//
// 先接续系统头（保证不遮蔽平台自带声明），再补齐本工程用到、而仿真平台缺失的符号。

#ifndef SCHED_POSIXSHIM_FCNTL_H
#define SCHED_POSIXSHIM_FCNTL_H

#include_next <fcntl.h>

#ifndef F_GETFL
#define F_GETFL 3
#endif
#ifndef F_SETFL
#define F_SETFL 4
#endif
#ifndef O_NONBLOCK
#define O_NONBLOCK 0x4000
#endif

#ifndef SCHED_POSIXSHIM_FCNTL_DECLARED
#define SCHED_POSIXSHIM_FCNTL_DECLARED
extern "C" int fcntl(int fd, int cmd, ...);
#endif

#endif // SCHED_POSIXSHIM_FCNTL_H
