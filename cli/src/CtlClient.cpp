#include "CtlClient.h"

namespace cli {

CtlClient::CtlClient(std::string socketPath) : m_socketPath(std::move(socketPath)) {}

CtlClient::~CtlClient() {
    // TODO: 关闭 m_fd
}

std::string CtlClient::request(common::MessageType type,
                               const std::string& payload,
                               bool& ok) {
    // TODO: socket(AF_UNIX, SOCK_STREAM) → connect → 组帧发送 → 读应答 → 解帧
    (void)type;
    (void)payload;
    ok = false;
    return std::string();
}

}  // namespace cli
