// SPDX-License-Identifier: MIT
// ServiceInterface 实现（骨架 —— 待填充）
//
// TODO(实现层)：
//   - start()：装配 UDSService（构造注入 handler = 本类报文入口，传 socketPath_）
//              → uds_->start(socketPath_) → running_ = true
//   - stop()：running_ = false → uds_->stop()
//   - taskAsk()：manager_->makeDecision(request)
//   - getSystemState()：manager_->getFeature()
//   - setModel()：manager_->setModel(modelName)
//   - 报文入口：解析请求帧 → 按命令分发（taskAsk/getSystemState/setModel）→ 组应答帧

#include "service/include/ServiceInterface.h"

#include "agent/include/SchedManager.h"
#include "common/include/UDSService.h"

namespace sched {

ServiceInterface::ServiceInterface(schedManager* manager) : manager_(manager) {}

ServiceInterface::~ServiceInterface() = default;

TaskResult ServiceInterface::taskAsk(const TaskRequest& request) {
    // TODO(实现层)
    (void)request;
    return {};
}

SystemFeaturedState ServiceInterface::getSystemState() {
    // TODO(实现层)
    return {};
}

bool ServiceInterface::setModel(const std::string& modelName) {
    // TODO(实现层)
    (void)modelName;
    return false;
}

bool ServiceInterface::start() {
    // TODO(实现层)
    return false;
}

void ServiceInterface::stop() {
    // TODO(实现层)
}

} // namespace sched
