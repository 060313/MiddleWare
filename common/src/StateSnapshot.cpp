#include "StateSnapshot.h"

#include "Types.h"

#include <chrono>

namespace common {

void snapshotBeginWrite(StateSnapshot& snap) {
    // 偶 → 奇：宣告"接下来写的字段不可信"。
    // relaxed 自增 + release 栅栏：栅栏是 StoreStore 屏障，挡住 seq 这次自增
    // 与后面字段写入的重排 —— 否则读者可能读到"偶数 seq + 半新数据"的组合。
    snap.seq.fetch_add(1, std::memory_order_relaxed);
    std::atomic_thread_fence(std::memory_order_release);
}

void snapshotEndWrite(StateSnapshot& snap) {
    // 奇 → 偶：宣告"数据已稳定，可以读了"。
    // release 存储本身已足够：它保证之前所有字段写入先于本次 seq 归偶对外可见。
    snap.seq.fetch_add(1, std::memory_order_release);
}

bool snapshotTryRead(const StateSnapshot& snap, StateSnapshotValue& out) {
    constexpr int kMaxRetry = 4;

    for (int i = 0; i < kMaxRetry; ++i) {
        const uint64_t s1 = snap.seq.load(std::memory_order_acquire);

        if (s1 & 1u) continue;

        StateSnapshotValue tmp;
        tmp.ucpu = snap.ucpu;
        for (int k = 0; k < 3; ++k) tmp.unpu[k] = snap.unpu[k];
        tmp.cpuFreqKHz = snap.cpuFreqKHz;
        tmp.npuFreqKHz = snap.npuFreqKHz;
        tmp.mode       = snap.mode;
        tmp.tsNs       = snap.tsNs;

        std::atomic_thread_fence(std::memory_order_acquire);
        const uint64_t s2 = snap.seq.load(std::memory_order_relaxed);

        if (s1 == s2) { out = tmp; return true; }
    }
    return false;                        
}

void snapshotStore(StateSnapshot& snap, const SystemFeature& feature, int mode) {
    snapshotBeginWrite(snap);
    snap.ucpu = feature.ucpu;
    for (int k = 0; k < 3; ++k) snap.unpu[k] = feature.unpu[static_cast<std::size_t>(k)];
    snap.cpuFreqKHz = feature.cpuFreqKHz;
    snap.npuFreqKHz = feature.npuFreqKHz;
    snap.mode       = mode;
    snap.tsNs = std::chrono::duration_cast<std::chrono::nanoseconds>(
                    std::chrono::steady_clock::now().time_since_epoch()).count();
    snapshotEndWrite(snap);
}

}  // namespace common
