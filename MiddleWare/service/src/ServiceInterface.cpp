// SPDX-License-Identifier: MIT
// serviceInterface 实现（详细设计 §3.1 / §4.6 / D-27）
//
// 分层边界：本类只处理「报文类型 + 纯载荷 JSON」，8 字节帧头由 UDSService 剥与加（D-27）。

#include "service/include/ServiceInterface.h"

#include <utility>

#include "agent/include/SchedManager.h"
#include "common/include/Json.h"
#include "common/include/UDSService.h"
#include "service/include/MessageCodec.h"

namespace sched {

serviceInterface::serviceInterface(const std::string& socketPath,
                                   schedManager* mgr,
                                   int maxConn)
    : manager_(mgr),
      running_(false),
      socketPath_(socketPath),
      maxConn_((maxConn > 0) ? maxConn : 16) {
    // 只做赋值、不做 I/O：UDSService 是全静态类，监听由 start() 间接建立
}

serviceInterface::~serviceInterface() {
    // 兜底：析构不保证被调用，正常清理一律走显式 stop()
    if (running_) {
        running_ = false;   // 先置标志再拆资源
        UDSService::stop();
    }
    // 不删除 manager_（不拥有）
}

ActionInfo serviceInterface::taskAsk(const TaskInfo& taskInfo) {
    // S1. 前置：服务已启动
    if (!running_) {
        return makeFailure(Reason::serviceNotRunning, "service not running");
    }

    // S2. 调度链未装配 → 1006（与「服务没起」区分开）
    if (manager_ == nullptr) {
        return makeFailure(Reason::schedulerAbsent, "scheduler not attached");
    }

    // S3. 转交总控
    return manager_->taskAsk(taskInfo);
}

SystemStateSnapshot serviceInterface::getSystemInfo() {
    // S1. 前置不满足 → 回空快照（system 零值、model 空串），
    //     成败由 handleFrame 按 1005 / 1006 表达——快照自身**不设 ok**（D-15）
    if (!running_ || manager_ == nullptr) {
        return SystemStateSnapshot{};
    }

    // S2. 原样转交
    return manager_->getSystemInfo();
}

bool serviceInterface::setModel(const std::string& modelPath) {
    // S1. 前置：服务已启动（装配期首次加载**不走门面**，见 D-11）
    if (!running_) {
        return false;
    }
    if (manager_ == nullptr) {
        return false;
    }

    // S2. 转交总控：由其解析模型名、先建后换
    return manager_->setModel(modelPath);
}

bool serviceInterface::start() {
    // S1. 前置：未运行且路径非空
    if (running_) {
        return false;
    }
    if (socketPath_.empty()) {
        return false;
    }

    // S2. 以绑定 this 的回调启动 UDS 监听（静态调用，不创建实例）
    const bool ok = UDSService::start(
        socketPath_, maxConn_,
        [this](const std::string& type, const std::string& payload) {
            return handleFrame(type, payload);
        });
    if (!ok) {
        return false;
    }

    // S3. 监听就绪才置标志
    running_ = true;
    return true;
}

void serviceInterface::stop() {
    // S1. 先置标志，再拆资源
    running_ = false;

    // S2. 关端点并唤醒服务循环（不 join）
    UDSService::stop();
}

void serviceInterface::serve() {
    // 服务线程入口：阻塞至事件循环退出
    UDSService::serve();
}

std::string serviceInterface::handleFrame(const std::string& type, const std::string& payload) {
    // S1. 前置：服务未启动 → 1005（§4.6.3）
    if (!running_) {
        return makeEnvelope(false, 1005, "service not running", nullptr);
    }

    // S2. 按报文类型分派；本方法不加帧头（加头归 UDSService::route S4，D-27）
    if (type == "TaskAsk") {
        TaskInfo taskInfo;
        if (!decodeTaskAsk(payload, taskInfo)) {
            return makeEnvelope(false, 1004, "bad payload", nullptr);
        }
        const ActionInfo info = taskAsk(taskInfo);
        if (!info.ok) {
            // 失败：data = null，code 由 reason 唯一映射（D-33）
            return makeEnvelope(false, reasonToCode(info.reason), info.message, nullptr);
        }
        json::Value data = encodeActionInfo(info);
        return makeEnvelope(true, 0, "", &data);
    }

    if (type == "GetState") {
        if (manager_ == nullptr) {
            return makeEnvelope(false, 1006, "scheduler not attached", nullptr);
        }
        json::Value data = encodeSnapshot(getSystemInfo());
        return makeEnvelope(true, 0, "", &data);
    }

    if (type == "SetModel") {
        std::string modelPath;
        if (!decodeSetModel(payload, modelPath)) {
            return makeEnvelope(false, 1004, "bad payload", nullptr);
        }
        if (manager_ == nullptr) {
            return makeEnvelope(false, 1006, "scheduler not attached", nullptr);
        }
        if (!setModel(modelPath)) {
            // Reason 枚举里没有 invalidModel：2003 只能在此直判（§4.6.3）
            return makeEnvelope(false, 2003, "invalid model", nullptr);
        }
        json::Value data = json::Value::object();
        data.set("model", json::Value::string(manager_->getModel()));
        return makeEnvelope(true, 0, "", &data);
    }

    // 其余 type → 1002（不断连，§4.6.3）
    return makeEnvelope(false, 1002, "unknown type", nullptr);
}

} // namespace sched
