#pragma once

// 主控：解析 argv、分发给命令、统一退出码。
// 见《用户界面设计文档》9.1。

#include <map>
#include <memory>
#include <string>

#include "Command.h"

namespace cli {

class CtlMain {
public:
    CtlMain();
    ~CtlMain();

    int run(int argc, char** argv);

private:
    void registerCommands();
    void printUsage() const;

    std::map<std::string, std::unique_ptr<Command>> m_commands;
};

}  // namespace cli
