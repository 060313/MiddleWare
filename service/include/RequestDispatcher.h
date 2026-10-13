#pragma once

// 按报文类型路由到处理器（Service 内部，类可见性 internal）。
// 见《软件详细设计文档》5.2。

#include <memory>
#include <string>

#include "Types.h"

namespace agent {
class SchedManager;
class SystemMonitor;
}  // namespace agent

namespace service {

class RequestDispatcher {
public:
    RequestDispatcher(std::shared_ptr<agent::SchedManager> manager,
                      std::shared_ptr<agent::SystemMonitor> monitor);
    ~RequestDispatcher();

    // 路由并产出应答载荷
    bool route(common::MessageType type, const std::string& payload, std::string& reply);

private:
    bool handleAskSched(const std::string& payload, std::string& reply);
    bool handleGetState(const std::string& payload, std::string& reply);
    bool handleSetMode(const std::string& payload, std::string& reply);

    std::shared_ptr<agent::SchedManager>  m_manager;
    std::shared_ptr<agent::SystemMonitor> m_monitor;
};

}  // namespace service
