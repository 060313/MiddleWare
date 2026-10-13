#include "RequestDispatcher.h"

#include "SchedManager.h"
#include "SystemMonitor.h"

namespace service {

RequestDispatcher::RequestDispatcher(std::shared_ptr<agent::SchedManager> manager,
                                     std::shared_ptr<agent::SystemMonitor> monitor)
    : m_manager(std::move(manager)), m_monitor(std::move(monitor)) {}

RequestDispatcher::~RequestDispatcher() = default;

bool RequestDispatcher::route(common::MessageType type,
                              const std::string& payload,
                              std::string& reply) {
    switch (type) {
        case common::MessageType::AskSched: return handleAskSched(payload, reply);
        case common::MessageType::GetState: return handleGetState(payload, reply);
        case common::MessageType::SetMode:  return handleSetMode(payload, reply);
        case common::MessageType::Unknown:  break;
    }
    return false;
}

bool RequestDispatcher::handleAskSched(const std::string& payload, std::string& reply) {
    // TODO: 解析 TaskFeature → m_manager->handleTask → 序列化 ActionInfo
    (void)payload;
    (void)reply;
    return false;
}

bool RequestDispatcher::handleGetState(const std::string& payload, std::string& reply) {
    // TODO: m_monitor->getFeature → 序列化 SystemFeature
    (void)payload;
    (void)reply;
    return false;
}

bool RequestDispatcher::handleSetMode(const std::string& payload, std::string& reply) {
    // TODO: 解析模式名 → m_manager->setMode → 回写结果
    (void)payload;
    (void)reply;
    return false;
}

}  // namespace service
