#include "CtlMain.h"

#include <cstdio>

#include "Commands.h"
#include "CtlClient.h"
#include "SystemdCtl.h"

namespace cli {

CtlMain::CtlMain() {
    registerCommands();
}

CtlMain::~CtlMain() = default;

void CtlMain::registerCommands() {
    // TODO: 读取 socket 路径（默认 /run/schedd.sock，可被 --socket 覆盖）
    auto client = std::make_shared<CtlClient>();
    auto systemd = std::make_shared<SystemdCtl>("schedd");

    m_commands.emplace("status",  std::make_unique<StatusCommand>(client));
    m_commands.emplace("mode",    std::make_unique<ModeCommand>(client));
    m_commands.emplace("service", std::make_unique<ServiceCommand>(systemd));
}

void CtlMain::printUsage() const {
    std::printf("用法: schedctl <子命令> [参数]\n\n子命令:\n");
    for (const auto& kv : m_commands) {
        std::printf("  %-10s %s\n", kv.first.c_str(), kv.second->usage().c_str());
    }
}

int CtlMain::run(int argc, char** argv) {
    // TODO: 解析全局选项（-h/--help、--version、--json、--socket）；
    //       分发到 m_commands；未知子命令返回 2
    if (argc < 2) {
        printUsage();
        return 2;
    }
    const std::string sub = argv[1];
    auto it = m_commands.find(sub);
    if (it == m_commands.end()) {
        std::printf("未知子命令: %s\n", sub.c_str());
        printUsage();
        return 2;
    }
    std::vector<std::string> args;
    for (int i = 2; i < argc; ++i) {
        args.emplace_back(argv[i]);
    }
    return it->second->execute(args);
}

}  // namespace cli
