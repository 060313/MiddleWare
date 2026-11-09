// SPDX-License-Identifier: MIT
// deviceFile 单元测试 · 桩模块（header-only）
//
// 本文件只放**测试替身与夹具**，不含任何用例、不持任何被测对象实例：
//   - DeviceFileProbe：被测对象 deviceFile 的派生探针，把 protected 的
//     readFreqTable 与四个成员提升为可观测接口（只读暴露，不改变基类行为）；
//   - LockProbe：锁回调替身，记录 withLock 调用次数，并在回调内部取样，
//     用来证明 freqTable_ 的写入确实发生在锁回调作用域内；
//   - 伪节点树夹具：caseDir / put / dropNode，在 build/test_fake_devicefile/ 下
//     按用例号建目录，冒充 cpufreq 挂点（FileService 只拼类里持有的路径，
//     故换路径即可换掉整个设备，无需 mock 框架）；
//   - 断言辅助：dump / checkTable / CHECK_TABLE / sortedUniquePositive。
//
// **不改生产代码、不加测试钩子**；被测对象由用例就地构造，驱动只负责编排。
// 桩模块无 .cpp：探针是类、锁替身是模板、夹具是短 inline 函数，全部随
// 包含它的编译单元展开。

#ifndef SCHED_TEST_DEVICEFILE_STUB_H
#define SCHED_TEST_DEVICEFILE_STUB_H

#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

#include "common/include/FileService.h"
#include "system/include/deviceFile.h"
#include "test/test_util.h"

namespace sched {
namespace test {
namespace devicefile {

namespace fs = std::filesystem;
using Ints = std::vector<int>;
inline const std::string kRoot = "build/test_fake_devicefile";

class DeviceFileProbe : public sched::deviceFile {
public:
    DeviceFileProbe(const std::string& freqPath, const std::string& statPath = "",
                    int freqMin = 0, int freqMax = 0)
        : deviceFile(freqPath, statPath, freqMin, freqMax) {}

    using deviceFile::readFreqTable;   // protected → public，仅测试替身使用

    const Ints& table() const { return freqTable_; }
    const std::string& freqPath() const { return freqPath_; }
    const std::string& statPath() const { return statPath_; }
    int freqMin() const { return freqMin_; }
    int freqMax() const { return freqMax_; }

protected:
    ResultInfo refreshInfo() override { return ResultInfo{}; }
    ResultInfo setFreq(const Freq&) override { return ResultInfo{}; }
};

// 锁回调探针：记录调用次数，并在回调内（即“锁内”）观察表长，
// 用于证明 freqTable_ 的写入确实发生在 withLock 内。
template <typename Device>
class LockProbe {
public:
    explicit LockProbe(const Device& device) : device_(device) {}

    template <typename Fn>
    void operator()(Fn&& fn) {
        ++calls_;
        fn();
        tableSizeInsideLock_ = static_cast<int>(device_.table().size());
    }

    int calls() const { return calls_; }
    int tableSizeInsideLock() const { return tableSizeInsideLock_; }

private:
    const Device& device_;
    int calls_ = 0;
    int tableSizeInsideLock_ = -1;
};

// ---------------------------------------------------------------------------
// 夹具与断言辅助
// ---------------------------------------------------------------------------

// 每个用例独立目录，先清后建，避免上一轮残留污染
inline std::string caseDir(const std::string& name) {
    const std::string dir = kRoot + "/" + name;
    fs::remove_all(dir);
    fs::create_directories(dir);
    return dir;
}

// 写一个伪设备节点
inline void put(const std::string& path, const std::string& text) {
    FileService::write(path, text);
}

// 让节点“消失”，模拟读取失败（内核节点被卸载 / 权限变化）
inline void dropNode(const std::string& path) {
    fs::remove(path);
}

inline std::string dump(const Ints& values) {
    std::string out = "[";
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (i != 0) {
            out += ", ";
        }
        out += std::to_string(values[i]);
    }
    out += "]";
    return out;
}

inline void checkTable(const Ints& actual, const Ints& expected,
                       const char* expr, const char* file, int line) {
    if (actual == expected) {
        test::check(true, expr, file, line);
        return;
    }
    test::check(false, expr, file, line);
    std::printf("         实际 = %s\n         期望 = %s\n",
                dump(actual).c_str(), dump(expected).c_str());
}

// 表不变量：升序、无重复、全为正
inline bool sortedUniquePositive(const Ints& table) {
    for (std::size_t i = 0; i < table.size(); ++i) {
        if (table[i] <= 0) {
            return false;
        }
        if (i > 0 && table[i] <= table[i - 1]) {
            return false;
        }
    }
    return true;
}

} // namespace devicefile
} // namespace test
} // namespace sched

// 失败时把实际值与期望值一并打印，便于定位；展开为完全限定名，避免依赖包含上下文
#define CHECK_TABLE(actual, expected)                                        \
    ::sched::test::devicefile::checkTable((actual), (expected), #actual,     \
                                          __FILE__, __LINE__)

#endif // SCHED_TEST_DEVICEFILE_STUB_H
