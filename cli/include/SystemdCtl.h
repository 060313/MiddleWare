#pragma once

// 边界/委托：封装 systemctl 调用（schedd 生命周期由 systemd 托管）。
// 见《用户界面设计文档》5.4、9.1。

#include <string>

namespace cli {

class SystemdCtl {
public:
    explicit SystemdCtl(std::string unit = "schedd");
    ~SystemdCtl() = default;

    int start();
    int stop();
    int restart();
    std::string status();

private:
    int systemctl(const std::string& verb);

    std::string m_unit;
};

}  // namespace cli
