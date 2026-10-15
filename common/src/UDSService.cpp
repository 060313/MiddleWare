// SPDX-License-Identifier: MIT
// UDSService 实现（骨架 —— 待填充）
//
// TODO(实现层)：
//   - start()：socket(AF_UNIX) → bind → listen → 设非阻塞/事件循环 → running = true
//   - stop()：写 eventfd 唤醒阻塞循环 → close 全部 conns → close listener → running = false
//   - route()：解析请求帧 → 调 handler_ → 组应答帧；帧级故障回可读失败体，不断连
//   - 接受连接循环：accept → conns_ 追加 → 逐帧读 → route → 回写

#include "common/include/UDSService.h"

namespace sched {

UDSService::UDSService(Handler handler) : handler_(std::move(handler)) {}

UDSService::~UDSService() {
    // TODO(实现层)：确保 stop() 被调用，释放端点
}

bool UDSService::start(const std::string& socketPath) {
    // TODO(实现层)
    (void)socketPath;
    return false;
}

void UDSService::stop() {
    // TODO(实现层)
}

std::string UDSService::route(const std::string& requestFrame) {
    // TODO(实现层)
    (void)requestFrame;
    return {};
}

} // namespace sched
