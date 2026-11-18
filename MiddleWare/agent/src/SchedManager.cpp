// SPDX-License-Identifier: MIT
// schedManager 实现（详细设计 §3.6 / D-08 / D-09 / D-10 / D-11 / D-34）
//
// taskAsk 是同步方法（D-01），推理就在服务线程内完成，不另起决策线程（D-35）。

#include "agent/include/SchedManager.h"

#include <string>

#include "agent/include/DecisionExecutor.h"
#include "agent/include/DecisionMaker.h"
#include "agent/include/SystemMonitor.h"
#include "common/include/SchedDefs.h"

namespace sched {

namespace {

// 连续推理失败阈值 N（§3.6 S2 / D-08）；内部固化，不由配置提供
constexpr int kFailThreshold = 3;

// 默认模型文件名（D-08 降级第①级）
constexpr const char* kDefaultModelFile = "default.onnx";

// 从完整路径解析模型名：取末段、去掉最后一个 '.' 及其后缀
std::string parseModelName(const std::string& modelPath) {
    const std::string::size_type slash = modelPath.find_last_of('/');
    const std::string base =
        (slash == std::string::npos) ? modelPath : modelPath.substr(slash + 1);
    const std::string::size_type dot = base.find_last_of('.');
    return (dot == std::string::npos) ? base : base.substr(0, dot);
}

} // namespace

schedManager::schedManager(systemMonitor* monitor,
                           decisionMaker* maker,
                           decisionExecutor* executor,
                           const std::string& model)
    : monitor_(monitor), maker_(maker), executor_(executor), model_(model), failCount_(0) {
    // 只做赋值、不做 I/O，也不加载模型（D-11）；形参默认值 "default" 即初值来源
}

schedManager::~schedManager() = default;
// 平凡析构：三个引用一律不 delete（所有者在装配层）

ActionInfo schedManager::taskAsk(const TaskInfo& taskInfo) {
    // S1. 取系统状态缓存（该层永不失败，未装配时回零值快照）
    SystemInfo sysInfo;
    if (monitor_ != nullptr) {
        sysInfo = monitor_->getSystemInfo();
    }

    // S2. 停用判断：model 为空串 → 不下发推理，且**不计 failCount**
    if (model_.empty()) {
        return makeFailure(Reason::modelNotReady, "model not loaded");
    }

    // S3. 就绪判断：会话未建 → 同样回 2001，但**不调 handleDecideFailure**
    //     （没发生推理就不算推理失败，否则「未就绪」会被误判成「推理坏了」）
    if (maker_ == nullptr || !maker_->isReady()) {
        return makeFailure(Reason::modelNotReady, "model not ready");
    }

    // S4. 推理；返回未选中任何处理器的 Decision 即推理失败
    const Decision decision = maker_->makeDecision(taskInfo, sysInfo);
    if (!decision.valid) {
        handleDecideFailure();
        return makeFailure(Reason::inferenceFailed, "inference failed");
    }

    // S5. 推理成功 → 重置连续失败计数
    failCount_ = 0;

    // S6. 下发唯一动作；执行失败**不回退 failCount、不触发降级**（D-34）
    if (executor_ == nullptr) {
        return makeFailure(Reason::schedulerAbsent, "executor not ready");
    }
    const ActionInfo info = executor_->execDecision(decision);

    // S7. 原样返回；ok / message / reason 已由 result 镜像，不二次加工
    return info;
}

SystemStateSnapshot schedManager::getSystemInfo() {
    // S1. 状态分量即 SystemInfo 本体
    SystemStateSnapshot snapshot;
    if (monitor_ != nullptr) {
        snapshot.system = monitor_->getSystemInfo();
    }

    // S2. 附当前模型名；无副作用
    snapshot.model = model_;
    return snapshot;
}

std::string schedManager::getModel() const {
    return model_;
}

bool schedManager::setModel(const std::string& modelPath) {
    // S1. 停用分流：空串 → 停用推理，**不改动 decisionMaker**（会话与路径保留）
    if (modelPath.empty()) {
        model_.clear();
        return true;
    }

    // S2. 可用性判断：maker 未装配，或路径以 '/' 结尾（末段为空、解析不出模型名）
    if (maker_ == nullptr) {
        return false;
    }
    if (modelPath.back() == '/') {
        return false;
    }

    // S3. 先建后换：由 decisionMaker 完成「校验 → 建新会话 → 载契约 → 成功才替换」
    if (!maker_->setModelPath(modelPath)) {
        // S3.1 失败 → model 原封不动（旧模型仍在服务）
        return false;
    }

    // S4. 模型名解析（.../dqn.onnx -> dqn）
    model_ = parseModelName(modelPath);

    // S5.
    return true;
}

void schedManager::handleDecideFailure() {
    // S1. 连续推理失败计数加一
    ++failCount_;

    // S2. 未达阈值 → 保持当前模型，等下次重试
    if (failCount_ < kFailThreshold) {
        return;
    }

    // S2.2 达阈值 → 先重置计数，再按当前层级降级（两级阶梯，D-08）
    failCount_ = 0;

    if (!model_.empty() && model_ != "default") {
        // S2.2.1 尚未使用默认模型 → 切到默认模型；失败则降至停用
        //   路径拼接例外：MODEL_DIR 是编译期宏，不依赖装配层存活
        const std::string defaultPath = std::string(MODEL_DIR) + kDefaultModelFile;
        if (!setModel(defaultPath)) {
            model_.clear();
        }
    } else {
        // S2.2.2 已在默认模型或已停用 → 无处可降：只停调用路径，**不卸载会话、不改 modelPath**
        model_.clear();
    }
}

} // namespace sched
