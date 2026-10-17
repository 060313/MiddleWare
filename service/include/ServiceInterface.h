// SPDX-License-Identifier: MIT
// serviceInterface —— 门面类 · Service 子系统 · public（详细设计 §3.1）
//
// 职责：Service 子系统对外门面（唯一直接对接界面的设计类）——受理推理请求、
//       查询系统状态、切换模型，并掌管 UDS 服务的启停与报文分派。
// 设计要点：
//   - schedManager 关联构造注入，**只持指针、不拥有**；
//   - socketPath 是**配置来源唯一**的存放点，调 UDSService::start 时传参
//     （UDSService 自持一份运行期副本供 stop() unlink，属实现必需，不是第二真相源）；
//   - UDSService 为全静态类：无实例可创建，监听与循环都以类名调静态方法；
//   - `running` 是**服务生命周期**标志，只管本门面的前置与 serve 的生命期，
//     **不承担监控线程开关**（D-36）。

#ifndef SCHED_SERVICE_SERVICEINTERFACE_H
#define SCHED_SERVICE_SERVICEINTERFACE_H

#include <string>

#include "common/include/SchedTypes.h"

namespace sched {

class schedManager;

class serviceInterface {
public:
    // socketPath：UDS 监听路径（§4.3.1 [server] socket_path）；
    // mgr：调度控制器（关联）；maxConn：并发连接上限（非正值回退 16）。
    // 只做赋值、不做 I/O——监听由 start() 建立。
    serviceInterface(const std::string& socketPath, schedManager* mgr, int maxConn);
    ~serviceInterface();

    // 受理一次推理请求：转交 schedManager::taskAsk，返回其产出
    ActionInfo taskAsk(const TaskInfo& taskInfo);

    // 查询系统状态：转交 schedManager::getSystemInfo，返回对外应答载荷
    SystemStateSnapshot getSystemInfo();

    // 切换模型（**只收完整路径**，空串表示停用推理）；转交 schedManager::setModel
    bool setModel(const std::string& modelPath);

    // 建立监听：以 socketPath / maxConn 与绑定 this 的 handleFrame 调 UDSService::start，
    // 成功才置 running。**不启循环**——循环是 serve()。
    bool start();

    // 停止服务：置 running = false → 调 UDSService::stop()（关端点、唤醒循环）。
    // **不 join**：本方法不能阻塞在一个正跑长推理的循环上，线程回收归装配层。
    void stop();

    // **服务线程入口**：转调 UDSService::serve()，阻塞至事件循环退出。
    void serve();

    const std::string& socketPath() const { return socketPath_; }
    bool running() const { return running_; }

private:
    // UDSService 的回调入口：入参只有「报文类型 + 纯载荷」，**不含 8 字节帧头**（D-27）；
    // 按类型分派到 taskAsk / getSystemInfo / setModel，结果编为四键外壳返回。
    std::string handleFrame(const std::string& type, const std::string& payload);

    // 设计文档中该属性名为 schedManager（与类同名）；C++ 里同名会遮蔽类型名，
    // 故改名为 manager_，语义与§3.1 一致
    schedManager* manager_ = nullptr;   // 关联，构造装配注入
    bool running_ = false;              // 服务运行标志
    std::string socketPath_;            // 配置来源唯一（UDSService 另持运行期副本）
    int maxConn_ = 16;                  // UDS 并发连接上限
};

} // namespace sched

#endif // SCHED_SERVICE_SERVICEINTERFACE_H
