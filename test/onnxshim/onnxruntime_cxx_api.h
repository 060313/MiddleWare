// SPDX-License-Identifier: MIT
// ONNX Runtime C++ API 的最小仿真件 —— **仅供 -fsyntax-only 语法检查**，不参与任何构建
//
// 用途：ONNXruntime.cpp 硬依赖 /opt/onnxruntime 的 SDK 头，无 SDK 的机器编不过。
//       本文件以同名头提供**签名一致**的声明，让 make check-onnx 能在任意机器上验证
//       该文件的语法与类型使用；真实链接与推理行为须在装好 SDK 的容器 / 真机上验证。
// 口径：只声明本工程实际用到的符号，不做任何实现。

#ifndef SCHED_ONNXSHIM_ONNXRUNTIME_CXX_API_H
#define SCHED_ONNXSHIM_ONNXRUNTIME_CXX_API_H

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#define ORT_LOGGING_LEVEL_WARNING 2

enum OrtAllocatorType { OrtArenaAllocator = 0, OrtDeviceAllocator = 1 };
enum OrtMemType { OrtMemTypeDefault = 0 };

namespace Ort {

class Exception : public std::exception {
public:
    const char* what() const noexcept override { return "Ort::Exception"; }
};

// 由分配器托管的字符串指针（GetInputNameAllocated 的返回类型）
class AllocatedStringPtr {
public:
    const char* get() const { return ptr_; }

private:
    const char* ptr_ = nullptr;
};

class AllocatorWithDefaultOptions {
public:
    AllocatorWithDefaultOptions() = default;
};

class Env {
public:
    Env(int loggingLevel, const char* name);
};

class SessionOptions {
public:
    SessionOptions() = default;
    void SetIntraOpNumThreads(int n);
};

class RunOptions {
public:
    RunOptions() = default;
    void SetTerminate();
};

class MemoryInfo {
public:
    static MemoryInfo CreateCpu(OrtAllocatorType type, OrtMemType memType);
};

class TensorTypeAndShapeInfo {
public:
    std::size_t GetElementCount() const;
    std::vector<std::int64_t> GetShape() const;
};

class TypeInfo {
public:
    TensorTypeAndShapeInfo GetTensorTypeAndShapeInfo() const;
};

class Value {
public:
    template <typename T>
    static Value CreateTensor(const MemoryInfo& info, T* data, std::size_t count,
                              const std::int64_t* shape, std::size_t shapeLen);

    TensorTypeAndShapeInfo GetTensorTypeAndShapeInfo() const;

    template <typename T>
    T* GetTensorMutableData();
};

class Session {
public:
    Session(const Env& env, const char* modelPath, const SessionOptions& options);

    AllocatedStringPtr GetInputNameAllocated(std::size_t index,
                                             const AllocatorWithDefaultOptions& alloc) const;
    AllocatedStringPtr GetOutputNameAllocated(std::size_t index,
                                              const AllocatorWithDefaultOptions& alloc) const;
    TypeInfo GetInputTypeInfo(std::size_t index) const;

    std::vector<Value> Run(const RunOptions& runOptions, const char* const* inputNames,
                           const Value* inputValues, std::size_t inputCount,
                           const char* const* outputNames, std::size_t outputCount);
};

} // namespace Ort

#endif // SCHED_ONNXSHIM_ONNXRUNTIME_CXX_API_H
