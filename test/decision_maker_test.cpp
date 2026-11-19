// SPDX-License-Identifier: MIT
// decisionMaker 单元测试（详细设计 §3.8 / §4.4 / D-24 / D-30）
//
// 覆盖两块：
//   ① 编码 / 解码内核（codec）——纯计算，可脱离 ONNX 会话与文件系统直测；
//   ② 类本身的生命周期与失败路径（未就绪、路径非法、先建后换失败不污染）。
//
// 说明：makeDecision 的「推理成功 → 解码 → 下发」整链依赖 ONNXruntime 与真实 .onnx，
// 该两项目前在仓库里仍未实现（common/src/ONNXruntime.cpp、data/src/ContractFile.cpp），
// 故此处只覆盖到「推理失败 → 无效 Decision」之前的部分；待二者落地后补整链用例。

#include <cmath>
#include <cstdio>
#include <fstream>
#include <limits>
#include <string>
#include <vector>

#include "agent/include/DecisionMaker.h"
#include "agent/include/DecisionMakerCodec.h"
#include "test/test_util.h"

using namespace sched;

namespace {

InputField field(const std::string& name, const std::string& source, double rawMin,
                 double rawMax, double min = 0.0, double max = 1.0) {
    InputField f;
    f.name = name;
    f.source = source;
    f.rawMin = rawMin;
    f.rawMax = rawMax;
    f.min = min;
    f.max = max;
    return f;
}

ActionSpec action(const std::string& device, int freqKHz) {
    ActionSpec a;
    a.device = device;
    a.freqKHz = freqKHz;
    return a;
}

// §4.4 的九维示例（任务段 5 项 + 系统段 4 项）
Contract nineDimContract() {
    Contract c;
    c.inputFields = {
        field("phase", "task", 0, 1),
        field("prompt_len", "task", 0, 8192),
        field("batch_tokens", "task", 0, 2048),
        field("n_remaining", "task", 0, 8192),
        field("kv_ratio", "task", 0, 1),
        field("ucpu", "system", 0, 1),
        field("unpu", "system", 0, 1),
        field("cpu_freq_khz", "system", 408000, 2256000),
        field("npu_freq_khz", "system", 300000, 1000000),
    };
    c.inputDim = static_cast<int>(c.inputFields.size());
    c.actions = {action("cpu", 1228800), action("cpu", 1800000), action("npu", 1000000)};
    c.outputDim = static_cast<int>(c.actions.size());
    return c;
}

TaskInfo nineDimTask() {
    TaskInfo t;
    t.requestId = "req-1";
    t.task["phase"] = 0.5;
    t.task["prompt_len"] = 4096;
    t.task["batch_tokens"] = 1024;
    t.task["n_remaining"] = 2048;
    t.task["kv_ratio"] = 0.25;
    return t;
}

SystemInfo nineDimSystem() {
    SystemInfo s;
    s.ucpu = 0.5;
    s.unpu = 0.25;
    s.cpuFreqKHz = 1228800;
    s.npuFreqKHz = 600000;
    return s;
}

} // namespace

// ---------------------------------------------------------------------------
// ① 编码内核
// ---------------------------------------------------------------------------

TEST_CASE(encode_follows_contract_order) {
    // 顺序只由契约数组决定：即使「系统量夹在任务量中间」也照契约排
    Contract c;
    c.inputFields = {field("a", "task", 0, 1), field("ucpu", "system", 0, 1),
                     field("b", "task", 0, 1)};
    c.inputDim = 3;

    TaskInfo t;
    t.task["a"] = 0.1;
    t.task["b"] = 0.9;

    SystemInfo s;
    s.ucpu = 0.5;

    std::vector<float> out;
    CHECK(codec::encodeFeatures(c, t, s, out));
    CHECK_EQ(out.size(), static_cast<std::size_t>(3));
    CHECK_NEAR(out[0], 0.1, 1e-6);
    CHECK_NEAR(out[1], 0.5, 1e-6);
    CHECK_NEAR(out[2], 0.9, 1e-6);
}

TEST_CASE(encode_normalizes_with_contract_scale) {
    const Contract c = nineDimContract();
    std::vector<float> out;
    CHECK(codec::encodeFeatures(c, nineDimTask(), nineDimSystem(), out));
    CHECK_EQ(out.size(), static_cast<std::size_t>(9));

    CHECK_NEAR(out[0], 0.5, 1e-6);                        // phase 0.5/1
    CHECK_NEAR(out[1], 0.5, 1e-6);                        // 4096/8192
    CHECK_NEAR(out[2], 0.5, 1e-6);                        // 1024/2048
    CHECK_NEAR(out[3], 0.25, 1e-6);                       // 2048/8192
    CHECK_NEAR(out[4], 0.25, 1e-6);                       // kv_ratio 0.25/1
    CHECK_NEAR(out[5], 0.5, 1e-6);                        // ucpu
    CHECK_NEAR(out[6], 0.25, 1e-6);                       // unpu
    CHECK_NEAR(out[7], 820800.0 / 1848000.0, 1e-6);       // (1228800-408000)/(2256000-408000)
    CHECK_NEAR(out[8], 300000.0 / 700000.0, 1e-6);        // (600000-300000)/(1000000-300000)
}

TEST_CASE(encode_missing_task_key_fails) {
    const Contract c = nineDimContract();
    TaskInfo t = nineDimTask();
    t.task.erase("kv_ratio");

    std::vector<float> out;
    CHECK(!codec::encodeFeatures(c, t, nineDimSystem(), out));
}

TEST_CASE(encode_unknown_system_field_fails) {
    Contract c = nineDimContract();
    c.inputFields[5].name = "gpu_util";   // 契约里写了个运行时不存在的分量
    c.inputDim = static_cast<int>(c.inputFields.size());

    std::vector<float> out;
    CHECK(!codec::encodeFeatures(c, nineDimTask(), nineDimSystem(), out));
}

TEST_CASE(encode_unknown_source_fails) {
    Contract c;
    c.inputFields = {field("a", "sensor", 0, 1)};
    c.inputDim = 1;

    TaskInfo t;
    t.task["a"] = 0.5;

    std::vector<float> out;
    CHECK(!codec::encodeFeatures(c, t, SystemInfo{}, out));
}

TEST_CASE(encode_degenerate_span_uses_midpoint) {
    Contract c;
    c.inputFields = {field("const_a", "task", 7, 7), field("const_b", "task", 9, 3)};
    c.inputDim = 2;

    TaskInfo t;
    t.task["const_a"] = 7;
    t.task["const_b"] = 5;

    std::vector<float> out;
    CHECK(codec::encodeFeatures(c, t, SystemInfo{}, out));
    CHECK_NEAR(out[0], 0.5, 1e-6);   // raw_max == raw_min → 中值，不除零
    CHECK_NEAR(out[1], 0.5, 1e-6);   // raw_max < raw_min 同属退化
}

TEST_CASE(encode_clamps_out_of_range) {
    Contract c;
    c.inputFields = {field("lo", "task", 0, 100), field("hi", "task", 0, 100)};
    c.inputDim = 2;

    TaskInfo t;
    t.task["lo"] = -50;    // 低于量程
    t.task["hi"] = 150;    // 高于量程

    std::vector<float> out;
    CHECK(codec::encodeFeatures(c, t, SystemInfo{}, out));   // 越界不判失败
    CHECK_NEAR(out[0], 0.0, 1e-6);
    CHECK_NEAR(out[1], 1.0, 1e-6);
}

TEST_CASE(encode_affine_to_target_range) {
    Contract c;
    c.inputFields = {field("lo", "task", 0, 10, 0.2, 0.8), field("hi", "task", 0, 10, 0.2, 0.8)};
    c.inputDim = 2;

    TaskInfo t;
    t.task["lo"] = 0;      // 量程下界 → 0.2
    t.task["hi"] = 10;     // 量程上界 → 0.8

    std::vector<float> out;
    CHECK(codec::encodeFeatures(c, t, SystemInfo{}, out));
    CHECK_NEAR(out[0], 0.2, 1e-6);
    CHECK_NEAR(out[1], 0.8, 1e-6);
}

TEST_CASE(encode_non_finite_value_fails) {
    Contract c;
    c.inputFields = {field("a", "task", 0, 1)};
    c.inputDim = 1;

    TaskInfo nanTask;
    nanTask.task["a"] = std::nan("");
    std::vector<float> out;
    CHECK(!codec::encodeFeatures(c, nanTask, SystemInfo{}, out));

    TaskInfo infTask;
    infTask.task["a"] = std::numeric_limits<double>::infinity();
    CHECK(!codec::encodeFeatures(c, infTask, SystemInfo{}, out));
}

TEST_CASE(encode_extra_task_keys_ignored) {
    const Contract c = nineDimContract();
    TaskInfo t = nineDimTask();
    t.task["unused_key"] = 1234.5;   // 契约没声明 → 忽略

    std::vector<float> out;
    CHECK(codec::encodeFeatures(c, t, nineDimSystem(), out));
    CHECK_EQ(out.size(), static_cast<std::size_t>(9));
}

TEST_CASE(encode_rejects_broken_contract_dim) {
    Contract c = nineDimContract();
    c.inputDim = 8;   // 与 inputFields 项数（9）不一致

    std::vector<float> out;
    CHECK(!codec::encodeFeatures(c, nineDimTask(), nineDimSystem(), out));
}

// ---------------------------------------------------------------------------
// ① 解码内核
// ---------------------------------------------------------------------------

TEST_CASE(decode_picks_argmax) {
    const Contract c = nineDimContract();   // 3 个动作

    const Decision d = codec::decodeAction(c, {0.1f, 0.9f, 0.3f});
    CHECK(d.valid);
    CHECK_EQ(d.action.device, std::string("cpu"));
    CHECK_EQ(d.action.freqKHz, 1800000);

    const Decision d2 = codec::decodeAction(c, {0.1f, 0.2f, 0.7f});
    CHECK(d2.valid);
    CHECK_EQ(d2.action.device, std::string("npu"));
    CHECK_EQ(d2.action.freqKHz, 1000000);
}

TEST_CASE(decode_tie_picks_lowest_index) {
    const Contract c = nineDimContract();
    const Decision d = codec::decodeAction(c, {0.5f, 0.5f, 0.5f});
    CHECK(d.valid);
    CHECK_EQ(d.action.freqKHz, 1228800);   // 编号最小的动作
}

TEST_CASE(decode_empty_output_invalid) {
    const Contract c = nineDimContract();
    CHECK(!codec::decodeAction(c, {}).valid);
}

TEST_CASE(decode_all_non_finite_invalid) {
    const Contract c = nineDimContract();
    const float nan = std::nan("");
    CHECK(!codec::decodeAction(c, {nan, nan, nan}).valid);
}

TEST_CASE(decode_size_mismatch_invalid) {
    const Contract c = nineDimContract();   // 3 个动作
    CHECK(!codec::decodeAction(c, {0.1f, 0.2f, 0.3f, 0.4f}).valid);
    CHECK(!codec::decodeAction(c, {0.1f, 0.2f}).valid);
}

TEST_CASE(decode_empty_actions_invalid) {
    Contract c;
    c.inputDim = 1;
    c.inputFields = {field("a", "task", 0, 1)};
    c.outputDim = 0;
    CHECK(!codec::decodeAction(c, {1.0f}).valid);
}

// ---------------------------------------------------------------------------
// ② 类生命周期与失败路径
// ---------------------------------------------------------------------------

TEST_CASE(maker_not_ready_initially) {
    decisionMaker maker;
    CHECK(!maker.isReady());
}

TEST_CASE(make_decision_without_ready_returns_invalid) {
    decisionMaker maker;
    const Decision d = maker.makeDecision(nineDimTask(), nineDimSystem());
    CHECK(!d.valid);
    CHECK_EQ(d.action.device, std::string());
    CHECK_EQ(d.action.freqKHz, 0);
}

TEST_CASE(set_model_path_rejects_empty) {
    decisionMaker maker;
    CHECK(!maker.setModelPath(""));
    CHECK(!maker.isReady());
}

TEST_CASE(set_model_path_rejects_missing_file) {
    decisionMaker maker;
    CHECK(!maker.setModelPath("no_such_dir/no_such_model.onnx"));
    CHECK(!maker.isReady());
}

TEST_CASE(set_model_path_failure_keeps_not_ready) {
    // 文件级校验能过（存在且非空），但会话建不起来（内容不是合法 ONNX）
    // —— 验证「先建后换」失败时就绪标志不转真（旧状态原封不动）
    const std::string path = "test_tmp_invalid_model.onnx";
    {
        std::ofstream ofs(path, std::ios::binary | std::ios::trunc);
        ofs << "not a real onnx model";
    }

    decisionMaker maker;
    CHECK(!maker.setModelPath(path));
    CHECK(!maker.isReady());

    std::remove(path.c_str());
}

int main() {
    RUN(encode_follows_contract_order);
    RUN(encode_normalizes_with_contract_scale);
    RUN(encode_missing_task_key_fails);
    RUN(encode_unknown_system_field_fails);
    RUN(encode_unknown_source_fails);
    RUN(encode_degenerate_span_uses_midpoint);
    RUN(encode_clamps_out_of_range);
    RUN(encode_affine_to_target_range);
    RUN(encode_non_finite_value_fails);
    RUN(encode_extra_task_keys_ignored);
    RUN(encode_rejects_broken_contract_dim);

    RUN(decode_picks_argmax);
    RUN(decode_tie_picks_lowest_index);
    RUN(decode_empty_output_invalid);
    RUN(decode_all_non_finite_invalid);
    RUN(decode_size_mismatch_invalid);
    RUN(decode_empty_actions_invalid);

    RUN(maker_not_ready_initially);
    RUN(make_decision_without_ready_returns_invalid);
    RUN(set_model_path_rejects_empty);
    RUN(set_model_path_rejects_missing_file);
    RUN(set_model_path_failure_keeps_not_ready);

    return test::summary();
}
