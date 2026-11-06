// SPDX-License-Identifier: MIT
// deviceFile 单元测试 · 测试用例（实现）
//
// 组织约定：
//   - 每个用例一个函数，命名 test<关注点>，函数体自洽：自建伪节点树 → 构造被测对象
//     → 断言。**被测对象就地构造**，不设共享实例，避免用例间隐式耦合；
//   - 只通过桩模块（deviceFile_stub.h）提供的探针与夹具触碰内部状态，
//     不直接读保护成员、不改生产代码；
//   - 断言失败只报告不中断（test_util.h 的 CHECK 语义），一次跑完看全貌。
//
// 范围与编号见 deviceFile_cases.h 与《测试用例说明.md》。

#include "deviceFile_cases.h"

#include <string>

#include "deviceFile_stub.h"

namespace sched {
namespace test {
namespace devicefile {

// ---------------------------------------------------------------------------
// 接口（IF）
// ---------------------------------------------------------------------------

// IF-01：构造把四个注入值原样保存，freqTable_ 为空
void testCtorStoresInjectedValues() {
    test::section("IF-01 构造：四个注入值原样保存，频率表为空");
    const std::string dir = caseDir("if01");
    DeviceFileProbe dev(dir, dir + "/stat", 1200000, 2400000);

    CHECK(dev.freqPath() == dir);
    CHECK(dev.statPath() == dir + "/stat");
    CHECK(dev.freqMin() == 1200000);
    CHECK(dev.freqMax() == 2400000);
    CHECK(dev.table().empty());
}

// IF-02：构造期不读任何节点（懒加载），首次显式调用才装载
void testCtorDoesNotReadNodes() {
    test::section("IF-02 构造不读节点：节点存在时表仍为空");
    const std::string dir = caseDir("if02");
    put(dir + "/available_frequencies", "1000000 2000000\n");

    DeviceFileProbe dev(dir, dir);
    CHECK(dev.table().empty());   // 若构造期读过，这里会非空

    LockProbe lock(dev);
    CHECK(dev.readFreqTable(1, "available_frequencies", lock));
    CHECK_TABLE(dev.table(), Ints({1000000, 2000000}));
}

// IF-03：基类只拼 freqPath_ + "/" + freqNode，不碰 statPath_
void testBaseReadsFreqPathNotStatPath() {
    test::section("IF-03 只读 freqPath：statPath 下放同名诱饵节点");
    const std::string freqDir = caseDir("if03/freq");
    const std::string statDir = caseDir("if03/stat");
    put(freqDir + "/available_frequencies", "1000000 2000000\n");
    put(statDir + "/available_frequencies", "9000000\n");   // 诱饵

    DeviceFileProbe dev(freqDir, statDir);
    LockProbe lock(dev);
    CHECK(dev.readFreqTable(1, "available_frequencies", lock));
    CHECK_TABLE(dev.table(), Ints({1000000, 2000000}));
    CHECK(lock.calls() == 1);
}

// IF-04：路径原样保存（不规整）；尾斜杠可读、空路径读失败
void testPathsStoredVerbatim() {
    test::section("IF-04 路径原样保存：尾斜杠 / 空串");
    const std::string dir = caseDir("if04");
    put(dir + "/available_frequencies", "1000000\n");

    DeviceFileProbe withSlash(dir + "/", "", 0, 0);
    CHECK(withSlash.freqPath() == dir + "/");
    CHECK(withSlash.statPath().empty());

    LockProbe slashLock(withSlash);
    CHECK(withSlash.readFreqTable(1, "available_frequencies", slashLock));
    CHECK_TABLE(withSlash.table(), Ints({1000000}));

    DeviceFileProbe emptyPath("", "", 0, 0);
    LockProbe emptyLock(emptyPath);
    CHECK(!emptyPath.readFreqTable(1, "available_frequencies", emptyLock));
    CHECK(emptyLock.calls() == 0);
    CHECK(emptyPath.table().empty());
}

// ---------------------------------------------------------------------------
// 数据结构（DS）
// ---------------------------------------------------------------------------

// DS-01：解析结果升序、去重、全为正
void testTableSortedUniquePositive() {
    test::section("DS-01 表不变量：乱序 + 重复 → 升序去重");
    const std::string dir = caseDir("ds01");
    put(dir + "/available_frequencies", "3000 1000 2000 1000\n");

    DeviceFileProbe dev(dir, dir);
    LockProbe lock(dev);
    CHECK(dev.readFreqTable(1, "available_frequencies", lock));
    CHECK_TABLE(dev.table(), Ints({1000, 2000, 3000}));
    CHECK(sortedUniquePositive(dev.table()));
}

// DS-02：0 与负值不是合法档位，直接剔除
void testNonPositiveDropped() {
    test::section("DS-02 非正值剔除");
    const std::string dir = caseDir("ds02");
    put(dir + "/available_frequencies", "0 -1000 2000\n");

    DeviceFileProbe dev(dir, dir);
    LockProbe lock(dev);
    CHECK(dev.readFreqTable(1, "available_frequencies", lock));
    CHECK_TABLE(dev.table(), Ints({2000}));
}

// DS-03：重复装载是覆盖，不是追加
void testReloadOverwrites() {
    test::section("DS-03 重复装载覆盖旧表");
    const std::string dir = caseDir("ds03");
    put(dir + "/available_frequencies", "1000 2000 3000\n");

    DeviceFileProbe dev(dir, dir);
    LockProbe firstLock(dev);
    CHECK(dev.readFreqTable(1, "available_frequencies", firstLock));
    CHECK_TABLE(dev.table(), Ints({1000, 2000, 3000}));

    put(dir + "/available_frequencies", "5000\n");
    LockProbe secondLock(dev);
    CHECK(dev.readFreqTable(1, "available_frequencies", secondLock));
    CHECK_TABLE(dev.table(), Ints({5000}));   // 不是 {1000,2000,3000,5000}
}

// DS-04：装载失败保留旧表（设备抖动时不能被清空）
void testFailurePreservesTable() {
    test::section("DS-04 装载失败保留旧表");
    const std::string dir = caseDir("ds04");
    const std::string node = dir + "/available_frequencies";
    put(node, "1000 2000\n");

    DeviceFileProbe dev(dir, dir);
    LockProbe firstLock(dev);
    CHECK(dev.readFreqTable(1, "available_frequencies", firstLock));
    CHECK_TABLE(dev.table(), Ints({1000, 2000}));

    dropNode(node);   // 模拟节点消失 / 读取失败
    LockProbe secondLock(dev);
    CHECK(!dev.readFreqTable(1, "available_frequencies", secondLock));
    CHECK_TABLE(dev.table(), Ints({1000, 2000}));   // 旧表原封不动
    CHECK(secondLock.calls() == 0);                 // 失败路径不取锁
}

// DS-05：收窄保留边界值（f == min、f == max 都要留）
void testNarrowKeepsBoundaries() {
    test::section("DS-05 收窄边界：等于上下限的档位保留");
    const std::string dir = caseDir("ds05");
    put(dir + "/available_frequencies", "1000 2000 3000\n");

    DeviceFileProbe dev(dir, dir, 2000, 3000);
    LockProbe lock(dev);
    CHECK(dev.readFreqTable(1, "available_frequencies", lock));
    CHECK_TABLE(dev.table(), Ints({2000, 3000}));
}

// DS-06：单侧收窄；另一侧为 0 表示不限
void testOneSidedNarrow() {
    test::section("DS-06 单侧收窄：另一侧 0 不限");
    const std::string dir = caseDir("ds06");
    put(dir + "/available_frequencies", "1000 2000 3000\n");

    DeviceFileProbe minOnly(dir, dir, 2000, 0);
    LockProbe minLock(minOnly);
    CHECK(minOnly.readFreqTable(1, "available_frequencies", minLock));
    CHECK_TABLE(minOnly.table(), Ints({2000, 3000}));

    DeviceFileProbe maxOnly(dir, dir, 0, 2000);
    LockProbe maxLock(maxOnly);
    CHECK(maxOnly.readFreqTable(1, "available_frequencies", maxLock));
    CHECK_TABLE(maxOnly.table(), Ints({1000, 2000}));
}

// DS-07：负收窄值按“不限”处理（越界入参的特征化，设计前置条件是 >= 0）
void testNegativeNarrowTreatedAsUnbounded() {
    test::section("DS-07 负收窄值 → 该侧不参与过滤");
    const std::string dir = caseDir("ds07");
    put(dir + "/available_frequencies", "1000 2000 3000\n");

    DeviceFileProbe dev(dir, dir, -5, -1);
    LockProbe lock(dev);
    CHECK(dev.readFreqTable(1, "available_frequencies", lock));
    CHECK_TABLE(dev.table(), Ints({1000, 2000, 3000}));
}

// ---------------------------------------------------------------------------
// 执行路径（EP）
// ---------------------------------------------------------------------------

// EP-01：全量装载 —— 返回 true、锁 1 次、写入发生在锁内
void testFullLoad() {
    test::section("EP-01 全量装载：true + 锁 1 次 + 写表在锁内");
    const std::string dir = caseDir("ep01");
    put(dir + "/available_frequencies", "1000000 2000000\n");

    DeviceFileProbe dev(dir, dir);
    LockProbe lock(dev);
    CHECK(dev.readFreqTable(1, "available_frequencies", lock));
    CHECK(lock.calls() == 1);
    CHECK(lock.tableSizeInsideLock() == 2);   // 回调返回前表已写入 ⇒ 写在锁内
    CHECK_TABLE(dev.table(), Ints({1000000, 2000000}));
}

// EP-02：部分收窄 —— 返回 true，表为子集
void testPartialNarrow() {
    test::section("EP-02 部分收窄：true + 子集");
    const std::string dir = caseDir("ep02");
    put(dir + "/available_frequencies", "1000000 2000000 3000000\n");

    DeviceFileProbe dev(dir, dir, 1500000, 2500000);
    LockProbe lock(dev);
    CHECK(dev.readFreqTable(1, "available_frequencies", lock));
    CHECK_TABLE(dev.table(), Ints({2000000}));
}

// EP-03：全部低于下限 —— 返回 false，但保留未收窄整表
void testAllBelowMinDegenerates() {
    test::section("EP-03 全部低于 min：false + 整表");
    const std::string dir = caseDir("ep03");
    put(dir + "/available_frequencies", "1000000 2000000 3000000\n");

    DeviceFileProbe dev(dir, dir, 5000000, 6000000);
    LockProbe lock(dev);
    CHECK(!dev.readFreqTable(1, "available_frequencies", lock));
    CHECK_TABLE(dev.table(), Ints({1000000, 2000000, 3000000}));
    CHECK(lock.calls() == 1);
}

// EP-04：全部高于上限 —— 返回 false，保留整表
void testAllAboveMaxDegenerates() {
    test::section("EP-04 全部高于 max：false + 整表");
    const std::string dir = caseDir("ep04");
    put(dir + "/available_frequencies", "1000000 2000000 3000000\n");

    DeviceFileProbe dev(dir, dir, 0, 500000);
    LockProbe lock(dev);
    CHECK(!dev.readFreqTable(1, "available_frequencies", lock));
    CHECK_TABLE(dev.table(), Ints({1000000, 2000000, 3000000}));
    CHECK(lock.calls() == 1);
}

// EP-05：min > max 的矛盾配置 —— 同样退化，返回 false
void testContradictoryBoundsDegenerate() {
    test::section("EP-05 min > max：false + 整表");
    const std::string dir = caseDir("ep05");
    put(dir + "/available_frequencies", "1000000 2000000 3000000\n");

    DeviceFileProbe dev(dir, dir, 3000000, 1000000);
    LockProbe lock(dev);
    CHECK(!dev.readFreqTable(1, "available_frequencies", lock));
    CHECK_TABLE(dev.table(), Ints({1000000, 2000000, 3000000}));
    CHECK(lock.calls() == 1);
}

// EP-06：节点缺失 —— false、不取锁、表保持原状
void testMissingNode() {
    test::section("EP-06 节点缺失：false + 不取锁");
    const std::string dir = caseDir("ep06");

    DeviceFileProbe dev(dir, dir);
    LockProbe lock(dev);
    CHECK(!dev.readFreqTable(1, "available_frequencies", lock));
    CHECK(lock.calls() == 0);
    CHECK(dev.table().empty());
}

// EP-07：空文件 —— 与缺失同义（read 回空串）
void testEmptyNode() {
    test::section("EP-07 空文件：false + 不取锁");
    const std::string dir = caseDir("ep07");
    put(dir + "/available_frequencies", "");

    DeviceFileProbe dev(dir, dir);
    LockProbe lock(dev);
    CHECK(!dev.readFreqTable(1, "available_frequencies", lock));
    CHECK(lock.calls() == 0);
    CHECK(dev.table().empty());
}

// EP-08：全是垃圾 —— false、不取锁
void testGarbageNode() {
    test::section("EP-08 全垃圾内容：false + 不取锁");
    const std::string dir = caseDir("ep08");
    put(dir + "/available_frequencies", "abc def\n");

    DeviceFileProbe dev(dir, dir);
    LockProbe lock(dev);
    CHECK(!dev.readFreqTable(1, "available_frequencies", lock));
    CHECK(lock.calls() == 0);
    CHECK(dev.table().empty());
}

// EP-09：半截垃圾 —— istringstream 遇错即停，后段被截断（现状行为，锁死）
void testPartialGarbageTruncates() {
    test::section("EP-09 半截垃圾：只取到错误前的档位");
    const std::string dir = caseDir("ep09");
    put(dir + "/available_frequencies", "1800000 abc 2000000\n");

    DeviceFileProbe dev(dir, dir);
    LockProbe lock(dev);
    CHECK(dev.readFreqTable(1, "available_frequencies", lock));
    CHECK_TABLE(dev.table(), Ints({1800000}));
    CHECK(lock.calls() == 1);
}

// EP-10：divisor <= 0 —— 防御分支，false、不取锁
void testInvalidDivisor() {
    test::section("EP-10 divisor <= 0：false + 不取锁");
    const std::string dir = caseDir("ep10");
    put(dir + "/available_frequencies", "1000000\n");

    DeviceFileProbe dev(dir, dir);
    LockProbe zeroLock(dev);
    CHECK(!dev.readFreqTable(0, "available_frequencies", zeroLock));
    CHECK(zeroLock.calls() == 0);
    CHECK(dev.table().empty());

    LockProbe negativeLock(dev);
    CHECK(!dev.readFreqTable(-1000, "available_frequencies", negativeLock));
    CHECK(negativeLock.calls() == 0);
    CHECK(dev.table().empty());
}

// EP-11：CRLF 与逗号分隔 —— 内核节点常见形态
void testCrlfAndCommaSeparators() {
    test::section("EP-11 CRLF / 逗号分隔");
    const std::string dir = caseDir("ep11");
    put(dir + "/available_frequencies", "1000000,\r\n2000000\r\n3000000\n");

    DeviceFileProbe dev(dir, dir);
    LockProbe lock(dev);
    CHECK(dev.readFreqTable(1, "available_frequencies", lock));
    CHECK_TABLE(dev.table(), Ints({1000000, 2000000, 3000000}));
}

} // namespace devicefile
} // namespace test
} // namespace sched
