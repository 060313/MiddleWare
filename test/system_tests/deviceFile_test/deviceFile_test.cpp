#include <cstdio>
#include <type_traits>

#include "deviceFile_tests.h"
#include "system/include/deviceFile.h"

namespace df = sched::test::devicefile;

// IF-00：编译期契约 —— 抽象基类 + 虚析构
static_assert(std::is_abstract_v<sched::deviceFile>,
              "deviceFile 必须是抽象基类（refreshInfo / setFreq 为纯虚）");
static_assert(std::has_virtual_destructor_v<sched::deviceFile>,
              "deviceFile 必须可经基类指针安全析构");

// 测试驱动：只负责编排两个用例并汇总退出码，用例体与被测对象都在别处
int main() {
    std::printf("== S001 初始化测试\n");
    int fatal = df::initTest();

    std::printf("\n== S002 频率表测试\n");
    fatal += df::freqTableTest();

    std::printf("\n%s\n", fatal == 0 ? "== all tests passed" : "== some tests failed");
    return fatal == 0 ? 0 : 1;
}
