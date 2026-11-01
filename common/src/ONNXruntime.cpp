// SPDX-License-Identifier: MIT
// ONNXruntime 实现（详细设计 §3.4 / D-05 / D-31）
//
// 静态成员定义集中在本文件顶部——全静态类没有构造，状态只能在此处落地。
//
// 四条硬约束：
//   1. **env_ 必须先于 session_ 定义**（声明顺序决定析构顺序）：会话不能活过环境；
//   2. **环境不得是方法内局部对象**——局部 Env 在方法返回时即销毁，而会话仍持有它，
//      首次推理必崩（§3.4 S2）；
//   3. **load 是替换语义**：先建新会话、成功才覆盖，失败保持旧会话不动（热切换的
//      「先建后换」由 decisionMaker 编排，本类只保证「失败不污染」）；
//   4. **超时必须在 infer 内部取消**（D-31）：同步 Run() 不能从外部打断，
//      故每次调用建局部 RunOptions + 一次性看护线程，到点 SetTerminate() 协作式取消，
//      返回前必须 join，不得留下持有 RunOptions 引用的游离线程。

#include "common/include/ONNXruntime.h"

#include <onnxruntime_cxx_api.h>

#include <array>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "common/include/FileService.h"

namespace sched {

namespace {

// 单次推理的硬上限（§3.8 S3.2）：**内部固化，不给配置文件开口子**。
// 下游 decisionMaker 不传该值（调用 infer(encoded) 用默认参数）。
constexpr int kDefaultInferTimeoutMs = 3000;

} // namespace

// ---------------------------------------------------------------------------
// PIMPL：隔离 SDK 头文件，避免污染上游编译单元
// ---------------------------------------------------------------------------

struct ONNXruntime::Env {
    // 进程级唯一：首次 load 创建，此后复用，进程退出由静态析构回收
    Ort::Env ort{ORT_LOGGING_LEVEL_WARNING, "schedd"};
};

struct ONNXruntime::Session {
    explicit Session(Ort::Session&& s) : ort(std::move(s)) {}

    Ort::Session ort;          // 唯一的推理会话（替换语义）
    std::string inputName;     // 输入名缓存（每帧 Run 都要用，避免反复查会话）
    std::string outputName;    // 输出名缓存（契约约定模型单输出）
};

// 定义顺序与声明顺序一致：env_ 先构造、后析构
std::unique_ptr<ONNXruntime::Env> ONNXruntime::env_ = nullptr;
std::unique_ptr<ONNXruntime::Session> ONNXruntime::session_ = nullptr;

bool ONNXruntime::load(const std::string& modelPath) {
    // S1. 前置：路径非空且文件可读（不可读直接拒，不让 SDK 抛栈）
    if (modelPath.empty() || !FileService::exists(modelPath)) {
        return false;
    }

    // S2. 环境惰性创建：进程内只建一次，此后复用（**静态成员，不得局部**）
    try {
        if (!env_) {
            env_ = std::make_unique<Env>();
        }
    } catch (...) {
        return false;
    }

    try {
        // S2 续. 建会话参数：单线程执行（调度中间件不与业务抢核），开全图优化
        Ort::SessionOptions options;
        options.SetIntraOpNumThreads(1);
        options.SetInterOpNumThreads(1);
        options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);

        // 先建到临时对象（失败会抛，旧 session_ 完全不受影响）
        Ort::Session fresh(env_->ort, modelPath.c_str(), options);

        const std::size_t inputCount = fresh.GetInputCount();
        const std::size_t outputCount = fresh.GetOutputCount();
        if (inputCount == 0 || outputCount == 0) {
            return false;   // 空图不可用
        }

        // 取输入 / 输出名（SDK 1.13+ 的 *Allocated 接口，所有权归 AllocatedStringPtr）
        Ort::AllocatorWithDefaultOptions allocator;
        auto inNamePtr = fresh.GetInputNameAllocated(0, allocator);
        auto outNamePtr = fresh.GetOutputNameAllocated(0, allocator);

        auto holder = std::make_unique<Session>(std::move(fresh));
        holder->inputName = inNamePtr.get();
        holder->outputName = outNamePtr.get();

        // S3. **替换语义**：到这里才覆盖，失败路径不碰 session_
        session_ = std::move(holder);
        return true;
    } catch (...) {
        // 建会话失败（文件损坏 / 算子不支持 / 版本不符）→ 保留旧会话，返回 false
        return false;
    }
}

std::vector<float> ONNXruntime::infer(const std::vector<float>& features, int timeoutMs) {
    // S1. 前置：会话有效、特征非空；超时非正取内部固化默认
    if (!session_ || features.empty()) {
        return std::vector<float>();
    }
    const int timeout = (timeoutMs > 0) ? timeoutMs : kDefaultInferTimeoutMs;

    // S1 续. 维度自检：特征长度必须等于模型输入末维（维度不符 → 空列表，§3.4 S2）
    const int expectDim = inputDim();
    if (expectDim > 0 && static_cast<std::size_t>(expectDim) != features.size()) {
        return std::vector<float>();
    }

    try {
        // S2. 组装输入张量：形状 {1, 特征维}。CreateTensor 需要可写指针，
        //     故拷一份（特征维通常只有个位数，代价可忽略），避免 const_cast
        std::vector<float> inputData(features);
        const std::array<int64_t, 2> shape{1, static_cast<int64_t>(inputData.size())};
        Ort::MemoryInfo memInfo =
            Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
        Ort::Value inputTensor = Ort::Value::CreateTensor<float>(
            memInfo, inputData.data(), inputData.size(), shape.data(), shape.size());

        const char* inputNames[] = {session_->inputName.c_str()};
        const char* outputNames[] = {session_->outputName.c_str()};

        // S3. 局部 RunOptions + 一次性看护线程（D-31）
        Ort::RunOptions runOptions;
        std::mutex mutex;
        std::condition_variable cv;
        bool finished = false;

        std::thread watchdog([&]() {
            std::unique_lock<std::mutex> lock(mutex);
            const bool doneInTime = cv.wait_for(lock, std::chrono::milliseconds(timeout),
                                                [&finished] { return finished; });
            if (doneInTime) {
                return;   // 推理已正常返回，无事可做
            }
            // 到点：协作式取消。执行器在算子边界检查终止标志，
            // 故 Run 的返回是「尽快」而非「立即」（实现与文档都不得假设零延迟）
            runOptions.SetTerminate();
        });

        // S4. 同步执行；取消 / 失败都表现为异常，统一按空列表处理
        std::vector<Ort::Value> outputs;
        bool ok = true;
        try {
            outputs = session_->ort.Run(runOptions, inputNames, &inputTensor, 1, outputNames, 1);
        } catch (...) {
            ok = false;
        }

        // S6. 收尾：先唤醒看护线程（置完成标志 + notify）再 join。
        //     顺序不可颠倒——先 join 会让本线程空等满 timeout
        {
            std::lock_guard<std::mutex> lock(mutex);
            finished = true;
        }
        cv.notify_all();
        if (watchdog.joinable()) {
            watchdog.join();
        }

        if (!ok || outputs.empty() || !outputs[0].IsTensor()) {
            return std::vector<float>();   // 失败 / 超时被取消 → 空列表（D-06）
        }

        // S5. 取首个输出张量按行展开（契约侧已约定模型必须单输出）
        const auto info = outputs[0].GetTensorTypeAndShapeInfo();
        const std::size_t count = info.GetElementCount();
        const float* data = outputs[0].GetTensorData<float>();
        if (data == nullptr || count == 0) {
            return std::vector<float>();
        }
        return std::vector<float>(data, data + count);
    } catch (...) {
        // 张量构造 / 类型不符等，一律以空列表表达失败，不向热路径抛异常
        return std::vector<float>();
    }
}

int ONNXruntime::inputDim() {
    // S1. 会话无效（空会话不可用态）→ 0，调用方据此判失败
    if (!session_) {
        return 0;
    }

    try {
        // S2. 取输入形状**末维**（首维是批维，导出模型通常为 -1，取它必踩坑）
        const auto info = session_->ort.GetInputTypeInfo(0).GetTensorTypeAndShapeInfo();
        const std::vector<int64_t> shape = info.GetShape();
        if (shape.empty()) {
            return 0;
        }
        const int64_t last = shape.back();
        if (last <= 0) {
            return 0;   // 动态轴：读不出确定的特征维
        }
        return static_cast<int>(last);
    } catch (...) {
        // S3. 读取失败 → 0；无副作用（不建会话、不改 session_）
        return 0;
    }
}

} // namespace sched
