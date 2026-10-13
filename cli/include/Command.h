#pragma once

// 命令抽象（设计模式：命令）。
// 见《软件详细设计文档》8 与《用户界面设计文档》9.1。

#include <string>
#include <vector>

namespace cli {

class Command {
public:
    virtual ~Command() = default;

    virtual std::string name() const = 0;
    virtual std::string usage() const = 0;

    // 返回进程退出码（0 成功）
    virtual int execute(const std::vector<std::string>& args) = 0;
};

}  // namespace cli
