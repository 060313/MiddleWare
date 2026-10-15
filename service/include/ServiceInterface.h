// SPDX-License-Identifier: MIT
// ServiceInterface —— 门面类 · Service 子系统 · public（详见详细设计 §7.1）
//
// 职责：Service 子系统对外门面（唯一直接对接界面的设计类）。
//       受理推理请求、查询系统状态、切换模型，并掌管 UDS 服务启停。
// 设计要点：
//   - schedManager 关联构造注入；
//   - socketPath 为全系统唯一存放点，调用 UDSService 时传参；
//   - taskAsk / getSystemState 为同步语义（阻塞至结果返回）。

#ifndef SCHED_SERVICE_SERVICEINTERFACE_H
#define SCHED_SERVICE_SERVICEINTERFACE_H

#include <string>

#include "common/include/SchedTypes.h"

namespace sched {

class schedManager;
class UDSService;

class ServiceInterface {
public:
    explicit ServiceInterface(schedManager* manager);
    ~ServiceInterface();

    // 受理一次推理请求 → 转交 schedManager → 返回其产出。
    TaskResult taskAsk(const TaskRequest& request);

    // 取系统特征快照 → 转交 schedManager（其经 systemMonitor 取特征）。
    SystemFeaturedState getSystemState();

    // 切换模型 → 转交 schedManager，由其后置校验并热切换。
    bool setModel(const std::string& modelName);

    // 装配 UDSService（传 socketPath）→ 监听 → 置 running。
    bool start();

    // 置 running = false → 关闭端点与连接 → 唤醒服务循环。
    void stop();

    void setSocketPath(const std::string& path) { socketPath_ = path; }
    const std::string& socketPath() const { return socketPath_; }
    bool running() const { return running_; }

private:
    schedManager* manager_ = nullptr;  // 关联，构造装配注入
    bool running_ = false;             // 服务运行标志
    std::string socketPath_;           // 全系统单存于此
    UDSService* uds_ = nullptr;        // 依赖：UDS 服务（装配期创建）
};

} // namespace sched

#endif // SCHED_SERVICE_SERVICEINTERFACE_H
