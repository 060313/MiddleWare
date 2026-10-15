// SPDX-License-Identifier: MIT
// schedd 进程入口
//
// TODO(实现层)：
//   - 解析命令行（--config <路径>）
//   - 安装信号处理（SIGTERM/SIGINT → ScheddApp::stop）
//   - 构造 ScheddApp → init → run

#include <iostream>
#include <string>

#include "app/include/ScheddApp.h"

int main(int argc, char** argv) {
    std::string configPath = "/etc/schedd/schedd.conf";

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--config" && i + 1 < argc) {
            configPath = argv[++i];
        } else if (arg == "-h" || arg == "--help") {
            std::cout << "用法: schedd [--config <路径>]\n";
            return 0;
        }
    }

    sched::ScheddApp app;
    // TODO(实现层)：装配并运行
    (void)app;
    (void)configPath;

    return 0;
}
