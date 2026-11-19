// SPDX-License-Identifier: MIT
// UDSService 实现（详细设计 §3.2 / §4.6）
//
// 静态成员定义集中在本文件顶部——全静态类没有构造，状态只能在此处落地。
//
// 三层职责，不要串味：
//   1. start()：只建端点与 epoll 注册，**不启循环**（循环归 serve，线程归装配层，D-07）；
//   2. serve()：单线程 epoll 循环，只管「字节进、字节出」——收字节入 inbuf、
//      按 8 字节头切帧、逐帧交 route、把应答整帧写回、清理半帧悬挂连接（D-29）；
//   3. route()：只管「帧」——验头剥头 → 调 handler（只给类型 + 纯载荷，D-27）→ 组壳加头。
//
// 断连判据（§4.6.5）：只有帧级故障（1001 / 1003）断连；业务失败一律回帧、保持连接（D-03）。

#include "common/include/UDSService.h"

#include <poll.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <utility>

namespace sched {

namespace {

// ---- 帧格式（§4.6.1）：定长 8 字节头（大端）+ 变长载荷 ----
constexpr unsigned char kMagic0 = 0x53;   // 'S'
constexpr unsigned char kMagic1 = 0x44;   // 'D'
constexpr unsigned char kVersion = 1;
constexpr std::size_t kHeaderLen = 8;
constexpr std::size_t kMaxPayload = 65536;  // 单帧上限 64 KiB，越界不分配缓冲
constexpr unsigned char kReplyFlag = 0x80;  // 应答 type 置高位

// ---- 报文类型（§4.6.1 / §4.6.4）----
constexpr unsigned char kTypeTaskAsk = 0x01;
constexpr unsigned char kTypeGetState = 0x02;
constexpr unsigned char kTypeSetModel = 0x03;

// ---- 循环参数（§3.2 serve S2 / S4）----
constexpr int kEpollTimeoutMs = 1000;        // 兼作半帧扫描周期钩子，不另建 timerfd
constexpr int kHalfFrameTimeoutSec = 5;      // 半帧超时（D-29）
constexpr int kReadChunk = 4096;
constexpr int kMaxEvents = 64;
constexpr int kMaxWriteStall = 10;           // 非阻塞写遇 EAGAIN 的最多重试次数

// 组 8 字节帧头；type 由调用方决定是否已置 0x80
std::string encodeFrame(unsigned char type, const std::string& payload) {
    std::string frame(kHeaderLen, '\0');
    frame[0] = static_cast<char>(kMagic0);
    frame[1] = static_cast<char>(kMagic1);
    frame[2] = static_cast<char>(kVersion);
    frame[3] = static_cast<char>(type);
    const std::size_t len = payload.size();
    frame[4] = static_cast<char>((len >> 24) & 0xFFU);
    frame[5] = static_cast<char>((len >> 16) & 0xFFU);
    frame[6] = static_cast<char>((len >> 8) & 0xFFU);
    frame[7] = static_cast<char>(len & 0xFFU);
    frame += payload;
    return frame;
}

// 四键外壳的失败体（§4.6.2）：ok=false / code / message / data=null
std::string failureShell(int code, const std::string& message) {
    return "{\"ok\":false,\"code\":" + std::to_string(code) +
           ",\"message\":\"" + message + "\",\"data\":null}";
}

// 帧级故障判据：只有 1001 / 1003 断连（§4.6.5）。
// route() 的返回值只有字符串、无旁路标志，故按固定失败体的 code 字段判定——
// 失败体由本文件 failureShell() 唯一生成，格式确定。
bool isFatalReply(const std::string& reply) {
    return reply.find("\"code\":1001") != std::string::npos ||
           reply.find("\"code\":1003") != std::string::npos;
}

// 整帧写回：非阻塞 socket 可能短写或 EAGAIN，逐段推进；失败即判断连
bool writeAll(int fd, const std::string& data) {
    std::size_t sent = 0;
    int stalls = 0;
    while (sent < data.size()) {
        const ssize_t n = ::send(fd, data.data() + sent, data.size() - sent, MSG_NOSIGNAL);
        if (n > 0) {
            sent += static_cast<std::size_t>(n);
            continue;
        }
        if (n < 0 && errno == EINTR) {
            continue;
        }
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK) && stalls < kMaxWriteStall) {
            ++stalls;
            pollfd pfd{};
            pfd.fd = fd;
            pfd.events = POLLOUT;
            if (::poll(&pfd, 1, 100) <= 0) {
                return false;   // 对端不读：不无限等，判失败
            }
            continue;
        }
        return false;
    }
    return true;
}

// 从 8 字节头取大端 length
std::size_t readLength(const char* header) {
    const unsigned char* p = reinterpret_cast<const unsigned char*>(header);
    return (static_cast<std::size_t>(p[4]) << 24) | (static_cast<std::size_t>(p[5]) << 16) |
           (static_cast<std::size_t>(p[6]) << 8) | static_cast<std::size_t>(p[7]);
}

} // namespace

UDSService::Socket UDSService::listener_ = -1;
int UDSService::epfd_ = -1;
int UDSService::wakeFd_ = -1;
std::vector<UDSService::Conn> UDSService::conns_;
std::string UDSService::socketPath_;
std::atomic<bool> UDSService::running_{false};
UDSService::Handler UDSService::handler_ = nullptr;
int UDSService::maxConn_ = 16;

bool UDSService::start(const std::string& socketPath, int maxConn, Handler handler) {
    // S1. 幂等保护：已在运行视为成功（设计 §3.2 start S1）
    if (running_.load()) {
        return true;
    }

    // S2. 路径非空（空路径无法确定监听地址）
    if (socketPath.empty()) {
        return false;
    }

    // sun_path 有 108 字节硬上限，超长直接拒
    sockaddr_un addr{};
    if (socketPath.size() >= sizeof(addr.sun_path)) {
        return false;
    }
    addr.sun_family = AF_UNIX;
    std::memcpy(addr.sun_path, socketPath.c_str(), socketPath.size() + 1);

    // S3. 建监听套接字（非阻塞：accept 循环靠 EAGAIN 收敛，阻塞会在第二次 accept 挂死）
    const Socket fd = ::socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (fd < 0) {
        return false;
    }

    // S4. 清理上次被 SIGKILL 留下的残留端点（否则 bind 报 EADDRINUSE）
    ::unlink(socketPath.c_str());

    // S5. bind → listen（队列长度取 maxConn，非正值回退 16）
    if (::bind(fd, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr)) < 0) {
        ::close(fd);
        return false;
    }
    const int backlog = (maxConn > 0) ? maxConn : 16;
    if (::listen(fd, backlog) < 0) {
        ::close(fd);
        ::unlink(socketPath.c_str());
        return false;
    }

    // S6. 写入回调与上限（回调可为空：route 会判 1006，属可测的降级态）
    handler_ = std::move(handler);
    maxConn_ = backlog;

    // S7. 建 epoll 与唤醒 fd，一并注册可读事件
    const int epfd = ::epoll_create1(EPOLL_CLOEXEC);
    if (epfd < 0) {
        ::close(fd);
        ::unlink(socketPath.c_str());
        return false;
    }
    const int wakeFd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (wakeFd < 0) {
        ::close(epfd);
        ::close(fd);
        ::unlink(socketPath.c_str());
        return false;
    }

    epoll_event ev{};
    ev.events = EPOLLIN;
    ev.data.fd = fd;
    if (::epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev) < 0) {
        ::close(wakeFd);
        ::close(epfd);
        ::close(fd);
        ::unlink(socketPath.c_str());
        return false;
    }
    epoll_event wakeEv{};
    wakeEv.events = EPOLLIN;
    wakeEv.data.fd = wakeFd;
    if (::epoll_ctl(epfd, EPOLL_CTL_ADD, wakeFd, &wakeEv) < 0) {
        ::close(wakeFd);
        ::close(epfd);
        ::close(fd);
        ::unlink(socketPath.c_str());
        return false;
    }

    // 全部就绪后才落静态状态，避免半成品被别的线程看见
    listener_ = fd;
    epfd_ = epfd;
    wakeFd_ = wakeFd;
    socketPath_ = socketPath;

    // S8. 置标志返回；**不启动任何循环**（线程归属见 D-07）
    running_.store(true);
    return true;
}

void UDSService::stop() {
    // S1/S2. 幂等 + 置标志（exchange 保证并发调用只走一次）
    if (!running_.exchange(false)) {
        return;
    }

    // S3. 已有连接不在此处关闭——conns_ 只由服务线程增删，
    //     跨线程遍历会让迭代器失效；统一归 serve() 退出路径（S8）

    // S4. 写 eventfd 唤醒阻塞中的 epoll_wait
    if (wakeFd_ >= 0) {
        const std::uint64_t one = 1;
        ssize_t ignored = ::write(wakeFd_, &one, sizeof(one));
        (void)ignored;
    }

    // S5. 关监听端点（此后不再接受新连接）
    if (listener_ >= 0) {
        if (epfd_ >= 0) {
            ::epoll_ctl(epfd_, EPOLL_CTL_DEL, listener_, nullptr);
        }
        ::close(listener_);
        listener_ = -1;
    }

    // S6. 移除套接字文件并清空路径
    if (!socketPath_.empty()) {
        ::unlink(socketPath_.c_str());
        socketPath_.clear();
    }

    // 注：epfd_ / wakeFd_ 由 serve() 退出路径关闭（§3.2 serve S8）。
    //     若 serve() 从未启动（如只做 route 单测），这两个 fd 会留到进程退出——
    //     这是设计里对「循环未启动」情形的已知缺口，不在 stop() 里补，以免与
    //     正在退出的服务线程双关同一 fd。
}

std::string UDSService::route(const std::string& requestFrame) {
    const char* raw = requestFrame.data();
    const std::size_t size = requestFrame.size();

    // 失败体也要成帧，故先尽量取出可回显的 type（取不到就用 0）
    const bool headerReadable = (size >= kHeaderLen &&
                                 static_cast<unsigned char>(raw[0]) == kMagic0 &&
                                 static_cast<unsigned char>(raw[1]) == kMagic1);
    const unsigned char echoType =
        headerReadable ? static_cast<unsigned char>(raw[3]) : static_cast<unsigned char>(0x00);
    const auto replyType = [](unsigned char type) {
        return static_cast<unsigned char>(type | kReplyFlag);
    };

    // S1. 前置：服务未启动 → 1005；回调未注入 → 1006（均回帧，不断连）
    if (!running_.load()) {
        return encodeFrame(replyType(echoType), failureShell(1005, "service not running"));
    }
    if (!handler_) {
        return encodeFrame(replyType(echoType), failureShell(1006, "handler not attached"));
    }

    // S2. 验头：短帧 / 魔数不符 → 1001；版本不符 → 1003；长度越界或与实收不符 → 1001
    if (size < kHeaderLen) {
        return encodeFrame(replyType(static_cast<unsigned char>(0x00)),
                           failureShell(1001, "frame too short"));
    }
    if (static_cast<unsigned char>(raw[0]) != kMagic0 ||
        static_cast<unsigned char>(raw[1]) != kMagic1) {
        return encodeFrame(replyType(static_cast<unsigned char>(0x00)),
                           failureShell(1001, "bad magic"));
    }
    if (static_cast<unsigned char>(raw[2]) != kVersion) {
        return encodeFrame(replyType(echoType), failureShell(1003, "bad version"));
    }
    const std::size_t len = readLength(raw);
    if (len > kMaxPayload || len + kHeaderLen != size) {
        return encodeFrame(replyType(echoType), failureShell(1001, "bad length"));
    }

    // S2 续. 剥前缀：**8 字节头在此丢弃**，业务层只见「类型 + 纯载荷」（D-27）
    const std::string payload = requestFrame.substr(kHeaderLen, len);

    // S3. 分派并同步调用（未知类型 → 1002，不断连）
    const unsigned char type = echoType;
    const char* typeName = nullptr;
    switch (type) {
        case kTypeTaskAsk:
            typeName = "TaskAsk";
            break;
        case kTypeGetState:
            typeName = "GetState";
            break;
        case kTypeSetModel:
            typeName = "SetModel";
            break;
        default:
            return encodeFrame(replyType(type), failureShell(1002, "unknown message type"));
    }

    // handler 返回的只是**四键外壳 JSON**（不含帧头，§4.6.6）；
    // 异常与空串都视为业务层未能给出结果，回 1004 而非断连
    std::string body;
    try {
        body = handler_(typeName, payload);
    } catch (...) {
        body.clear();
    }
    if (body.empty()) {
        return encodeFrame(replyType(type), failureShell(1004, "empty handler result"));
    }

    // S4. 组壳加头：业务结果原样作载荷，前置 8 字节头（type 置应答高位）
    return encodeFrame(replyType(type), body);
}

void UDSService::serve() {
    // 前置：端点与 epoll 必须已就绪
    if (epfd_ < 0 || listener_ < 0) {
        return;
    }

    epoll_event events[kMaxEvents];

    const auto now = [] { return std::chrono::steady_clock::now(); };
    const auto indexOf = [](Socket fd) -> std::size_t {
        for (std::size_t i = 0; i < conns_.size(); ++i) {
            if (conns_[i].fd == fd) {
                return i;
            }
        }
        return conns_.size();   // == size() 表示未找到
    };
    // 清理顺序不可颠倒：先 EPOLL_CTL_DEL 再 close 再 erase，
    // 否则 fd 号会被后续 accept 复用而残留陈旧事件（D-29）
    const auto dropAt = [](std::size_t idx) {
        if (idx >= conns_.size()) {
            return;
        }
        const Socket fd = conns_[idx].fd;
        if (epfd_ >= 0) {
            ::epoll_ctl(epfd_, EPOLL_CTL_DEL, fd, nullptr);
        }
        ::close(fd);
        conns_.erase(conns_.begin() + static_cast<std::ptrdiff_t>(idx));
    };

    // S5. accept：超 maxConn 立即关闭新连接，不排队、不挤占已有连接
    const auto acceptAll = [&]() {
        for (;;) {
            const int cfd = ::accept4(listener_, nullptr, nullptr, SOCK_NONBLOCK | SOCK_CLOEXEC);
            if (cfd < 0) {
                if (errno == EINTR) {
                    continue;
                }
                return;
            }
            if (static_cast<int>(conns_.size()) >= maxConn_) {
                ::close(cfd);
                continue;
            }
            epoll_event ev{};
            ev.events = EPOLLIN | EPOLLRDHUP;
            ev.data.fd = cfd;
            if (::epoll_ctl(epfd_, EPOLL_CTL_ADD, cfd, &ev) < 0) {
                ::close(cfd);
                continue;
            }
            Conn fresh;
            fresh.fd = cfd;
            fresh.lastActive = now();
            conns_.push_back(std::move(fresh));
        }
    };

    // S6. 读字节 → 追加 inbuf → 按 length 反复切帧 → 逐帧 route → 整帧写回
    const auto serveConn = [&](std::size_t idx) {
        char buf[kReadChunk];
        for (;;) {
            const ssize_t got = ::recv(conns_[idx].fd, buf, sizeof(buf), 0);
            if (got > 0) {
                conns_[idx].inbuf.append(buf, static_cast<std::size_t>(got));
                conns_[idx].lastActive = now();           // 半帧超时只看「最后成功读到字节」
                if (static_cast<std::size_t>(got) < sizeof(buf)) {
                    break;
                }
                continue;
            }
            if (got == 0) {
                dropAt(idx);                              // 对端关闭
                return;
            }
            if (errno == EINTR) {
                continue;
            }
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            }
            dropAt(idx);
            return;
        }

        // 切帧：够一帧就处理，粘包逐帧走，半包留待下次事件
        while (conns_[idx].inbuf.size() >= kHeaderLen) {
            const char* head = conns_[idx].inbuf.data();
            const bool magicOk = static_cast<unsigned char>(head[0]) == kMagic0 &&
                                 static_cast<unsigned char>(head[1]) == kMagic1;
            const std::size_t len = readLength(head);
            if (!magicOk || len > kMaxPayload) {
                // 帧级故障：回失败体（1001 由 route 生成）后断连
                const std::size_t take = std::min(kHeaderLen, conns_[idx].inbuf.size());
                const std::string reply = route(conns_[idx].inbuf.substr(0, take));
                const bool written = writeAll(conns_[idx].fd, reply);
                (void)written;
                dropAt(idx);
                return;
            }
            if (conns_[idx].inbuf.size() < kHeaderLen + len) {
                break;                                    // 半包：等后续字节
            }
            const std::string frame = conns_[idx].inbuf.substr(0, kHeaderLen + len);
            conns_[idx].inbuf.erase(0, kHeaderLen + len);

            const std::string reply = route(frame);
            if (!writeAll(conns_[idx].fd, reply)) {
                dropAt(idx);
                return;
            }
            if (isFatalReply(reply)) {
                dropAt(idx);                              // 1001 / 1003 才断连（§4.6.5）
                return;
            }
        }
    };

    // S4. 半帧超时扫描：inbuf 非空且超过 5 s 未读到新字节 → 剪掉；
    //     inbuf 为空的空闲连接不参与判定（D-29：不剪 schedctl 这类常驻客户端）
    const auto scanHalfFrame = [&]() {
        const auto t = now();
        for (std::size_t i = conns_.size(); i-- > 0;) {
            if (conns_[i].inbuf.empty()) {
                continue;
            }
            const auto idleSec =
                std::chrono::duration_cast<std::chrono::seconds>(t - conns_[i].lastActive).count();
            if (idleSec >= kHalfFrameTimeoutSec) {
                dropAt(i);
            }
        }
    };

    // S1. 循环条件：running_ 为真
    while (running_.load()) {
        // S2. 1000 ms 超时兼作半帧扫描的周期钩子；EINTR 不是错误
        const int n = ::epoll_wait(epfd_, events, kMaxEvents, kEpollTimeoutMs);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            break;
        }

        for (int i = 0; i < n; ++i) {
            const int fd = events[i].data.fd;

            // S3. 唤醒事件：读走计数后回循环顶部重判 running_
            if (fd == wakeFd_) {
                std::uint64_t counter = 0;
                while (::read(wakeFd_, &counter, sizeof(counter)) < 0 && errno == EINTR) {
                }
                continue;
            }

            if (fd == listener_) {
                acceptAll();
                continue;
            }

            const std::size_t idx = indexOf(fd);
            if (idx >= conns_.size()) {
                continue;   // 陈旧事件（fd 已被上一轮事件清理）
            }
            if (events[i].events & (EPOLLERR | EPOLLHUP)) {
                dropAt(idx);
                continue;
            }
            serveConn(idx);
        }

        // S4. 无论本轮有无事件都要扫（超时返回 0 时同样执行）
        scanHalfFrame();

        // S7. 上限兜底：任何时刻超限则关掉最后接入的连接
        while (static_cast<int>(conns_.size()) > maxConn_) {
            dropAt(conns_.size() - 1);
        }
    }

    // S8. 退出：清空连接 → 关 epfd / wakeFd → 置 running_ 假；**不抛异常**
    for (std::size_t i = 0; i < conns_.size(); ++i) {
        if (conns_[i].fd >= 0) {
            ::close(conns_[i].fd);
        }
    }
    conns_.clear();
    if (epfd_ >= 0) {
        ::close(epfd_);
        epfd_ = -1;
    }
    if (wakeFd_ >= 0) {
        ::close(wakeFd_);
        wakeFd_ = -1;
    }
    if (listener_ >= 0) {
        ::close(listener_);
        listener_ = -1;
    }
    running_.store(false);
}

} // namespace sched
