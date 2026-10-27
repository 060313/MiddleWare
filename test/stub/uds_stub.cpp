// SPDX-License-Identifier: MIT
// UDSService 桩实现（配套 test/stub/uds_stub.h）

#include "test/stub/uds_stub.h"

#include <utility>
#include <vector>

namespace sched {

namespace stub {

UdsState& uds() {
    static UdsState state;
    return state;
}

} // namespace stub

UDSService::Socket UDSService::listener_ = -1;
int UDSService::epfd_ = -1;
int UDSService::wakeFd_ = -1;
std::vector<UDSService::Conn> UDSService::conns_;
std::string UDSService::socketPath_;
std::atomic<bool> UDSService::running_{false};
UDSService::Handler UDSService::handler_ = nullptr;
int UDSService::maxConn_ = 16;

bool UDSService::start(const std::string& socketPath, int maxConn, Handler handler) {
    ++stub::uds().startCalls;
    if (running_.load() || socketPath.empty() || !handler) {
        return false;
    }
    socketPath_ = socketPath;
    maxConn_ = (maxConn > 0) ? maxConn : 16;
    running_.store(true);

    // 先给桩留一份回调副本（handleFrame 是 private，测试只能经它进入），再移交给静态成员
    stub::uds().socketPath = socketPath;
    stub::uds().maxConn = maxConn_;
    stub::uds().hasHandler = true;
    stub::uds().handler = handler;
    handler_ = std::move(handler);
    return stub::uds().startOk;
}

void UDSService::stop() {
    ++stub::uds().stopCalls;
    running_.store(false);
}

void UDSService::serve() {
    ++stub::uds().serveCalls;
}

std::string UDSService::route(const std::string& requestFrame) {
    (void)requestFrame;
    return std::string();   // 帧级处理归真实实现，桩不参与
}

} // namespace sched
