#pragma once

// 三个运维子命令：status / mode / service。
// 见《用户界面设计文档》5.2~5.4、9.1。

#include <memory>
#include <string>
#include <vector>

#include "Command.h"
#include "OutputFormatter.h"
#include "Types.h"

namespace cli {

class CtlClient;
class SystemdCtl;

// UI2：查询并展示系统状态
class StatusCommand final : public Command {
public:
    explicit StatusCommand(std::shared_ptr<CtlClient> client);
    ~StatusCommand() override = default;

    std::string name() const override;
    std::string usage() const override;
    int execute(const std::vector<std::string>& args) override;

private:
    std::shared_ptr<CtlClient> m_client;
    OutputFormatter            m_fmt;
};

// UI3：查询 / 切换调度模式
class ModeCommand final : public Command {
public:
    explicit ModeCommand(std::shared_ptr<CtlClient> client);
    ~ModeCommand() override = default;

    std::string name() const override;
    std::string usage() const override;
    int execute(const std::vector<std::string>& args) override;

private:
    std::shared_ptr<CtlClient> m_client;
    OutputFormatter            m_fmt;
};

// UI4：委托 systemd 管理 schedd 服务
class ServiceCommand final : public Command {
public:
    explicit ServiceCommand(std::shared_ptr<SystemdCtl> ctl);
    ~ServiceCommand() override = default;

    std::string name() const override;
    std::string usage() const override;
    int execute(const std::vector<std::string>& args) override;

private:
    std::shared_ptr<SystemdCtl> m_ctl;
};

}  // namespace cli
