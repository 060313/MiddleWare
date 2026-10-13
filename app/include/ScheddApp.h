#pragma once

// 守护进程主对象（设计模式：单例）。
// 见《软件详细设计文档》8。职责：装配子系统、启动监控/决策/服务线程。

#include <memory>
#include <string>

namespace common {
class ConfigLoader;
}
namespace syscore {
class SystemInterface;
}
namespace agent {
class SchedManager;
class SystemMonitor;
}
namespace service {
class ServiceInterface;
}

namespace app {

class ScheddApp {
public:
    static ScheddApp& instance();

    // 返回进程退出码
    int run(int argc, char** argv);

private:
    ScheddApp();
    ~ScheddApp();

    ScheddApp(const ScheddApp&) = delete;
    ScheddApp& operator=(const ScheddApp&) = delete;

    bool init(const std::string& configPath);

    // 监控线程主体：按 sample_interval_ms 周期采集并写共享内存
    void runMonitorLoop();

    std::unique_ptr<common::ConfigLoader>       m_config;
    std::shared_ptr<syscore::SystemInterface>    m_system;
    std::shared_ptr<agent::SystemMonitor>       m_monitor;
    std::shared_ptr<agent::SchedManager>        m_manager;
    std::shared_ptr<service::ServiceInterface>  m_service;
};

}  // namespace app
