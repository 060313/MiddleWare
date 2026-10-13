#include "ActionDecoder.h"

#include <cstddef>
#include <optional>
#include <utility>

#include "Json.h"
#include "SystemFS.h"

namespace agent {
namespace {

// 档位序号 → 具体频率（kHz）。契约只声明"第几档"，物理频率在 C++ 侧固化。
// 取值与目标平台 RK3588 的 devfreq available_frequencies 对齐；
// SystemManager::applyAction 仍会用硬件实际档位表再夹取一次（双保险，防契约与板子不一致）。
// 序号越出本表长度时返回 0，即"不改频"（DeviceAction 的约定值）。
constexpr int kCpuFreqLadder[] = {600000, 1200000, 1800000};
constexpr int kNpuFreqLadder[] = {600000, 800000, 1000000};

constexpr std::size_t kCpuLadderSize = sizeof(kCpuFreqLadder) / sizeof(kCpuFreqLadder[0]);
constexpr std::size_t kNpuLadderSize = sizeof(kNpuFreqLadder) / sizeof(kNpuFreqLadder[0]);

int levelToFreq(int level, const int* ladder, std::size_t size) {
    if (level < 0 || static_cast<std::size_t>(level) >= size) {
        return 0;
    }
    return ladder[level];
}

}  // namespace

bool ActionDecoder::loadContract(const std::string& path) {
    const std::optional<std::string> text = sched::SystemFS::read(path);
    if (!text) {
        return false;
    }

    common::Json root;
    if (!common::Json::parse(*text, root) || !root.isObject()) {
        return false;
    }

    const common::Json& dimField = root["output_dim"];
    if (!dimField.isNumber()) {
        return false;
    }
    const int actionCount = dimField.asInt(0);
    if (actionCount <= 0) {
        return false;
    }

    // 动作表项数必须等于 output_dim：否则 Q 值下标与动作语义错位，
    // 表现为"能跑、但选中的动作与训练时不是同一个"。
    const common::Json& actions = root["actions"];
    if (!actions.isArray() || actions.size() != static_cast<std::size_t>(actionCount)) {
        return false;
    }

    std::vector<ActionSpec> parsed;
    parsed.reserve(actions.size());
    for (std::size_t i = 0; i < actions.size(); ++i) {
        const common::Json& item = actions[i];
        if (!item.isObject()) {
            return false;
        }
        ActionSpec spec;
        spec.index = item["index"].asInt(static_cast<int>(i));
        spec.name = item["name"].asString();
        spec.cpuShare = item["cpu_share"].asNumber(0.0);
        spec.batchSize = item["batch_size"].asInt(0);
        spec.cpuFreqLevel = item["cpu_freq_level"].asInt(0);
        spec.npuFreqLevel = item["npu_freq_level"].asInt(0);
        parsed.push_back(std::move(spec));
    }

    // 校验全部通过后才落值
    m_actionCount = actionCount;
    m_actions = std::move(parsed);
    return true;
}

common::Decision ActionDecoder::decode(const std::vector<float>& qValues) const {
    common::Decision decision;   // actionIndex = -1 表示无效
    if (qValues.empty()) {
        return decision;
    }
    // 两处长度校验：前者防模型输出与契约不符，后者防"忘了先 loadContract"。
    // 任一不成立都不猜一个动作出来 —— DQN 会把错误动作当作已生效继续学习。
    if (static_cast<int>(qValues.size()) != m_actionCount) {
        return decision;
    }
    if (m_actions.size() != qValues.size()) {
        return decision;
    }

    // argmax：并列时取序号小者（用 > 而非 >=），保证同一 Q 向量多次解码结果一致
    std::size_t best = 0;
    for (std::size_t i = 1; i < qValues.size(); ++i) {
        if (qValues[i] > qValues[best]) {
            best = i;
        }
    }

    decision.actionIndex = static_cast<int>(best);
    decision.qValue = static_cast<double>(qValues[best]);

    const ActionSpec& spec = m_actions[best];

    // NPU 动作总是下发：调度目标本身就是 NPU 侧的批处理与调频
    common::DeviceAction npu;
    npu.device = "npu";
    npu.freqKHz = levelToFreq(spec.npuFreqLevel, kNpuFreqLadder, kNpuLadderSize);
    npu.batchSize = spec.batchSize;
    decision.actions.push_back(std::move(npu));

    // CPU 动作仅在本动作确实占 CPU 份额时下发
    // （cpu_share = 0 表示把 CPU 完全让给其他负载，此时不该去动它的频率）
    if (spec.cpuShare > 0.0) {
        common::DeviceAction cpu;
        cpu.device = "cpu";
        cpu.freqKHz = levelToFreq(spec.cpuFreqLevel, kCpuFreqLadder, kCpuLadderSize);
        cpu.batchSize = spec.batchSize;
        decision.actions.push_back(std::move(cpu));
    }

    return decision;
}

}  // namespace agent
