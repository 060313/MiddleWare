// SPDX-License-Identifier: MIT
// 单测公用断言（不引 gtest，保持零第三方依赖）
//
// 每个 *_test.cpp 自带 main，末尾调 report 汇总；失败项打印 文件:行:表达式，
// 进程退出码即成败，供 make test 直接判定。

#ifndef SCHED_TEST_TEST_UTIL_H
#define SCHED_TEST_TEST_UTIL_H

#include <cstdio>

namespace sched {
namespace test {

inline int& passedCount() {
    static int n = 0;
    return n;
}

inline int& failedCount() {
    static int n = 0;
    return n;
}

inline void check(bool ok, const char* expr, const char* file, int line) {
    if (ok) {
        ++passedCount();
    } else {
        ++failedCount();
        std::printf("  [FAIL] %s:%d: %s\n", file, line, expr);
    }
}

inline void section(const char* name) {
    std::printf("%s\n", name);
}

inline int report(const char* suite) {
    std::printf("\n[%s] passed %d, failed %d\n", suite, passedCount(), failedCount());
    return failedCount() == 0 ? 0 : 1;
}

} // namespace test
} // namespace sched

#define CHECK(expr) ::sched::test::check((expr), #expr, __FILE__, __LINE__)

#endif // SCHED_TEST_TEST_UTIL_H
