// SPDX-License-Identifier: MIT
// ScheddApp —— 进程装配层（无对应设计类，仅负责对象装配与生命周期）
//
// 职责：按详细设计 §4.1~§4.3 的调用链，装配 12 个设计类并驱动服务运行。
// 装配顺序（自底向上）：
//   1. FileService / ONNXruntime           （common）
//   2. CPU / NPU → SystemManager           （System 内部）
//   3. SystemInterface                     （System 门面）
//   4. SystemMonitor(systemInterface)      （Agent）
//   5. DecisionMaker(ONNXruntime)          （Agent）
//   6. DecisionExecutor                    （Agent）
//   7. schedManager(monitor, maker, exec)  （Agent 总控）
//   8. ServiceInterface(schedManager)      （Service 门面）
//
// 注：本类不属于 12 个设计类，仅为进程入口的装配与运行控制。

#ifndef SCHED_APP_SCHEDDAPP_H
#define SCHED_APP_SCHEDDAPP_H

#include <memory>
#include <string>

namespace sched {

class ServiceInterface;
class schedManager;
class SystemMonitor;
class DecisionMaker;
class DecisionExecutor;
class SystemInterface;
class SystemManager;
class CPU;
class NPU;
class UDSService;
class FileService;
class ONNXruntime;

class ScheddApp {
public:
    ScheddApp();
    // 析构在 .cpp 中定义：unique_ptr 成员为不完整类型，需完整定义才能释放
    ~ScheddApp();

    // 读取配置 → 装配全部对象 → 建立服务
    bool init(const std::string& configPath);

    // 启动服务并进入运行循环（阻塞至 stop）
    int run();

    // 请求停止运行循环
    void stop();

private:
    std::string configPath_;

    // 设计类实例（按装配顺序持有）
    std::unique_ptr<FileService> fileService_;
    std::unique_ptr<ONNXruntime> onnx_;
    std::unique_ptr<CPU> cpu_;
    std::unique_ptr<NPU> npu_;
    std::unique_ptr<SystemManager> systemManager_;
    std::unique_ptr<SystemInterface> systemInterface_;
    std::unique_ptr<SystemMonitor> systemMonitor_;
    std::unique_ptr<DecisionMaker> decisionMaker_;
    std::unique_ptr<DecisionExecutor> decisionExecutor_;
    std::unique_ptr<schedManager> schedManager_;
    std::unique_ptr<ServiceInterface> serviceInterface_;
};

} // namespace sched

#endif // SCHED_APP_SCHEDDAPP_H
