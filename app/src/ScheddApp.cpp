#include "ScheddApp.h"

#include <cstdio>

#include "ConfigLoader.h"
#include "SchedManager.h"
#include "ServiceInterface.h"
#include "SystemInterface.h"
#include "SystemMonitor.h"

namespace app {

ScheddApp& ScheddApp::instance() {
    static ScheddApp inst;
    return inst;
}

ScheddApp::ScheddApp() = default;
ScheddApp::~ScheddApp() = default;

bool ScheddApp::init(const std::string& configPath) {
    // TODO: 1) m_config->load(configPath)
    //       2) 构造 SystemInterface（读档位上下限）
    //       3) 构造 SystemMonitor / SchedManager（model_path、contract_path）
    //       4) 构造 ServiceInterface（socket_path）
    (void)configPath;
    return false;
}

void ScheddApp::runMonitorLoop() {
    // TODO: while 运行中：SystemInterface::getInfo() 写共享内存；sleep sample_interval_ms
}

int ScheddApp::run(int argc, char** argv) {
    // TODO: 解析 --config；init()；起监控线程；ServiceInterface::run() 阻塞；优雅退出
    std::printf("schedd 0.1.0 (骨架阶段，主流程待实现) argc=%d\n", argc);
    (void)argv;
    return 0;
}

}  // namespace app
