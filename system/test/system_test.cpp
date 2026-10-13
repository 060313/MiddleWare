// system 子系统冒烟测试
#include <cstdio>

#include "SystemInterface.h"

int main() {
    syscore::SystemInterface sys;

    common::SystemFeature feature;
    const bool collected = sys.getInfo(feature);
    std::printf("[system_test] getInfo=%d ucpu=%.4f stale=%d\n",
                static_cast<int>(collected), feature.ucpu,
                static_cast<int>(feature.stale));

    // TODO: 用 VirtualNpu 注入后断言 collect 结果；断言频率夹取行为
    std::printf("[system_test] TODO: assertions pending\n");
    return 0;
}
