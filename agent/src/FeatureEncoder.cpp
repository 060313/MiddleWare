#include "FeatureEncoder.h"

#include <algorithm>
#include <cstddef>
#include <optional>

#include "Json.h"
#include "SystemFS.h"

namespace agent {
namespace {

// ---- 归一化标度 ----
// model_contract.json 的 state_bounds 给出的是 8 个分量各自的 [0,1] 上下界，
// 并未给出频率与批大小的物理量程 —— 频率比值必须在 C++ 侧确定分母。
// 此处按目标平台 RK3588 的档位上限固化；换 SoC 需同时改这里与 Python 训练侧，
// 故本组常量与 state_bounds 同属"契约冻结项"。
constexpr double kCpuFreqMaxKHz = 1800000.0;   // 1800 MHz
constexpr double kNpuFreqMaxKHz = 1000000.0;   // 1000 MHz
constexpr double kBatchSizeMax = 8.0;          // 与契约动作表中 batch_size 上限一致
constexpr double kSeqLenMax = 2048.0;

// 输入向量的分量个数，与 state_bounds.fields 逐项对应
constexpr std::size_t kFeatureCount = 8;

float clampUnit(double value) {
    if (!(value > 0.0)) {
        return 0.0f;   // 同一分支挡住 NaN
    }
    if (value > 1.0) {
        return 1.0f;
    }
    return static_cast<float>(value);
}

float normalize(double value, double scale) {
    if (!(scale > 0.0)) {
        return 0.0f;
    }
    return clampUnit(value / scale);
}

}  // namespace

bool FeatureEncoder::loadContract(const std::string& path) {
    const std::optional<std::string> text = sched::SystemFS::read(path);
    if (!text) {
        return false;   // 文件缺失：调用方进入降级模式
    }

    common::Json root;
    if (!common::Json::parse(*text, root) || !root.isObject()) {
        return false;
    }

    const common::Json& dimField = root["input_dim"];
    if (!dimField.isNumber()) {
        return false;
    }
    const int inputDim = dimField.asInt(0);
    if (inputDim <= 0) {
        return false;
    }

    // fields 若存在，其项数必须与 input_dim 一致。
    // 维度错配是"编译通过、仿真不报错、上板结果全错"的典型来源，在加载期就拦住。
    const common::Json& bounds = root["state_bounds"];
    if (bounds.isObject()) {
        const common::Json& fields = bounds["fields"];
        if (fields.isArray() && fields.size() != static_cast<std::size_t>(inputDim)) {
            return false;
        }
    }

    // 全部校验通过后才落值，保证失败时对象保持原状（§6.3.5 后置条件）
    m_inputDim = inputDim;
    return true;
}

std::vector<float> FeatureEncoder::encode(const common::TaskFeature& task,
                                          const common::SystemFeature& sys) const {
    std::vector<float> features(static_cast<std::size_t>(m_inputDim > 0 ? m_inputDim : 0), 0.0f);

    // 顺序与 model_contract.json 的 state_bounds.fields 严格一致：
    //   ucpu / unpu_core0 / unpu_core1 / unpu_core2
    //   / cpu_freq_ratio / npu_freq_ratio / batch_size_norm / seq_len_norm
    const float source[kFeatureCount] = {
        clampUnit(static_cast<double>(sys.ucpu)),
        clampUnit(static_cast<double>(sys.unpu[0])),
        clampUnit(static_cast<double>(sys.unpu[1])),
        clampUnit(static_cast<double>(sys.unpu[2])),
        normalize(static_cast<double>(sys.cpuFreqKHz), kCpuFreqMaxKHz),
        normalize(static_cast<double>(sys.npuFreqKHz), kNpuFreqMaxKHz),
        normalize(static_cast<double>(task.batchSize), kBatchSizeMax),
        normalize(static_cast<double>(task.seqLen), kSeqLenMax),
    };

    // 契约维度大于 8 时多出的分量留 0；小于 8 时截断 —— 两种情况都不越界。
    const std::size_t count = std::min(features.size(), kFeatureCount);
    for (std::size_t i = 0; i < count; ++i) {
        features[i] = source[i];
    }
    return features;
}

}  // namespace agent
