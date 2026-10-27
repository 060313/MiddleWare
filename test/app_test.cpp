// SPDX-License-Identifier: MIT
// 装配层集成测试（详细设计 §6.1 / §6.2 / §6.3 / D-23）
//
// 手法：init + run 走完整装配与停机，UDSService 用桩（端点参数与 serve 调用可被观测），
//       故「配置 → 装配 → 启两线程 → 停机」这条链路无需真 socket 即可验证。
//       run() 会阻塞等 stopRequested，测试另起一线程延时调 stop()。

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <string>
#include <thread>

#include "app/include/ScheddApp.h"
#include "common/include/FileService.h"
#include "test/stub/onnx_stub.h"
#include "test/stub/uds_stub.h"
#include "test/test_util.h"

using namespace sched;

namespace fs = std::filesystem;

namespace {

const std::string kRoot = "build/test_fake_app";

const char* kConfig =
    "[server]\n"
    "socket_path = /tmp/schedd-test.sock\n"
    "max_conn = 7\n"
    "[agent]\n"
    "model_file =\n"
    "[system]\n"
    "cpu_freq_min = 408000\n"
    "cpu_freq_max = 2256000\n"
    "npu_freq_min = 300000\n"
    "npu_freq_max = 1000000\n"
    "cpu_freq_path =\n"
    "npu_path =\n"
    "proc_stat_path =\n"
    "sample_interval_ms = 200\n";

void resetRoot() {
    fs::remove_all(kRoot);
    fs::create_directories(kRoot);
}

void testInitTolerant() {
    test::section("1. 装配容错：配置缺失不阻塞启动（D-23）");
    resetRoot();
    stub::uds().reset();
    stub::onnx().reset();

    ScheddApp app;
    CHECK(app.init(kRoot + "/nope.conf"));   // 文件不存在 → 全部键取默认，仍应装配成功
}

void testRunAndStop() {
    test::section("2. run / stop：起两线程、端点参数入配置、停机返回 0");
    resetRoot();
    stub::uds().reset();
    stub::onnx().reset();
    stub::onnx().dim = 4;

    const std::string conf = kRoot + "/schedd.conf";
    FileService::write(conf, kConfig);

    ScheddApp app;
    CHECK(app.init(conf));

    // run 阻塞等 stopRequested：延时后从另一线程请求停机
    std::thread stopper([&app]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        app.stop();
    });

    const int rc = app.run();
    stopper.join();

    CHECK(rc == 0);
    CHECK(stub::uds().socketPath == "/tmp/schedd-test.sock");   // 配置覆盖了默认值
    CHECK(stub::uds().maxConn == 7);
    CHECK(stub::uds().serveCalls == 1);                          // 服务线程进来过一次
    CHECK(stub::uds().stopCalls >= 1);                           // 停机首步即关端点
    CHECK(stub::onnx().loadCalls == 0);                          // model_file 留空 → 不加载模型
}

} // namespace

int main() {
    resetRoot();

    testInitTolerant();
    testRunAndStop();

    const int rc = test::report("app");
    fs::remove_all(kRoot);
    return rc;
}
