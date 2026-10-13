#pragma once

// ONNX Runtime 封装：加载模型并执行推理。
// 见《软件详细设计文档》5.3、6.3。
// OrtHandles 采用 pimpl：不暴露 onnxruntime 头文件，缺失依赖时仍可编译。

#include <chrono>
#include <memory>
#include <optional>
#include <string>

#include "Types.h"

namespace agent {

class FeatureEncoder;
class ActionDecoder;

class DecisionMaker {
public:
    DecisionMaker();
    ~DecisionMaker();

    DecisionMaker(const DecisionMaker&) = delete;
    DecisionMaker& operator=(const DecisionMaker&) = delete;

    // 前置：modelPath 可读；后置：m_ready 表示是否可用
    bool load(const std::string& modelPath, const std::string& contractPath);

    // 前置：m_ready；超时返回 nullopt
    std::optional<common::Decision> infer(const common::TaskFeature& task,
                                          const common::SystemFeature& sys);

    bool ready() const { return m_ready; }

    void setTimeout(std::chrono::microseconds t) { m_timeout = t; }
    std::chrono::microseconds timeout() const { return m_timeout; }

private:
    struct OrtHandles;                          // ONNX Runtime 句柄（定义在 .cpp）

    std::unique_ptr<FeatureEncoder>  m_encoder;
    std::unique_ptr<ActionDecoder>   m_decoder;
    std::unique_ptr<OrtHandles>      m_ort;
    std::chrono::microseconds        m_timeout{5000};
    bool                             m_ready = false;
};

}  // namespace agent
