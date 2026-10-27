// SPDX-License-Identifier: MIT
// UDSService 的链接期桩：只记录调用、不建真端点
//
// 用途：serviceInterface 的报文分派与协议编解码与 socket 无关，但链接期需要
//       UDSService 的符号。测试链路用本桩替代 common/src/UDSService.cpp
//       （后者依赖 epoll / AF_UNIX，非 POSIX 平台编不过），并把 start 注入的
//       handler 暴露出来——handler 是 private 方法 handleFrame 的唯一入口。

#ifndef SCHED_TEST_UDS_STUB_H
#define SCHED_TEST_UDS_STUB_H

#include <string>

#include "common/include/UDSService.h"

namespace sched {
namespace stub {

struct UdsState {
    bool startOk = true;              // start 的成败开关
    int startCalls = 0;
    int stopCalls = 0;
    int serveCalls = 0;
    std::string socketPath;           // 最近一次 start 的监听路径
    int maxConn = 0;
    bool hasHandler = false;
    UDSService::Handler handler;      // start 注入的回调（即 handleFrame）

    void reset() { *this = UdsState(); }
};

UdsState& uds();

} // namespace stub
} // namespace sched

#endif // SCHED_TEST_UDS_STUB_H
