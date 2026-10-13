#pragma once

// 运行时共享内存状态快照（POD 布局，seqlock 双写序号）。
// 见《软件详细设计文档》6.3 / 7.4：监控线程写、决策线程读，无锁零拷贝。

#include <atomic>
#include <cstdint>

namespace common {

// 共享内存中的快照本体（不可拷贝：含 atomic 成员）
struct StateSnapshot {
    std::atomic<uint64_t> seq{0};   // 写前 +1（奇数 = 写入中），写后 +1（偶数 = 稳定）
    float   ucpu = 0.0f;
    float   unpu[3] = {0.0f, 0.0f, 0.0f};
    int     cpuFreqKHz = 0;
    int     npuFreqKHz = 0;
    int     mode = 0;               // static_cast<int>(SchedMode)
    int64_t tsNs = 0;
};

// 读取结果（拷贝友好）
struct StateSnapshotValue {
    float   ucpu = 0.0f;
    float   unpu[3] = {0.0f, 0.0f, 0.0f};
    int     cpuFreqKHz = 0;
    int     npuFreqKHz = 0;
    int     mode = 0;
    int64_t tsNs = 0;
};

// ---- seqlock 辅助 ----
void snapshotBeginWrite(StateSnapshot& snap);
void snapshotEndWrite(StateSnapshot& snap);

// 读前 / 读后 seq 一致且为偶数才算有效
bool snapshotTryRead(const StateSnapshot& snap, StateSnapshotValue& out);

// 写入辅助：由 SystemFeature 填充快照
void snapshotStore(StateSnapshot& snap, const struct SystemFeature& feature, int mode);

}  // namespace common
