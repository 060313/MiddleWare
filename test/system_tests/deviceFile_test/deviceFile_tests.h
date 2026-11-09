// SPDX-License-Identifier: MIT
// deviceFile 单元测试 · 测试用例（声明）
//
// 范围：**只测 deviceFile 基类本身**。CPUFile / NPUFile 包装层的节点名与单位折算、
//       CPU / NPU 设备层的调频副作用，均不在本套件内（由各自套件覆盖）。
//
// 本文件只声明入口，用例体全部在 deviceFile_tests.cpp 中实现；
// 被测对象由各用例就地构造，驱动 deviceFile_test.cpp 只负责编排与汇总退出码。
//
// 用例编号与《测试用例说明.md》一一对应：
//   S001 初始化测试 —— 逐行读 initCase，构造桩对象，比对四个属性；
//   S002 频率表测试 —— readFreqTable 装载结果 vs 直接解析样例表的逐项对比。
//
// 两个入口均返回「不一致条数」，0 表示该用例全通过；驱动据此汇总是非。

#ifndef SCHED_TEST_DEVICEFILE_CASES_H
#define SCHED_TEST_DEVICEFILE_CASES_H

namespace sched {
namespace test {
namespace devicefile {

    // S001-初始化测试
    // 逐条读 caseDir/initCase（每行 `freqPath,statPath,freqMin,freqMax`），
    // 构造 DeviceFileProbe 后比对四个已注入属性；返回不一致的条数。
    // 数据文件打不开时返回 1（不允许静默通过）。
    int initTest();

    // S002-频率表测试
    // 同一份 freqTableCase，一边经 readFreqTable 装载、一边直接解析，
    // 逐项比对并打印 T/F；返回不一致的项数。
    // 数据文件打不开时返回 1。
    int freqTableTest();

} // namespace devicefile
} // namespace test
} // namespace sched

#endif // SCHED_TEST_DEVICEFILE_CASES_H
