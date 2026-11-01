// SPDX-License-Identifier: MIT
// test_util.h —— 极简单元测试框架（镜像内无 gtest，故自带断言与用例注册）
//
// 用法：
//   #include "test_util.h"
//
//   TEST_CASE(my_case) {
//       CHECK_TRUE(FileService::exists("/tmp"));
//       CHECK_EQ((int)v.size(), 3);
//   }
//
//   TEST_MAIN()
//
// 顶层 Makefile 把每个 test/*_test.cpp 单独编成一个可执行文件并直接运行，
// 故每个测试文件都要有自己的 main（TEST_MAIN 宏提供）。

#ifndef SCHED_TEST_UTIL_H
#define SCHED_TEST_UTIL_H

#include <cstdio>
#include <string>
#include <vector>

namespace sched_test {

struct Case {
    const char* name;
    void (*fn)();
};

inline std::vector<Case>& cases() {
    static std::vector<Case> list;
    return list;
}

inline int& checks() {
    static int n = 0;
    return n;
}

inline int& failures() {
    static int n = 0;
    return n;
}

struct Registrar {
    Registrar(const char* name, void (*fn)()) {
        cases().push_back(Case{name, fn});
    }
};

inline void checkTrue(bool ok, const char* expr, const char* file, int line) {
    ++checks();
    if (!ok) {
        ++failures();
        std::printf("    FAIL %s:%d  %s\n", file, line, expr);
    }
}

// 比较两个值：只在失败时打印表达式（不做类型转换，避免引入打印依赖）
template <typename A, typename B>
inline void checkEq(const A& actual, const B& expected, const char* expr, const char* file,
                    int line) {
    ++checks();
    if (!(actual == expected)) {
        ++failures();
        std::printf("    FAIL %s:%d  %s\n", file, line, expr);
    }
}

inline int runAll() {
    std::printf("== %d 个用例 ==\n", static_cast<int>(cases().size()));
    for (const Case& c : cases()) {
        const int before = failures();
        std::printf("[ RUN  ] %s\n", c.name);
        try {
            c.fn();
        } catch (...) {
            ++failures();
            std::printf("    FAIL 用例抛出未预期异常\n");
        }
        std::printf("[ %s ] %s\n", (failures() == before) ? " PASS " : " FAIL ", c.name);
    }
    std::printf("\n断言 %d 条，失败 %d 条\n", checks(), failures());
    return (failures() == 0) ? 0 : 1;
}

} // namespace sched_test

#define TEST_CASE(name)                                              \
    static void name();                                              \
    static ::sched_test::Registrar name##_registrar(#name, &name);   \
    static void name()

#define CHECK_TRUE(cond) ::sched_test::checkTrue((cond), #cond, __FILE__, __LINE__)
#define CHECK_FALSE(cond) ::sched_test::checkTrue(!(cond), "!(" #cond ")", __FILE__, __LINE__)
#define CHECK_EQ(actual, expected) \
    ::sched_test::checkEq((actual), (expected), #actual " == " #expected, __FILE__, __LINE__)

#define TEST_MAIN()                       \
    int main() {                          \
        return ::sched_test::runAll();    \
    }

#endif // SCHED_TEST_UTIL_H
