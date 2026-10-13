#include "SystemdCtl.h"

namespace cli {

SystemdCtl::SystemdCtl(std::string unit) : m_unit(std::move(unit)) {}

int SystemdCtl::systemctl(const std::string& verb) {
    // TODO: popen("systemctl <verb> <unit>") 或 fork/exec，回传退出码
    (void)verb;
    return -1;
}

int SystemdCtl::start() {
    return systemctl("start");
}

int SystemdCtl::stop() {
    return systemctl("stop");
}

int SystemdCtl::restart() {
    return systemctl("restart");
}

std::string SystemdCtl::status() {
    // TODO: systemctl status schedd 的输出（供表格化展示）
    return std::string();
}

}  // namespace cli
