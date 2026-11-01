// SPDX-License-Identifier: MIT
// ONNXruntime 单元测试（详细设计 §3.4 / D-06）
//
// 现实约束：*.onnx 被 .gitignore 排除，仓库里没有模型；容器内也未必挂 ONNX SDK。
// 故本文件分两段：
//   - **无模型也能跑的契约用例**（未加载时的零值语义、坏文件不污染会话）；
//   - **有模型才跑的用例**（输入维、正常推理、维度不符）——找不到模型就打印 SKIP。
//
// 找模型的方式：环境变量 SCHEDD_TEST_MODEL 给出 .onnx 完整路径，或 MODEL_DIR 下任意
// 一个 .onnx。两者都没有则跳过。

#include <dirent.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "common/include/FileService.h"
#include "common/include/ONNXruntime.h"
#include "common/include/SchedDefs.h"
#include "test_util.h"

using sched::FileService;
using sched::ONNXruntime;

namespace {

// 依次尝试：环境变量 → MODEL_DIR 下的 *.onnx（取字典序第一个）
std::string findModel() {
    const char* fromEnv = std::getenv("SCHEDD_TEST_MODEL");
    if (fromEnv != nullptr && fromEnv[0] != '\0' && FileService::exists(fromEnv)) {
        return std::string(fromEnv);
    }
    // 目录枚举用 posix 方式，避免引 <filesystem> 的老工具链问题
    DIR* dir = ::opendir(MODEL_DIR);
    if (dir == nullptr) {
        return std::string();
    }
    std::string best;
    while (struct dirent* entry = ::readdir(dir)) {
        const std::string name = entry->d_name;
        if (name.size() > 5 && name.compare(name.size() - 5, 5, ".onnx") == 0) {
            const std::string full = std::string(MODEL_DIR) + name;
            if (best.empty() || full < best) {
                best = full;
            }
        }
    }
    ::closedir(dir);
    return best;
}

void skip(const char* reason) {
    std::printf("    SKIP %s\n", reason);
}

} // namespace

TEST_CASE(input_dim_zero_before_load) {
    // 空会话 -> 0；decisionMaker 据此判「未就绪」（§3.4 inputDim S1）
    CHECK_EQ(ONNXruntime::inputDim(), 0);
}

TEST_CASE(infer_empty_before_load) {
    // 未加载时推理一律空列表，不抛异常（D-06）
    CHECK_TRUE(ONNXruntime::infer(std::vector<float>{0.1F, 0.2F}).empty());
}

TEST_CASE(load_rejects_empty_path) {
    CHECK_FALSE(ONNXruntime::load(""));
}

TEST_CASE(load_rejects_missing_file) {
    CHECK_FALSE(ONNXruntime::load("/tmp/schedd_no_model_9527.onnx"));
}

TEST_CASE(load_rejects_garbage_file) {
    // 非模型文件：SDK 建会话失败 -> false，且**旧会话不被污染**（替换语义）
    CHECK_FALSE(ONNXruntime::load("/etc/hostname"));
    CHECK_EQ(ONNXruntime::inputDim(), 0);
}

TEST_CASE(load_and_infer_with_real_model) {
    const std::string model = findModel();
    if (model.empty()) {
        skip("未找到 .onnx（设 SCHEDD_TEST_MODEL 或放入 MODEL_DIR 后重跑）");
        return;
    }

    CHECK_TRUE(ONNXruntime::load(model));

    const int dim = ONNXruntime::inputDim();
    CHECK_TRUE(dim > 0);   // 末维可读；读到 -1 批维或 0 都说明取错了维度

    // 正常推理：输出应为非空的动作值列表（长度 = 契约 output_dim）
    std::vector<float> features(static_cast<std::size_t>(dim), 0.5F);
    const std::vector<float> actions = ONNXruntime::infer(features);
    CHECK_FALSE(actions.empty());

    // 维度不符：必须回空列表，而不是让 SDK 抛栈
    std::vector<float> wrong(static_cast<std::size_t>(dim) + 1, 0.5F);
    CHECK_TRUE(ONNXruntime::infer(wrong).empty());

    // 幂等重载：同一路径再 load 一次仍成功（替换语义，旧会话回收）
    CHECK_TRUE(ONNXruntime::load(model));
    CHECK_EQ(ONNXruntime::inputDim(), dim);
}

TEST_MAIN()
