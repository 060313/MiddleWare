// SPDX-License-Identifier: MIT
// ScheddApp 实现（详细设计 §6.1 / §6.2 / §6.3）
//
// 装配拓扑是一条单链，没有环：
//   systemManager → systemInterface → systemMonitor → decisionExecutor ┐
//                                                                      ├→ schedManager → serviceInterface
//                                       decisionMaker ─────────────────┘
// CPU / NPU 由 systemManager 在构造内创建（组合），装配层不持有它们。

#include "app/include/ScheddApp.h"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <utility>

#include "agent/include/DecisionExecutor.h"
#include "agent/include/DecisionMaker.h"
#include "agent/include/SchedManager.h"
#include "agent/include/SystemMonitor.h"
#include "common/include/SchedDefs.h"
#include "common/include/Timer.h"
#include "data/include/ConfigFile.h"
#include "service/include/ServiceInterface.h"
#include "system/include/SystemInterface.h"
#include "system/include/SystemManager.h"

namespace sched {

namespace {

constexpr int kDefaultMaxConn = 16;
constexpr int kDefaultSampleMs = 500;
constexpr int kDefaultPollDivisor = 10;   // 监控线程睡眠 = 采样间隔 / 10

} // namespace

ScheddApp::ScheddApp() = default;

ScheddApp::~ScheddApp() {
    // 兜底：run() 正常退出时线程已 join，这里只处理异常路径，避免 std::terminate
    monitorRun_.store(false);
    if (serviceThread_.joinable()) {
        serviceThread_.join();
    }
    if (monitorThread_.joinable()) {
        monitorThread_.join();
    }
    // 成员 unique_ptr 按声明逆序销毁，即 §6.3 步 5 的顺序
}

bool ScheddApp::init(const std::string& configPath) {
    configPath_ = configPath;

    // ---- 步 1：栈上读配置（取完即销毁，不进运行期对象图，D-23）----
    ConfigFile config;
    if (!config.configValidate(configPath_)) {
        // 校验失败**不阻塞启动**：缺失键一律取默认值，由降级链兜底（D-23）
        std::cerr << "[schedd] 配置校验未通过（节/键缺失），缺失项按默认值处理: "
                  << configPath_ << '\n';
    }
    if (!config.getConfig(configPath_)) {
        // 读不到配置文件同样不阻塞：全部键退化为各自默认值
        std::cerr << "[schedd] 配置文件读取失败，全部取默认值: " << configPath_ << '\n';
    }

    const std::string socketPath = config.getString("server", "socket_path", "/run/schedd.sock");
    const int maxConn = config.getInt("server", "max_conn", kDefaultMaxConn);
    const std::string modelFile = config.getString("agent", "model_file", "");
    const int cpuFreqMin = config.getInt("system", "cpu_freq_min", 0);
    const int cpuFreqMax = config.getInt("system", "cpu_freq_max", 0);
    const int npuFreqMin = config.getInt("system", "npu_freq_min", 0);
    const int npuFreqMax = config.getInt("system", "npu_freq_max", 0);
    const std::string cpuFreqPath = config.getString("system", "cpu_freq_path", "");
    const std::string npuFreqPath = config.getString("system", "npu_freq_path", "");
    const std::string procStatPath = config.getString("system", "proc_stat_path", "");
    const int sampleMs = config.getInt("system", "sample_interval_ms", kDefaultSampleMs);

    sampleIntervalMs_ = (sampleMs > 0) ? sampleMs : kDefaultSampleMs;

    // ---- 步 2~7：自底向上构造（构造只赋值、不 I/O，故不可失败）----
    systemManager_ = std::make_unique<systemManager>(cpuFreqPath, npuFreqPath, procStatPath,
                                                    cpuFreqMin, cpuFreqMax,
                                                    npuFreqMin, npuFreqMax);
    systemInterface_ = std::make_unique<systemInterface>(systemManager_.get());
    systemMonitor_ = std::make_unique<systemMonitor>(systemInterface_.get());
    decisionExecutor_ = std::make_unique<decisionExecutor>(systemMonitor_.get());

    // 步 6：模型完整路径 = 宏 MODEL_DIR（含结尾 '/'）+ model_file；留空即「无模型」
    const std::string modelPath =
        modelFile.empty() ? std::string() : (std::string(MODEL_DIR) + modelFile);
    decisionMaker_ = std::make_unique<decisionMaker>(modelPath);

    // 步 7：model 取形参缺省值 "default"
    schedManager_ = std::make_unique<schedManager>(systemMonitor_.get(),
                                                  decisionMaker_.get(),
                                                  decisionExecutor_.get());

    // ---- 步 8：首次加载模型（直调控制器，不经门面——门面的 running 前置在装配期必拒，D-11）----
    //   失败不阻塞启动：model 置空串（停用），taskAsk 一律回 2001，等人处理
    if (!schedManager_->setModel(modelPath)) {
        std::cerr << "[schedd] 模型加载失败，推理停用: " << modelPath << '\n';
    }

    // ---- 步 9：服务门面（依赖 schedManager）----
    serviceInterface_ = std::make_unique<serviceInterface>(socketPath, schedManager_.get(), maxConn);

    return true;
}

int ScheddApp::run() {
    // 前置：装配已完成
    if (!serviceInterface_ || !schedManager_ || !systemManager_) {
        std::cerr << "[schedd] 尚未完成装配，无法运行\n";
        return 1;
    }

    // ---- 步 10：装载监控节拍（回调内容 = 刷一次 SystemInfo 缓存）----
    //   Timer 不建线程，节拍要人工推进（§3.5）
    if (!Timer::start(sampleIntervalMs_, [this]() {
            if (systemManager_) {
                systemManager_->refreshSystemInfo();
            }
        })) {
        std::cerr << "[schedd] 定时器装载失败\n";
        return 1;
    }

    // ---- 步 11：起监控线程（只写缓存，尚不对外）----
    monitorRun_.store(true);
    monitorThread_ = std::thread(&ScheddApp::monitorLoop, this);

    // ---- 步 12：建监听（对外可见的最后一步；模型在步 8 已就绪）----
    if (!serviceInterface_->start()) {
        std::cerr << "[schedd] UDS 监听建立失败: " << serviceInterface_->socketPath() << '\n';
        monitorRun_.store(false);
        if (monitorThread_.joinable()) {
            monitorThread_.join();
        }
        Timer::stop();
        return 1;
    }

    // ---- 步 13：起服务线程（accept 时对象图已完整）----
    serviceThread_ = std::thread([this]() {
        if (serviceInterface_) {
            serviceInterface_->serve();
        }
    });

    // ---- 步 14：等信号（stop() 只置标志，真正的清理在下面按序做）----
    while (!stopRequested_.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    // ================= §6.3 停机：先停服务、再停监控、最后析构 =================
    // 步 1：置 running=false → 关 listener → 写 wakeFd 唤醒 epoll_wait（不 join）
    serviceInterface_->stop();

    // 步 2：监控线程循环条件置假（D-36）
    monitorRun_.store(false);

    // 步 3：join 服务线程——必须早于析构，让在飞的 taskAsk 走完
    if (serviceThread_.joinable()) {
        serviceThread_.join();
    }

    // 步 4：join 监控线程——必须早于析构，否则它会解引用已释放的 systemManager
    if (monitorThread_.joinable()) {
        monitorThread_.join();
    }

    // 步 5~6：逆序析构（由成员声明顺序保证）与进程退出
    Timer::stop();
    return 0;
}

void ScheddApp::stop() {
    // 信号处理函数可直接调用：只做一次原子写入，无锁、无 I/O、异步信号安全
    stopRequested_.store(true);
}

void ScheddApp::monitorLoop() {
    // 睡眠取采样间隔的 1/10（默认 50 ms）：Timer::poll 自己**不睡眠**，
    // 若写成 while(poll()) 的空转循环会 100% 占满一个核
    const int sleepMs =
        std::max(1, sampleIntervalMs_ / kDefaultPollDivisor);

    while (monitorRun_.load()) {
        Timer::poll();
        std::this_thread::sleep_for(std::chrono::milliseconds(sleepMs));
    }
}

} // namespace sched
