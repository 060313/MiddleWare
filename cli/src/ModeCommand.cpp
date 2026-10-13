#include "Commands.h"

#include <cstdio>

#include "CtlClient.h"

namespace cli {

ModeCommand::ModeCommand(std::shared_ptr<CtlClient> client)
    : m_client(std::move(client)) {}

std::string ModeCommand::name() const {
    return "mode";
}

std::string ModeCommand::usage() const {
    return "[auto|static|off]      查询或切换调度模式（off 需二次确认）";
}

int ModeCommand::execute(const std::vector<std::string>& args) {
    // TODO: 无参数 → 查询；有参数 → 校验合法值 + 二次确认 → request(SetMode, ...)
    (void)args;
    std::printf("schedctl mode: 待实现\n");
    return 0;
}

}  // namespace cli
