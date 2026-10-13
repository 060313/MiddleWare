#include "Commands.h"

#include <cstdio>

#include "SystemdCtl.h"

namespace cli {

ServiceCommand::ServiceCommand(std::shared_ptr<SystemdCtl> ctl)
    : m_ctl(std::move(ctl)) {}

std::string ServiceCommand::name() const {
    return "service";
}

std::string ServiceCommand::usage() const {
    return "<start|stop|restart|status>   委托 systemd 管理 schedd 服务";
}

int ServiceCommand::execute(const std::vector<std::string>& args) {
    // TODO: 按 args[0] 分派 start/stop/restart/status；stop 需二次确认
    (void)args;
    std::printf("schedctl service: 待实现\n");
    return 0;
}

}  // namespace cli
