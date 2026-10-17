// SPDX-License-Identifier: MIT
// decisionMaker 实现（详细设计 §3.8 / D-10 / D-11 / D-25 / D-30 / D-31）
//
// 三段分工（§4.5.3）：ModelFile 管文件级、ONNXruntime 管会话级、本类管配对级。

#include "agent/include/DecisionMaker.h"

#include <cstddef>
#include <utility>

#include "common/include/ONNXruntime.h"
#include "data/include/ContractFile.h"
#include "data/include/ModelFile.h"

namespace sched {

namespace {

// 把模型路径的扩展名换成 .json（.../dqn.onnx -> .../dqn.json），与模型同目录同名
std::string deriveContractPath(const std::string& modelPath) {
    const std::string::size_type slash = modelPath.find_last_of('/');
    const std::string::size_type dot = modelPath.find_last_of('.');

    // 点号必须在末段内（否则是目录名里的点，不当作扩展名）
    const bool dotInName = (dot != std::string::npos) &&
                           (slash == std::string::npos || dot > slash);
    if (dotInName) {
        return modelPath.substr(0, dot) + ".json";
    }
    return modelPath + ".json";
}

} // namespace

decisionMaker::decisionMaker(const std::string& modelPath) : modelPath_(modelPath) {
    // S1. 路径仅作默认路径预置（空串即「未加载」）
    // S2. 契约为空、ready = false；**不在构造期加载模型与契约**（D-11）
}

decisionMaker::~decisionMaker() = default;
// 只释放本类的契约数据（值语义成员自动销毁）并置 ready = false；
// **不释放推理会话**——ONNXruntime::session 是进程级静态资源（D-05）

bool decisionMaker::isReady() const {
    return ready_;
}

bool decisionMaker::setModelPath(const std::string& modelPath) {
    // S1. 路径校验：空串不表示「停用」而表示「非法入参」——停用分流在 schedManager（S1）
    if (modelPath.empty()) {
        return false;
    }

    // S1.2 文件级校验：路径非空、文件可读且非空
    ModelFile modelFile;
    if (!modelFile.modelValidate(modelPath)) {
        return false;
    }

    // S2. 幂等：同路径且已就绪 → 不重建会话、不重载契约
    if (modelPath == modelPath_ && ready_) {
        return true;
    }

    // S3. **先建新会话**（此时旧会话 / 旧路径 / 旧契约仍可用）；失败即原样返回
    if (!ONNXruntime::load(modelPath)) {
        return false;
    }

    // S3.2 读出**新会话**的输入维度（末维），用于第 3 道校验
    const int inDim = ONNXruntime::inputDim();
    if (inDim <= 0) {
        // 新会话维度读不出 → 视为建会话失败，旧会话仍可用
        return false;
    }

    // S4. 派生契约路径并在**同一次调用内**加载（契约与模型成对切换）
    if (!loadContract(deriveContractPath(modelPath), inDim)) {
        // S4.1 契约不过（含第 3 道维度不匹配）→ 丢弃新会话，旧模型仍在服务
        return false;
    }

    // S5. 两步均成功 → 提交路径与就绪标志
    modelPath_ = modelPath;
    ready_ = true;
    return true;
}

bool decisionMaker::loadContract(const std::string& contractPath, int modelInputDim) {
    // S1. 前置：契约路径非空、新会话维度为正
    if (contractPath.empty() || modelInputDim <= 0) {
        return false;
    }

    // S2. 解析到**临时副本**（失败不污染已生效契约）
    ContractFile contractFile;
    Contract tmp;
    if (!contractFile.loadContract(contractPath, tmp)) {
        return false;
    }

    // S3. 第 1、2 道校验（只看契约自身）
    if (!contractFile.contractValidate(contractPath)) {
        return false;
    }

    // S4. 第 3 道校验：契约 input_dim 与新会话输入维度一致（比新会话，不比旧模型）
    if (tmp.inputDim != modelInputDim) {
        return false;
    }

    // S5. 三道全过才提交
    contract_ = std::move(tmp);
    return true;
}

Decision decisionMaker::makeDecision(const TaskInfo& taskInfo, const SystemInfo& sysInfo) {
    Decision decision;   // 默认 valid == false（未选中任何处理器 = 推理失败）

    // S1. 前置：契约与会话均已就绪
    if (!ready_) {
        return decision;
    }

    // S2. 特征编码（按契约拼接）：TODO(实现层)
    //   S2.1 遍历 contract_.inputFields——**数组顺序即模型输入位置**，唯一真相源；
    //   S2.2 source == "task" → 按 name 从 taskInfo.task 取值
    //        source == "system" → 按 name 取 sysInfo 的对应分量（ucpu / unpu /
    //        cpu_freq_khz / npu_freq_khz）
    //   S2.3 契约要求而载荷未提供的任务键（或值非数值）→ 返回无效 Decision（映射 2002）；
    //        **多余键一律忽略**
    //   S2.4 归一化两次仿射，分母**只取 rawMin / rawMax**：
    //        ① v = (x - rawMin) / (rawMax - rawMin)，rawMin == rawMax 取 0.5（不除零）
    //        ② v = min + v * (max - min)
    //        ③ 夹取到 [0,1]（越界不判失败）
    //   S2.5 **不得硬编码任何量程字面量**（D-30）
    std::vector<float> encoded;
    (void)taskInfo;
    (void)sysInfo;

    // S3. 推理：返回空列表即失败（含超时被 SetTerminate 取消，D-31）
    const std::vector<float> output = ONNXruntime::infer(encoded);
    if (output.empty()) {
        return decision;
    }

    // S4. 动作解码：argmax → 按契约 actions 查出唯一「处理器 + 目标频率」
    if (contract_.actions.empty()) {
        return decision;
    }
    std::size_t best = 0;
    for (std::size_t i = 1; i < output.size(); ++i) {
        if (output[i] > output[best]) {
            best = i;
        }
    }
    if (best >= contract_.actions.size()) {
        // 输出维与动作数不匹配（契约第 2 道校验本应挡住，此处兜底）
        return decision;
    }
    decision.action.device = contract_.actions[best].device;
    decision.action.freqKHz = contract_.actions[best].freqKHz;
    decision.valid = true;

    // S5. 无副作用，返回
    return decision;
}

} // namespace sched
