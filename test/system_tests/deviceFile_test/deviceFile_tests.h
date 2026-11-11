// SPDX-License-Identifier: MIT
// deviceFile 单元测试 · 测试用例（声明）
//
// 范围：**只测 deviceFile 基类本身**。CPUFile / NPUFile 包装层的节点名与单位折算、
//       CPU / NPU 设备层的调频副作用，均不在本套件内（由各自套件覆盖）。
//
// 本文件只声明入口，用例体全部在 deviceFile_tests.cpp 中实现；
// 被测对象由各用例就地构造，驱动 deviceFile_test.cpp 只负责编排与汇总退出码。
//
// 用例编号与 systemTestCases.md 一一对应：
//   S001 初始化测试       —— 逐行读 initCase，构造桩对象，比对四个属性；
//   S002 频率表文件解析测试 —— parseFreqTable 对 freqTableCases/case0~case9
//                            在 divisor=1 / 1000（及退化 divisor=0、-1）下的输出性质；
//   S003 频率表测试       —— readFreqTable 对 xpuCases/case0~case4 的装载结果，
//                            并对 sampleB 追加量程收窄的后置条件检查。
//
// 三个入口均返回「不一致条数」，0 表示该用例全通过；驱动据此汇总是非。

#ifndef SCHED_TEST_DEVICEFILE_CASES_H
#define SCHED_TEST_DEVICEFILE_CASES_H

namespace sched {
namespace test {
namespace devicefile {

    // S001-初始化测试
    // 逐条读 initCase（每行 `freqPath,statPath,freqMin,freqMax`），
    // 构造 DeviceFileProbe 后比对四个已注入属性；返回不一致的条数。
    // 数据文件打不开时返回 1（不允许静默通过）。
    int initTest();

    // S002-频率表文件解析测试
    // 直接调用 parseFreqTable：空文件与非法 divisor → 空表；
    // 合法样例 → 升序正数序列（内部须已排序去重）。返回失败项数。
    int freqTableParseTest();

    // S003-频率表测试
    // 经桩对象调用 readFreqTable：装载结果须等于「样例文件对应的无重复升序序列」；
    // sampleB 追加量程检查，保证收窄后每个元素仍落在 [freqMin_, freqMax_] 内。
    // 返回失败项数。
    int readFreqTableTest();

} // namespace devicefile
} // namespace test
} // namespace sched

#endif // SCHED_TEST_DEVICEFILE_CASES_H
