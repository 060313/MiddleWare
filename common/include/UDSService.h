// SPDX-License-Identifier: MIT
// UDSService —— 公共基础设施 · common 子系统（详见详细设计 §7.2）
//
// 职责：UDS 监听端点与连接集合的持有者，负责监听、收发与帧路由。
// 设计要点：
//   - 报文处理回调以构造注入方式存入 handler 属性（由 serviceInterface 提供）；
//   - route 解析请求帧 → 调 handler → 组应答帧；失败回可读失败体而非断连。

#ifndef SCHED_COMMON_UDSSERVICE_H
#define SCHED_COMMON_UDSSERVICE_H

#include <functional>
#include <string>
#include <vector>

namespace sched {

// 报文处理回调：输入请求帧，输出应答帧
using Handler = std::function<std::string(const std::string&)>;

// 端点/连接的占位类型；实现层可替换为文件描述符或 RAII 包装
using Socket = int;

class UDSService {
public:
    // 构造注入报文处理回调（由 serviceInterface 装配时提供）
    explicit UDSService(Handler handler);
    ~UDSService();

    // 禁止拷贝（持有端点与连接）
    UDSService(const UDSService&) = delete;
    UDSService& operator=(const UDSService&) = delete;

    // 建立监听端点。前置：running == false、路径非空。
    bool start(const std::string& socketPath);

    // 关闭 listener 与全部 conns、running 置 false。
    void stop();

    // 处理一帧：解析 → 调 handler → 组应答帧。
    std::string route(const std::string& requestFrame);

private:
    Socket listener_ = -1;                 // 监听端点
    std::vector<Socket> conns_;            // 已接受连接集合
    bool running_ = false;                 // 收发循环运行标志
    Handler handler_;                      // 报文处理回调
};

} // namespace sched

#endif // SCHED_COMMON_UDSSERVICE_H
