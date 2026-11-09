// SPDX-License-Identifier: MIT
// deviceFile 单元测试 · 测试用例（实现）
//
// 两个用例都按《测试用例说明.md》的步骤与输出格式落实：
//   S001 逐行读 initCase → 构造桩对象 → 比对四个属性 → 计不一致数 → 末尾汇总；
//   S002 同一份 freqTableCase 两路读取（readFreqTable / 直接解析）→ 逐项比对。
//
// 数据文件定位不写死单一相对路径：按 system_tests 下的运行位置依次尝试
// 三个候选目录，找到即用。这样从 system_tests 直接 `make` 能跑，
// 在套件目录里单独执行产物也能跑。

#include "deviceFile_tests.h"
#include "deviceFile_stub.h"

#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

namespace sched {
namespace test {
namespace devicefile {

namespace {

// 按分隔符切分，保留空字段（尾随逗号、连续逗号都能如实反映出来）
std::vector<std::string> split(const std::string& s, char delim) {
    std::vector<std::string> out;
    std::string token;
    for (const char c : s) {
        if (c == delim) {
            out.push_back(token);
            token.clear();
        } else if (c != '\r') {   // 兼容 CRLF 写出的用例文件
            token.push_back(c);
        }
    }
    out.push_back(token);
    return out;
}

// 整串必须是整数；空字段按 0 处理（deviceFile 语义：0 表示该侧不限）
bool toInt(const std::string& s, int& out) {
    std::size_t b = 0;
    std::size_t e = s.size();
    while (b < e && (s[b] == ' ' || s[b] == '\t')) {
        ++b;
    }
    while (e > b && (s[e - 1] == ' ' || s[e - 1] == '\t')) {
        --e;
    }

    if (b == e) {
        out = 0;
        return true;
    }

    try {
        std::size_t pos = 0;
        const std::string t = s.substr(b, e - b);
        const int v = std::stoi(t, &pos);
        if (pos != t.size()) {
            return false;
        }
        out = v;
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

// 用例数据文件定位：返回第一个真实存在的候选路径
std::string locate(const std::string& name) {
    const char* dirs[] = {
        "deviceFile_test/deviceFile_cases/",     // cwd = test/system_tests
        "deviceFile_cases/",                     // cwd = deviceFile_test
        "../deviceFile_test/deviceFile_cases/"   // cwd = build
    };

    for (const char* d : dirs) {
        const std::string p = std::string(d) + name;
        std::ifstream f(p);
        if (f.good()) {
            return p;
        }
    }
    return std::string(dirs[0]) + name;
}

} // namespace

// ---------------------------------------------------------------------------
// S001-初始化测试
// ---------------------------------------------------------------------------
int initTest() {
    const std::string path = locate("initCase");
    std::ifstream fin(path);
    if (!fin) {
        std::printf("S001: 无法打开数据文件 %s\n", path.c_str());
        return 1;
    }

    int total = 0;
    int fatal = 0;
    std::string line;

    while (std::getline(fin, line)) {
        if (line.empty()) {
            continue;
        }

        const std::vector<std::string> f = split(line, ',');
        ++total;

        if (f.size() < 4) {
            ++fatal;
            std::printf("case %d:字段不足，需 4 项，实得 %d 项\n",
                        total, static_cast<int>(f.size()));
            continue;
        }

        int lo = 0;
        int hi = 0;
        if (!toInt(f[2], lo) || !toInt(f[3], hi)) {
            ++fatal;
            std::printf("case %d:频率上下限不是整数（%s / %s）\n",
                        total, f[2].c_str(), f[3].c_str());
            continue;
        }

        // 被测对象在用例内就地构造：四元组即构造实参，属性应与实参逐项一致
        const DeviceFileProbe dfp(f[0], f[1], lo, hi);

        const bool okPath = (f[0] == dfp.freqPath());
        const bool okStat = (f[1] == dfp.statPath());
        const bool okMin = (lo == dfp.freqMin());
        const bool okMax = (hi == dfp.freqMax());

        std::printf("case %d:%s %s %s %s\n", total,
                    okPath ? "true" : "false",
                    okStat ? "true" : "false",
                    okMin ? "true" : "false",
                    okMax ? "true" : "false");

        if (!(okPath && okStat && okMin && okMax)) {
            ++fatal;
            std::printf("        输入 = %s,%s,%d,%d\n", f[0].c_str(), f[1].c_str(), lo, hi);
            std::printf("        实际 = %s,%s,%d,%d\n",
                        dfp.freqPath().c_str(), dfp.statPath().c_str(),
                        dfp.freqMin(), dfp.freqMax());
        }
    }

    std::printf("ALL:%d passed %d fatal %d\n", total, total - fatal, fatal);
    return fatal;
}

// ---------------------------------------------------------------------------
// S002-频率表测试
// ---------------------------------------------------------------------------
int freqTableTest() {
    const std::string node = "freqTableCase";
    const std::string path = locate(node);

    std::ifstream probe(path);
    if (!probe) {
        std::printf("S002: 无法打开数据文件 %s\n", path.c_str());
        return 1;
    }

    // readFreqTable 内部按 freqPath_ + "/" + freqNode 拼路径，所以这里传目录
    std::string dir = ".";
    if (path.size() > node.size() + 1) {
        dir = path.substr(0, path.size() - node.size() - 1);
    }

    // 路径 1：经被测接口装载（桩对象 + 锁探针，同时验证写入落在锁回调内）
    DeviceFileProbe dfp(dir, "", 0, 0);
    LockProbe<DeviceFileProbe> lock(dfp);
    const bool loaded = dfp.readFreqTable(1, node, lock);
    const Ints viaInterface = dfp.table();

    // 路径 2：不经封装，直接读同一份样例表再解析
    const Ints direct = parseFreqTable(FileService::read(path), 1);

    std::printf("case:");
    for (const int v : direct) {
        std::printf(" %d", v);
    }
    std::printf("\n");

    std::printf("readFreqTable:");
    for (const int v : viaInterface) {
        std::printf(" %d", v);
    }
    std::printf("\n");

    const std::size_t n = std::max(direct.size(), viaInterface.size());
    int diff = 0;
    std::printf("compare:");
    for (std::size_t i = 0; i < n; ++i) {
        const bool eq = (i < direct.size() && i < viaInterface.size()
                         && direct[i] == viaInterface[i]);
        if (!eq) {
            ++diff;
        }
        std::printf(" %s", eq ? "T" : "F");
    }
    std::printf("\n");

    // 正常路径不额外输出，保持与《说明》的预期输出逐行一致；仅异常时补诊断
    if (!loaded) {
        std::printf("note: readFreqTable 返回 false（节点读取失败，或收窄后为空退化为整表）\n");
        std::printf("note: 锁回调调用 %d 次，锁内表长 %d\n", lock.calls(),
                    lock.tableSizeInsideLock());
    }

    return diff;
}

} // namespace devicefile
} // namespace test
} // namespace sched
