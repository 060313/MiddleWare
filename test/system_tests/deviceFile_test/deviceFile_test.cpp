// SPDX-License-Identifier: MIT
// deviceFile 单元测试 · 测试驱动
//
// 职责只有三件：编译期契约、按维度编排用例、汇总与清理。用例实现在
// deviceFile_cases.cpp，桩与夹具在 deviceFile_stub.h，本文件不定义任何用例、
// 也不持有被测对象实例。
//
// 运行（仓库根目录，伪节点树落在 build/test_fake_devicefile/）：
//   make test
// 或单独编（Windows / Linux 同一命令，仅可执行文件名后缀不同）：
//   g++ -std=c++17 -Wall -Wextra -O2 -g -I. -Itest test/system_tests/deviceFile_test/deviceFile_test.cpp
//       test/system_tests/deviceFile_test/deviceFile_cases.cpp common/src/FileService.cpp -o build/deviceFile_test
//   ./build/deviceFile_test
// （上面第二行为续写，实际输入时拼成一行；此处不写行尾反斜杠，避免注释被行拼接。）
//
// 注：deviceFile 构造已内联在头文件，无需链接任何 system 侧 .cpp；本套件只依赖
// FileService。用例编号与《测试用例说明.md》一一对应。

#include <cstdio>
#include <filesystem>
#include <type_traits>

#include "deviceFile_cases.h"
#include "deviceFile_stub.h"
#include "system/include/deviceFile.h"

namespace df = sched::test::devicefile;

// IF-00：编译期契约 —— 抽象基类 + 虚析构
static_assert(std::is_abstract_v<sched::deviceFile>,
              "deviceFile 必须是抽象基类（refreshInfo / setFreq 为纯虚）");
static_assert(std::has_virtual_destructor_v<sched::deviceFile>,
              "deviceFile 必须可经基类指针安全析构");

int main() {
    std::filesystem::remove_all(df::kRoot);

    // 接口（IF）
    df::testCtorStoresInjectedValues();
    df::testCtorDoesNotReadNodes();
    df::testBaseReadsFreqPathNotStatPath();
    df::testPathsStoredVerbatim();

    // 数据结构（DS）
    df::testTableSortedUniquePositive();
    df::testNonPositiveDropped();
    df::testReloadOverwrites();
    df::testFailurePreservesTable();
    df::testNarrowKeepsBoundaries();
    df::testOneSidedNarrow();
    df::testNegativeNarrowTreatedAsUnbounded();

    // 执行路径（EP）
    df::testFullLoad();
    df::testPartialNarrow();
    df::testAllBelowMinDegenerates();
    df::testAllAboveMaxDegenerates();
    df::testContradictoryBoundsDegenerate();
    df::testMissingNode();
    df::testEmptyNode();
    df::testGarbageNode();
    df::testPartialGarbageTruncates();
    df::testInvalidDivisor();
    df::testCrlfAndCommaSeparators();

    const int rc = sched::test::report("deviceFile");
    std::filesystem::remove_all(df::kRoot);
    return rc;
}
