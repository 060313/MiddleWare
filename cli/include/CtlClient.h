#pragma once

// 边界/通信：通过 UDS 与 schedd 交互。
// 见《用户界面设计文档》9.1。

#include <string>

#include "Types.h"

namespace cli {

class CtlClient {
public:
    explicit CtlClient(std::string socketPath = "/run/schedd.sock");
    ~CtlClient();

    CtlClient(const CtlClient&) = delete;
    CtlClient& operator=(const CtlClient&) = delete;

    // 发送请求并取回应答；ok 表示本次往返是否成功
    std::string request(common::MessageType type, const std::string& payload, bool& ok);

    void setSocketPath(const std::string& path) { m_socketPath = path; }
    const std::string& socketPath() const { return m_socketPath; }

private:
    std::string m_socketPath;
    int m_fd = -1;
};

}  // namespace cli
