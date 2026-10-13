#include "DecisionMaker.h"

#include "ActionDecoder.h"
#include "FeatureEncoder.h"

namespace agent {

// ONNX Runtime 句柄的真实定义（SCHED_HAS_ONNX 时持有 Ort::Env / Ort::Session）
struct DecisionMaker::OrtHandles {
    // TODO: Ort::Env env; Ort::Session session; Ort::MemoryInfo memInfo;
};

DecisionMaker::DecisionMaker()
    : m_encoder(std::make_unique<FeatureEncoder>()),
      m_decoder(std::make_unique<ActionDecoder>()),
      m_ort(nullptr) {}

DecisionMaker::~DecisionMaker() = default;

bool DecisionMaker::load(const std::string& modelPath, const std::string& contractPath) {
    // TODO: 加载契约（m_encoder/m_decoder->loadContract）；
    //       建 Ort::Session；按 input_dim 校验模型输入维度；置 m_ready
    (void)modelPath;
    (void)contractPath;
    return false;
}

std::optional<common::Decision> DecisionMaker::infer(const common::TaskFeature& task,
                                                     const common::SystemFeature& sys) {
    // TODO: encode → session.Run → 取 Q 值 → decode(argmax)；超时返回 nullopt
    (void)task;
    (void)sys;
    return std::nullopt;
}

}  // namespace agent
