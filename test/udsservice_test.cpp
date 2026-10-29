// SPDX-License-Identifier: MIT
// UDSService 单元测试（详细设计 §3.2 / §4.6）
//
// 两层覆盖：
//   1. **route() 直测**——start() 只建监听、不起循环（§3.2 S8），故可以不起线程直接喂帧，
//      验证验头、剥头、分派、加头与四个失败码（1001 / 1002 / 1003 / 1005 / 1006）；
//   2. **端到端**——起 serve() 线程 + 裸 socket 客户端，验证半包、粘包、
//      超 maxConn 被拒、半帧超时（D-29）、stop 后套接字文件被清理。
//
// 所有用例共用一个进程内的全静态类，故每个用例开始/结束都要 stop()，避免状态互串。

#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>
#include <unistd.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
#include <utility>

#include "common/include/FileService.h"
#include "common/include/UDSService.h"
#include "test_util.h"

using sched::FileService;
using sched::UDSService;
using namespace std::chrono_literals;

namespace {

constexpr unsigned char kMagic0 = 0x53;
constexpr unsigned char kMagic1 = 0x44;
constexpr unsigned char kTypeTaskAsk = 0x01;
constexpr unsigned char kTypeGetState = 0x02;

const char* kOkShell = "{\"ok\":true,\"code\":0,\"message\":\"\",\"data\":null}";

std::string makeFrame(unsigned char type, const std::string& payload, unsigned char version = 1,
                      unsigned char magic0 = kMagic0, unsigned char magic1 = kMagic1) {
    std::string frame(8, '\0');
    frame[0] = static_cast<char>(magic0);
    frame[1] = static_cast<char>(magic1);
    frame[2] = static_cast<char>(version);
    frame[3] = static_cast<char>(type);
    const std::size_t n = payload.size();
    frame[4] = static_cast<char>((n >> 24) & 0xFFU);
    frame[5] = static_cast<char>((n >> 16) & 0xFFU);
    frame[6] = static_cast<char>((n >> 8) & 0xFFU);
    frame[7] = static_cast<char>(n & 0xFFU);
    return frame + payload;
}

// 按 §4.6.1 解应答帧：验头合法后取出 type 与纯载荷
bool parseReply(const std::string& reply, unsigned char* outType, std::string* outPayload) {
    if (reply.size() < 8) {
        return false;
    }
    const unsigned char* p = reinterpret_cast<const unsigned char*>(reply.data());
    if (p[0] != kMagic0 || p[1] != kMagic1 || p[2] != 1) {
        return false;
    }
    const std::size_t len = (static_cast<std::size_t>(p[4]) << 24) |
                            (static_cast<std::size_t>(p[5]) << 16) |
                            (static_cast<std::size_t>(p[6]) << 8) | static_cast<std::size_t>(p[7]);
    if (len + 8 != reply.size()) {
        return false;
    }
    if (outType != nullptr) {
        *outType = p[3];
    }
    if (outPayload != nullptr) {
        *outPayload = reply.substr(8);
    }
    return true;
}

// 取外壳里的 code 字段（失败体形态固定，直接找键即可，不引 JSON 库）
int replyCode(const std::string& payload) {
    const std::string key = "\"code\":";
    const std::size_t at = payload.find(key);
    if (at == std::string::npos) {
        return -1;
    }
    return std::atoi(payload.c_str() + at + key.size());
}

std::string sockPath(const char* tag) {
    static int seq = 0;
    ++seq;
    return std::string("/tmp/schedd_uds_") + tag + "_" + std::to_string(::getpid()) + "_" +
           std::to_string(seq) + ".sock";
}

int connectClient(const std::string& path) {
    const int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        return -1;
    }
    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    std::snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", path.c_str());
    if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        ::close(fd);
        return -1;
    }
    timeval tv{};
    tv.tv_sec = 2;   // 客户端读超时：服务端不回应也不要挂死测试
    ::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    return fd;
}

bool sendAll(int fd, const std::string& data) {
    std::size_t sent = 0;
    while (sent < data.size()) {
        const ssize_t n = ::send(fd, data.data() + sent, data.size() - sent, MSG_NOSIGNAL);
        if (n <= 0) {
            return false;
        }
        sent += static_cast<std::size_t>(n);
    }
    return true;
}

bool recvAll(int fd, char* buf, std::size_t want) {
    std::size_t got = 0;
    while (got < want) {
        const ssize_t n = ::recv(fd, buf + got, want - got, 0);
        if (n <= 0) {
            return false;
        }
        got += static_cast<std::size_t>(n);
    }
    return true;
}

bool readReply(int fd, std::string* payload) {
    char head[8];
    if (!recvAll(fd, head, sizeof(head))) {
        return false;
    }
    const unsigned char* p = reinterpret_cast<const unsigned char*>(head);
    if (p[0] != kMagic0 || p[1] != kMagic1 || p[2] != 1) {
        return false;
    }
    const std::size_t len = (static_cast<std::size_t>(p[4]) << 24) |
                            (static_cast<std::size_t>(p[5]) << 16) |
                            (static_cast<std::size_t>(p[6]) << 8) | static_cast<std::size_t>(p[7]);
    payload->assign(len, '\0');
    if (len > 0 && !recvAll(fd, &(*payload)[0], len)) {
        return false;
    }
    return true;
}

// 起服务端 + serve 线程；返回 false 表示 start 失败
bool startServer(const std::string& path, int maxConn, UDSService::Handler handler,
                 std::thread* thread) {
    if (!UDSService::start(path, maxConn, std::move(handler))) {
        return false;
    }
    *thread = std::thread([] { UDSService::serve(); });
    return true;
}

void stopServer(std::thread* thread) {
    UDSService::stop();
    if (thread->joinable()) {
        thread->join();
    }
}

} // namespace

// ---------------------------------------------------------------------------
// route() 直测
// ---------------------------------------------------------------------------

TEST_CASE(route_before_start_returns_1005) {
    UDSService::stop();
    const std::string reply = UDSService::route(makeFrame(kTypeGetState, ""));
    unsigned char type = 0;
    std::string payload;
    CHECK_TRUE(parseReply(reply, &type, &payload));
    CHECK_EQ(static_cast<int>(type), 0x82);       // 应答 type 置高位
    CHECK_EQ(replyCode(payload), 1005);
}

TEST_CASE(route_without_handler_returns_1006) {
    UDSService::stop();
    const std::string path = sockPath("nohandler");
    CHECK_TRUE(UDSService::start(path, 4, nullptr));

    const std::string reply = UDSService::route(makeFrame(kTypeGetState, ""));
    std::string payload;
    CHECK_TRUE(parseReply(reply, nullptr, &payload));
    CHECK_EQ(replyCode(payload), 1006);

    UDSService::stop();
}

TEST_CASE(route_success_strips_and_adds_header) {
    UDSService::stop();
    const std::string path = sockPath("ok");

    std::string seenType;
    std::string seenPayload;
    CHECK_TRUE(UDSService::start(path, 4, [&](const std::string& type, const std::string& body) {
        seenType = type;
        seenPayload = body;
        return std::string(kOkShell);
    }));

    const std::string requestPayload = "{\"request_id\":\"r-1\"}";
    const std::string reply = UDSService::route(makeFrame(kTypeTaskAsk, requestPayload));

    unsigned char type = 0;
    std::string payload;
    CHECK_TRUE(parseReply(reply, &type, &payload));
    CHECK_EQ(static_cast<int>(type), 0x81);
    // 业务层只看到「类型 + 纯载荷」，8 字节前缀在此丢弃（D-27）
    CHECK_EQ(seenType, std::string("TaskAsk"));
    CHECK_EQ(seenPayload, requestPayload);
    // handler 返回的外壳原样作载荷，route 只在前面加头、不解析
    CHECK_EQ(payload, std::string(kOkShell));

    UDSService::stop();
}

TEST_CASE(route_bad_magic_returns_1001) {
    UDSService::stop();
    const std::string path = sockPath("magic");
    CHECK_TRUE(UDSService::start(path, 4, [](const std::string&, const std::string&) {
        return std::string(kOkShell);
    }));

    const std::string reply =
        UDSService::route(makeFrame(kTypeTaskAsk, "{}", 1, 0x58, 0x58));   // 'XX'
    std::string payload;
    CHECK_TRUE(parseReply(reply, nullptr, &payload));
    CHECK_EQ(replyCode(payload), 1001);

    UDSService::stop();
}

TEST_CASE(route_bad_version_returns_1003) {
    UDSService::stop();
    const std::string path = sockPath("version");
    CHECK_TRUE(UDSService::start(path, 4, [](const std::string&, const std::string&) {
        return std::string(kOkShell);
    }));

    const std::string reply = UDSService::route(makeFrame(kTypeTaskAsk, "{}", 2));
    unsigned char type = 0;
    std::string payload;
    CHECK_TRUE(parseReply(reply, &type, &payload));
    CHECK_EQ(replyCode(payload), 1003);
    CHECK_EQ(static_cast<int>(type), 0x81);   // 能识别 type 就回显

    UDSService::stop();
}

TEST_CASE(route_length_mismatch_returns_1001) {
    UDSService::stop();
    const std::string path = sockPath("length");
    CHECK_TRUE(UDSService::start(path, 4, [](const std::string&, const std::string&) {
        return std::string(kOkShell);
    }));

    // 头里声明的长度比实收多 1 字节
    std::string frame = makeFrame(kTypeTaskAsk, "{}");
    frame[7] = static_cast<char>(frame[7] + 1);
    std::string payload;
    CHECK_TRUE(parseReply(UDSService::route(frame), nullptr, &payload));
    CHECK_EQ(replyCode(payload), 1001);

    UDSService::stop();
}

TEST_CASE(route_length_over_64k_returns_1001) {
    UDSService::stop();
    const std::string path = sockPath("toolong");
    CHECK_TRUE(UDSService::start(path, 4, [](const std::string&, const std::string&) {
        return std::string(kOkShell);
    }));

    // 声明 70000 字节（> 65536）：按帧级故障处理，不为它分配缓冲
    std::string frame(8, '\0');
    frame[0] = static_cast<char>(kMagic0);
    frame[1] = static_cast<char>(kMagic1);
    frame[2] = 1;
    frame[3] = static_cast<char>(kTypeTaskAsk);
    const unsigned int bad = 70000U;
    frame[4] = static_cast<char>((bad >> 24) & 0xFFU);
    frame[5] = static_cast<char>((bad >> 16) & 0xFFU);
    frame[6] = static_cast<char>((bad >> 8) & 0xFFU);
    frame[7] = static_cast<char>(bad & 0xFFU);

    std::string payload;
    CHECK_TRUE(parseReply(UDSService::route(frame), nullptr, &payload));
    CHECK_EQ(replyCode(payload), 1001);

    UDSService::stop();
}

TEST_CASE(route_short_frame_returns_1001) {
    UDSService::stop();
    const std::string path = sockPath("short");
    CHECK_TRUE(UDSService::start(path, 4, [](const std::string&, const std::string&) {
        return std::string(kOkShell);
    }));

    std::string payload;
    CHECK_TRUE(parseReply(UDSService::route(std::string("SD")), nullptr, &payload));
    CHECK_EQ(replyCode(payload), 1001);

    UDSService::stop();
}

TEST_CASE(route_unknown_type_returns_1002_without_dispatch) {
    UDSService::stop();
    const std::string path = sockPath("unknown");
    int calls = 0;
    CHECK_TRUE(UDSService::start(path, 4, [&calls](const std::string&, const std::string&) {
        ++calls;
        return std::string(kOkShell);
    }));

    const std::string reply = UDSService::route(makeFrame(0x09, ""));
    unsigned char type = 0;
    std::string payload;
    CHECK_TRUE(parseReply(reply, &type, &payload));
    CHECK_EQ(replyCode(payload), 1002);
    CHECK_EQ(static_cast<int>(type), 0x89);
    CHECK_EQ(calls, 0);   // 未知类型不进业务层

    UDSService::stop();
}

// ---------------------------------------------------------------------------
// 端到端（真实 UDS + serve 线程）
// ---------------------------------------------------------------------------

TEST_CASE(e2e_half_packet_is_reassembled) {
    UDSService::stop();
    const std::string path = sockPath("half");
    std::thread server;
    CHECK_TRUE(startServer(
        path, 8,
        [](const std::string&, const std::string&) { return std::string(kOkShell); }, &server));

    const int client = connectClient(path);
    CHECK_TRUE(client >= 0);
    const std::string frame = makeFrame(kTypeGetState, "");
    CHECK_TRUE(sendAll(client, frame.substr(0, 4)));       // 先发半帧
    std::this_thread::sleep_for(120ms);
    CHECK_TRUE(sendAll(client, frame.substr(4)));          // 再补齐

    std::string payload;
    CHECK_TRUE(readReply(client, &payload));
    CHECK_EQ(replyCode(payload), 0);

    ::close(client);
    stopServer(&server);
    CHECK_FALSE(FileService::exists(path));                // stop 已 unlink
}

TEST_CASE(e2e_sticky_frames_each_replied) {
    UDSService::stop();
    const std::string path = sockPath("sticky");
    std::thread server;
    CHECK_TRUE(startServer(
        path, 8,
        [](const std::string&, const std::string&) { return std::string(kOkShell); }, &server));

    const int client = connectClient(path);
    CHECK_TRUE(client >= 0);
    // 一次写两帧（粘包）：服务端应逐帧切分、逐帧应答
    CHECK_TRUE(sendAll(client, makeFrame(kTypeGetState, "") + makeFrame(kTypeTaskAsk, "{}")));

    std::string first;
    std::string second;
    CHECK_TRUE(readReply(client, &first));
    CHECK_TRUE(readReply(client, &second));
    CHECK_EQ(replyCode(first), 0);
    CHECK_EQ(replyCode(second), 0);

    ::close(client);
    stopServer(&server);
}

TEST_CASE(e2e_connection_over_max_conn_is_closed) {
    UDSService::stop();
    const std::string path = sockPath("maxconn");
    std::thread server;
    CHECK_TRUE(startServer(
        path, 1,   // 只容 1 条连接
        [](const std::string&, const std::string&) { return std::string(kOkShell); }, &server));

    const int first = connectClient(path);
    CHECK_TRUE(first >= 0);
    std::this_thread::sleep_for(150ms);   // 等第一条被 accept

    const int second = connectClient(path);
    CHECK_TRUE(second >= 0);              // connect 会成功（进 backlog）
    std::this_thread::sleep_for(200ms);

    char byte = 0;
    const ssize_t n = ::recv(second, &byte, 1, 0);
    CHECK_EQ(static_cast<int>(n), 0);     // 超限的新连接被立即 close → 读到 EOF

    ::close(second);
    ::close(first);
    stopServer(&server);
}

TEST_CASE(e2e_half_frame_times_out) {
    // D-29：inbuf 非空且 5 s 未读到新字节 → 剪掉该连接。这是唯一需要等满超时的用例。
    UDSService::stop();
    const std::string path = sockPath("halftimeout");
    std::thread server;
    CHECK_TRUE(startServer(
        path, 8,
        [](const std::string&, const std::string&) { return std::string(kOkShell); }, &server));

    const int client = connectClient(path);
    CHECK_TRUE(client >= 0);
    CHECK_TRUE(sendAll(client, makeFrame(kTypeGetState, "").substr(0, 4)));   // 只发 4 字节
    std::this_thread::sleep_for(6300ms);                                      // 越过 5 s 阈值

    char byte = 0;
    const ssize_t n = ::recv(client, &byte, 1, 0);
    CHECK_EQ(static_cast<int>(n), 0);     // 已被服务端关闭（不回错误码，D-29）

    ::close(client);
    stopServer(&server);
}

TEST_CASE(e2e_idle_connection_not_cut) {
    // D-29 反面：inbuf 为空的空闲连接不参与半帧判定（schedctl 常驻场景）
    UDSService::stop();
    const std::string path = sockPath("idle");
    std::thread server;
    CHECK_TRUE(startServer(
        path, 8,
        [](const std::string&, const std::string&) { return std::string(kOkShell); }, &server));

    const int client = connectClient(path);
    CHECK_TRUE(client >= 0);
    std::this_thread::sleep_for(1500ms);   // 空闲但不发半帧
    CHECK_TRUE(sendAll(client, makeFrame(kTypeGetState, "")));

    std::string payload;
    CHECK_TRUE(readReply(client, &payload));   // 仍能正常应答
    CHECK_EQ(replyCode(payload), 0);

    ::close(client);
    stopServer(&server);
}

TEST_MAIN()
