#include "StateSnapshot.h"

#include "Types.h"

namespace common {

void snapshotBeginWrite(StateSnapshot& snap) {
    // TODO(seqlock): seq 自增为奇数，标记"写入中"
    (void)snap;
}

void snapshotEndWrite(StateSnapshot& snap) {
    // TODO(seqlock): seq 自增为偶数，标记"稳定"
    (void)snap;
}

bool snapshotTryRead(const StateSnapshot& snap, StateSnapshotValue& out) {
    // TODO(seqlock): 先读 seq，读数据后再读 seq，两次一致且为偶数才写 out 并返回 true
    (void)snap;
    (void)out;
    return false;
}

void snapshotStore(StateSnapshot& snap, const SystemFeature& feature, int mode) {
    // TODO(seqlock): beginWrite -> 填字段 -> endWrite
    (void)snap;
    (void)feature;
    (void)mode;
}

}  // namespace common
