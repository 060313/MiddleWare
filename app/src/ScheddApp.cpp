// SPDX-License-Identifier: MIT
// ScheddApp 实现（骨架 —— 待填充）
//
// TODO(实现层)：
//   - init()：解析配置（socket_path / model_dir+file / cpu_freq_path / ...）
//             → 按装配顺序 new 出全部设计类（构造注入上层依赖）
//   - run()：serviceInterface_->start() → 进入事件循环（阻塞）→ 收到停止信号退出
//   - stop()：serviceInterface_->stop() → 唤醒循环
//
// 注：本文件必须 include 全部设计类头文件——ScheddApp 以 unique_ptr 持有它们，
//     析构时需要完整类型定义。

#include "app/include/ScheddApp.h"

#include "agent/include/DecisionExecutor.h"
#include "agent/include/DecisionMaker.h"
#include "agent/include/SchedManager.h"
#include "agent/include/SystemMonitor.h"
#include "common/include/FileService.h"
#include "common/include/ONNXruntime.h"
#include "common/include/UDSService.h"
#include "service/include/ServiceInterface.h"
#include "system/include/CPU.h"
#include "system/include/NPU.h"
#include "system/include/SystemInterface.h"
#include "system/include/SystemManager.h"

namespace sched {

ScheddApp::ScheddApp() = default;

ScheddApp::~ScheddApp() = default;

bool ScheddApp::init(const std::string& configPath) {
    // TODO(实现层)
    configPath_ = configPath;
    return false;
}

int ScheddApp::run() {
    // TODO(实现层)
    return 0;
}

void ScheddApp::stop() {
    // TODO(实现层)
}

} // namespace sched
