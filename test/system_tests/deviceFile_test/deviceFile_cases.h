// SPDX-License-Identifier: MIT
// deviceFile 单元测试 · 测试用例（声明）
//
// 范围：**只测 deviceFile 基类本身**。CPUFile / NPUFile 包装层的节点名与单位折算、
//       CPU / NPU 设备层的调频副作用，均不在本套件内（由各自套件覆盖）。
//
// 覆盖三个维度：
//   接口   —— 构造契约、抽象基类/虚析构、只读 freqPath 不碰 statPath；
//   数据结构 —— freqTable_ 的不变量（升序、去重、全为正）、重复装载覆盖、
//              失败保留旧表、收窄边界、0/负值收窄语义；
//   执行路径 —— readFreqTable 的每一条分支：正常装载、部分收窄、三种退化、
//              节点缺失、空文件、全垃圾、半截垃圾（截断）、非法 divisor、CRLF/逗号分隔。
//
// 实现见 deviceFile_cases.cpp；用例编号与《测试用例说明.md》一一对应。
// IF-00 是编译期契约（two static_assert），放在驱动 deviceFile_test.cpp 中。

#ifndef SCHED_TEST_DEVICEFILE_CASES_H
#define SCHED_TEST_DEVICEFILE_CASES_H

namespace sched {
namespace test {
namespace devicefile {

// 接口（IF）
void testCtorStoresInjectedValues();
void testCtorDoesNotReadNodes();
void testBaseReadsFreqPathNotStatPath();
void testPathsStoredVerbatim();

// 数据结构（DS）
void testTableSortedUniquePositive();
void testNonPositiveDropped();
void testReloadOverwrites();
void testFailurePreservesTable();
void testNarrowKeepsBoundaries();
void testOneSidedNarrow();
void testNegativeNarrowTreatedAsUnbounded();

// 执行路径（EP）
void testFullLoad();
void testPartialNarrow();
void testAllBelowMinDegenerates();
void testAllAboveMaxDegenerates();
void testContradictoryBoundsDegenerate();
void testMissingNode();
void testEmptyNode();
void testGarbageNode();
void testPartialGarbageTruncates();
void testInvalidDivisor();
void testCrlfAndCommaSeparators();

} // namespace devicefile
} // namespace test
} // namespace sched

#endif // SCHED_TEST_DEVICEFILE_CASES_H
