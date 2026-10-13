#pragma once

// Service 子系统：唯一对外通信入口（UDS），类可见性 public。
// 见《软件详细设计文档》5.2。职责：创建/绑定/监听套接字、epoll 事件循环、accept/读写。

#include <memory>
#include <string>

#include "Types.h"

namespace agent {
class SchedManager;
class SystemMonitor;
}  // namespace agent

namespace service {

class RequestDispatcher;
class RequestCodec;

class ServiceInterface {
public:
    ServiceInterface(std::shared_ptr<agent::SchedManager> manager,
                     std::shared_ptr<agent::SystemMonitor> monitor);
    ~ServiceInterface();

    ServiceInterface(const ServiceInterface&) = delete;
    ServiceInterface& operator=(const ServiceInterface&) = delete;

    // 绑定并监听 UDS，随后进入 epoll 事件循环（阻塞直到 stop()）
    bool run(const std::string& socketPath);

    // 请求停止事件循环（供信号处理器调用）
    void stop();

private:
    bool setupListener(const std::string& socketPath);
    void handleReadable(int clientFd);

    std::shared_ptr<agent::SchedManager>  m_manager;
    std::shared_ptr<agent::SystemMonitor> m_monitor;
    std::unique_ptr<RequestDispatcher>    m_dispatcher;
    std::unique_ptr<RequestCodec>         m_codec;
    int  m_listenFd = -1;    // UDS 监听描述符
    int  m_epollFd  = -1;    // epoll 实例
    bool m_running  = false;
};

}  // namespace service
