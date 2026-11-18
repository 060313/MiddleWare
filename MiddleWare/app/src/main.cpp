// SPDX-License-Identifier: MIT
// schedd 进程入口（详细设计 §6.1 步 1~14 / D-22 / D-23）
//
// 本文件只做三件事：解析命令行 → 装信号处理 → 交给 ScheddApp 装配与运行。
// 它**不属于 §1 的 13 个设计类**，也不进 §3.14 的边表。

#include <csignal>
#include <iostream>
#include <string>

#include "app/include/ScheddApp.h"
#include "common/include/SchedDefs.h"

namespace {

// 信号处理函数只能碰异步信号安全的东西：这里仅调用 ScheddApp::stop()（一次原子写）
sched::ScheddApp* g_app = nullptr;

void handleSignal(int /*signo*/) {
    if (g_app != nullptr) {
        g_app->stop();
    }
}

void printUsage() {
    std::cout << "用法: schedd [--config <路径>]\n"
              << "  --config <路径>  指定配置文件，缺省为宏 CONFIG_PATH = " << CONFIG_PATH << '\n'
              << "  -h, --help       显示本帮助\n";
}

} // namespace

int main(int argc, char** argv) {
    // 配置文件路径：默认取宏 CONFIG_PATH，命令行可覆盖（覆盖后不再使用宏，D-22）
    std::string configPath = CONFIG_PATH;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--config" && i + 1 < argc) {
            configPath = argv[++i];
        } else if (arg == "-h" || arg == "--help") {
            printUsage();
            return 0;
        } else {
            std::cerr << "未知参数: " << arg << '\n';
            printUsage();
            return 2;
        }
    }

    sched::ScheddApp app;
    g_app = &app;

    // 先装信号处理，再做装配——装配可能较慢（模型加载），期间收到 SIGTERM 也应能停
    std::signal(SIGTERM, handleSignal);
    std::signal(SIGINT, handleSignal);

    if (!app.init(configPath)) {
        std::cerr << "[schedd] 装配失败\n";
        g_app = nullptr;
        return 1;
    }

    const int code = app.run();
    g_app = nullptr;
    return code;
}
