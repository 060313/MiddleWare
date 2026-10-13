#include "ServiceInterface.h"

#include "RequestCodec.h"
#include "RequestDispatcher.h"

namespace service {

ServiceInterface::ServiceInterface(std::shared_ptr<agent::SchedManager> manager,
                                   std::shared_ptr<agent::SystemMonitor> monitor)
    : m_manager(std::move(manager)),
      m_monitor(std::move(monitor)),
      m_dispatcher(nullptr),
      m_codec(nullptr) {
    // TODO: m_dispatcher = make_unique<RequestDispatcher>(m_manager, m_monitor);
    //       m_codec     = make_unique<proto::RequestCodec>();
}

ServiceInterface::~ServiceInterface() = default;

bool ServiceInterface::run(const std::string& socketPath) {
    // TODO: setupListener → epoll_create1 → 事件循环
    //       （socket(AF_UNIX, SOCK_STREAM) / bind / listen / epoll_wait / accept4）
    (void)socketPath;
    return false;
}

void ServiceInterface::stop() {
    // TODO: m_running = false（并 eventfd 唤醒 epoll_wait）
}

bool ServiceInterface::setupListener(const std::string& socketPath) {
    // TODO: 创建 AF_UNIX 监听套接字；已存在则 unlink；返回是否成功
    (void)socketPath;
    return false;
}

void ServiceInterface::handleReadable(int clientFd) {
    // TODO: 读完整帧 → m_codec->decode → m_dispatcher->route → m_codec->encode → 回写
    (void)clientFd;
}

}  // namespace service
