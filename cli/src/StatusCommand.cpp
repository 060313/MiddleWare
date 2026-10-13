#include "Commands.h"

#include <cstdio>

#include "CtlClient.h"

namespace cli {

StatusCommand::StatusCommand(std::shared_ptr<CtlClient> client)
    : m_client(std::move(client)) {}

std::string StatusCommand::name() const {
    return "status";
}

std::string StatusCommand::usage() const {
    return "[-w|--watch] [--json]   查询 schedd 当前系统状态";
}

int StatusCommand::execute(const std::vector<std::string>& args) {
    // TODO: 解析 --watch / --json → m_client->request(GetState, ...) →
    //       m_fmt.formatState() 打印
    (void)args;
    std::printf("schedctl status: 待实现\n");
    return 0;
}

}  // namespace cli
