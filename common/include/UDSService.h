// SPDX-License-Identifier: MIT
// UDSService —— 公共基础设施类 · common 子系统 · public（详细设计 §3.2）
//
// 职责：UDS 监听端点与连接集合的持有者——建监听、跑单线程 epoll 事件循环、
//       收发字节、切帧、验头剥头、组壳加头。
// 设计要点：
//   - **全静态类**（§1）：不实例化、无构造与析构，故所有状态都是静态成员；
//   - handler 带参：入参是「报文类型 + 纯载荷」，**不含 8 字节帧头**（D-27）——
//     前缀只在本类剥、只在本类加，业务层不接触二进制前缀；
//   - start 只建监听与注册 epoll，**不启循环**；循环是 serve()，由服务线程调用；
//   - 业务失败一律回帧、保持连接，只有帧级故障（1001 / 1003）才断连（§4.6.5）。

#ifndef SCHED_COMMON_UDSSERVICE_H
#define SCHED_COMMON_UDSSERVICE_H

#include <atomic>
#include <chrono>
#include <functional>
#include <string>
#include <vector>

namespace sched {

class UDSService {
public:
    // 端点类型；实现层用 POSIX fd
    using Socket = int;

    // 报文处理回调：入参（报文类型、纯载荷 JSON），返回**四键外壳 JSON**
    // （不含 8 字节头——组壳由业务层做、加头由 route 做，见 D-27 / §4.6.6）
    using Handler = std::function<std::string(const std::string&, const std::string&)>;

    // 全静态类：禁止实例化
    UDSService() = delete;

    // 建立监听端点：建 socket → bind → listen（队列长度取 maxConn）→
    // 存回调与上限 → 建 epoll 与唤醒 fd 并注册 → 置 running。
    // 前置：running == false、路径非空。**不建线程、不启循环**。
    static bool start(const std::string& socketPath, int maxConn, Handler handler);

    // 关闭监听端点：置 running = false → 向 wakeFd 写 1 唤醒 epoll_wait →
    // 关 listener → unlink 端点。**不 join、不遍历 conns**（交给 serve 退出路径）。
    static void stop();

    // 处理一帧：验 8 字节帧头并剥离 → 调 handler → 前置 8 字节头返回。
    // 帧级故障（魔数 / 版本 / 长度越界）由本方法自建 1001 / 1003 失败壳；
    // 业务失败壳由 handler 返回，本方法原样透传、不解析。
    static std::string route(const std::string& requestFrame);

    // **服务循环（服务线程入口）**：epoll_wait 等事件 → wakeFd 可读则判停 →
    // listener 可读则 accept（超 maxConn 立即关新连接）→ 连接可读则收字节入 inbuf、
    // 按帧切分后逐帧 route 并写回 → 对端关闭则移除 → 每轮扫描半帧悬挂连接（D-29）。
    // 退出时清空 conns 并关闭 epfd / wakeFd。
    static void serve();

private:
    // 一条已接受的连接；inbuf 应对粘包 / 半包，lastActive 仅供半帧超时判定
    struct Conn {
        Socket fd = -1;
        std::string inbuf;
        std::chrono::steady_clock::time_point lastActive;
    };

    static Socket listener_;                  // 监听端点
    static int epfd_;                         // epoll 实例句柄
    static int wakeFd_;                       // eventfd 唤醒句柄
    static std::vector<Conn> conns_;          // 已接受连接集合（仅服务线程访问）
    static std::string socketPath_;           // 监听路径（stop 时 unlink 用）
    static std::atomic<bool> running_;        // 服务循环运行标志（跨线程）
    static Handler handler_;                  // 报文处理回调
    static int maxConn_;                      // 并发连接上限
};

} // namespace sched

#endif // SCHED_COMMON_UDSSERVICE_H
