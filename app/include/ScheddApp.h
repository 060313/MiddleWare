// SPDX-License-Identifier: MIT
// ScheddApp —— 进程装配层（**无对应设计类**，不进 §1 清单与 §3.14 边表）
//
// 依据：详细设计 §6.1（装配 14 步）/ §6.2（两线程）/ §6.3（停机 6 步）/ D-23 / D-35 / D-36。
// 职责：读配置 → 按拓扑序装配全部设计类 → 起两线程 → 等信号 → 依序停机。
//
// 一条贯穿全局的约束：**装配期不读设备节点、不建监听、不加载模型**
// （构造只做赋值，故装配本身不可失败），三者分别推迟到步 8、步 12 与首次 getCPUInfo/getNPUInfo。

#ifndef SCHED_APP_SCHEDDAPP_H
#define SCHED_APP_SCHEDDAPP_H

#include <atomic>
#include <memory>
#include <string>
#include <thread>

namespace sched {

class serviceInterface;
class schedManager;
class systemMonitor;
class decisionMaker;
class decisionExecutor;
class systemInterface;
class systemManager;

class ScheddApp {
public:
    ScheddApp();
    // 析构在 .cpp 中定义：unique_ptr 成员为不完整类型，需完整定义才能释放
    ~ScheddApp();

    // 步 1~9：读配置 → 自底向上构造 → 首次加载模型 → 构造服务门面
    bool init(const std::string& configPath);

    // 步 10~14 + 停机：装载定时器 → 起监控线程 → 建监听 → 起服务线程 → 等信号 → 依次停机
    int run();

    // 请求停机（信号处理函数调用）：只置标志，真正的清理在 run() 里按序做
    void stop();

private:
    // 监控线程循环体：poll 推进时间 + 短睡（**不 sleep 会 100% 占满一个核**）
    void monitorLoop();

    std::string configPath_;

    int sampleIntervalMs_ = 500;             // 采样间隔（§4.3.3 [system] sample_interval_ms）
    std::atomic<bool> monitorRun_{false};    // 监控线程循环条件（D-36：归装配层，不入 Timer）
    std::atomic<bool> stopRequested_{false}; // 停机请求标志

    std::thread monitorThread_;
    std::thread serviceThread_;

    // ---------------------------------------------------------------------
    // 声明顺序 = 析构顺序的**逆序**（C++ 保证按声明逆序销毁）。
    // 目标销毁顺序（§6.3 步 5）：serviceInterface → schedManager → decisionMaker /
    // decisionExecutor → systemMonitor → systemInterface → systemManager
    // （末者析构时 delete 其 CPU / NPU）。
    // ---------------------------------------------------------------------
    std::unique_ptr<systemManager> systemManager_;
    std::unique_ptr<systemInterface> systemInterface_;
    std::unique_ptr<systemMonitor> systemMonitor_;
    std::unique_ptr<decisionMaker> decisionMaker_;
    std::unique_ptr<decisionExecutor> decisionExecutor_;
    std::unique_ptr<schedManager> schedManager_;
    std::unique_ptr<serviceInterface> serviceInterface_;
};

} // namespace sched

#endif // SCHED_APP_SCHEDDAPP_H
