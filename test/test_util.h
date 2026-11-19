// SPDX-License-Identifier: MIT
// test_util —— 极简断言与用例框架（不引第三方依赖）
//
// 为什么自己写：顶层 Makefile 的 test 目标只做「编译 test/*_test.cpp → 运行」，
// 不引入 gtest，故此处提供 INC 已含的 -I$(ROOT)/test 下的轻量框架。
//
// 用法：
//   TEST_CASE(name) { ... CHECK(...); }
//   int main() { RUN(name); return test::summary(); }

#ifndef SCHED_TEST_TEST_UTIL_H
#define SCHED_TEST_TEST_UTIL_H

#include <cmath>
#include <cstdio>
#include <string>
#include <type_traits>

namespace test {

inline int& checkCount() {
    static int n = 0;
    return n;
}

inline int& failCount() {
    static int n = 0;
    return n;
}

template <typename T>
std::string show(const T& value) {
    if constexpr (std::is_same<T, std::string>::value) {
        return "\"" + value + "\"";
    } else if constexpr (std::is_same<T, bool>::value) {
        return value ? "true" : "false";
    } else if constexpr (std::is_enum<T>::value) {
        return std::to_string(static_cast<long long>(value));
    } else if constexpr (std::is_arithmetic<T>::value) {
        return std::to_string(value);
    } else {
        return "<value>";
    }
}

inline void report(bool ok, const char* expr, const std::string& detail,
                   const char* file, int line) {
    ++checkCount();
    if (ok) {
        return;
    }
    ++failCount();
    if (detail.empty()) {
        std::printf("[FAIL] %s:%d  %s\n", file, line, expr);
    } else {
        std::printf("[FAIL] %s:%d  %s  (%s)\n", file, line, expr, detail.c_str());
    }
}

inline void check(bool cond, const char* expr, const char* file, int line) {
    report(cond, expr, std::string(), file, line);
}

template <typename A, typename B>
void checkEq(const A& got, const B& want, const char* expr, const char* file, int line) {
    const bool ok = (got == want);
    report(ok, expr, ok ? std::string() : ("got " + show(got) + ", want " + show(want)),
           file, line);
}

inline void checkNear(double got, double want, double eps, const char* expr,
                      const char* file, int line) {
    const bool ok = std::fabs(got - want) <= eps;
    report(ok, expr,
           ok ? std::string() : ("got " + std::to_string(got) + ", want " + std::to_string(want)),
           file, line);
}

inline void runCase(void (*fn)(), const char* name) {
    std::printf("--- %s\n", name);
    fn();
}

inline int summary() {
    std::printf("\n%d checks, %d failed\n", checkCount(), failCount());
    return failCount() == 0 ? 0 : 1;
}

} // namespace test

#define TEST_CASE(fn) static void fn()
#define RUN(fn) ::test::runCase(fn, #fn)

#define CHECK(cond) ::test::check((cond), #cond, __FILE__, __LINE__)
#define CHECK_EQ(got, want) ::test::checkEq((got), (want), #got " == " #want, __FILE__, __LINE__)
#define CHECK_NEAR(got, want, eps) \
    ::test::checkNear((got), (want), (eps), #got " ~= " #want, __FILE__, __LINE__)

#endif // SCHED_TEST_TEST_UTIL_H
