// SPDX-License-Identifier: MIT
// deviceFile 单元测试 · 测试用例（实现）
//
// 三个用例都按 systemTestCases.md 的步骤与输出格式落实：
//   S001 initTest           逐行读 initCase → 构造桩对象 → 比对四个属性 → 计不一致数；
//   S002 freqTableParseTest 直接调 parseFreqTable，按 divisor 分档验证输出性质；
//   S003 readFreqTableTest  经桩对象调 readFreqTable，与字面量期望表逐项比对。
//
// 数据文件定位不写死单一相对路径：按运行位置依次尝试四个候选根目录，
// 这样从 system_tests 直接 `make` 能跑，在套件目录或 build/ 下单独执行也能跑。
//
// **期望值的独立来源**：S003 的期望表由本文件内的字面量原始值独立归一化得出，
// 不调用被测的 parseFreqTable——否则「解析器读错、期望也读错」会互相抵消。
// 字面量按样例文件原样照抄（含乱序与重复），因此逗号 / 换行 / 制表 / 回车
// 这几种分隔符若解析失手，S003 会直接翻成 fatal。

#include "deviceFile_tests.h"
#include "deviceFile_stub.h"

#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <exception>
#include <fstream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace sched {
namespace test {
namespace devicefile {

namespace {

// ---------------------------------------------------------------------------
// 通用小工具
// ---------------------------------------------------------------------------

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

// 用例数据根目录（deviceFile_cases/ 的上一层拼接位置），末尾带 '/'
std::string casesRoot() {
    const char* roots[] = {
        "deviceFile_test/deviceFile_cases/",       // cwd = test/system_tests
        "deviceFile_cases/",                       // cwd = deviceFile_test
        "../deviceFile_test/deviceFile_cases/",    // cwd = build（run 从上层发起时）
        "../../deviceFile_test/deviceFile_cases/"  // cwd = build 且在其中执行
    };

    for (const char* r : roots) {
        std::ifstream probe(std::string(r) + "initCase");   // 非空文件，用作目录探针
        if (probe.good()) {
            return r;
        }
    }
    return roots[0];
}

// 期望表：原始值 → ÷divisor → 丢弃非正 → 去重升序。
// 与被测 parseFreqTable 的**契约一致但实现独立**（不经字符串解析）。
std::vector<int> normalize(const std::vector<int>& raw, int divisor) {
    std::vector<int> out;
    if (divisor <= 0) {
        return out;
    }
    for (const int v : raw) {
        const int kHz = v / divisor;
        if (kHz > 0) {
            out.push_back(kHz);
        }
    }
    std::sort(out.begin(), out.end());
    out.erase(std::unique(out.begin(), out.end()), out.end());
    return out;
}

// 样例文件的原始整数，按 README 的形态照抄（乱序、重复保留）
struct CaseSpec {
    const char* name;
    std::vector<int> raw;
};

const CaseSpec kXpuCases[] = {
    {"case1", {408000, 600000, 816000, 1008000, 1200000, 1416000, 1608000, 1800000}},
    {"case2", {408000, 600000, 600000, 816000, 1008000, 1008000,
               1200000, 1416000, 1608000, 1800000, 1800000}},
    {"case3", {300000000, 400000000, 500000000, 600000000,
               700000000, 800000000, 900000000, 1000000000}},
    {"case4", {300000000, 400000000, 400000000, 500000000, 600000000,
               600000000, 700000000, 800000000, 900000000, 1000000000}},
};

// S002 要遍历的样例：case0 空文件，case1~case5 仅分隔符不同，
// case6 降序非正数，case7~case9 分别是有序整数 / 无序整数 / 无序正数
const char* const kFreqCases[] = {
    "case0", "case1", "case2", "case3", "case4",
    "case5", "case6", "case7", "case8", "case9"
};

const char* const kXpuFiles[] = {"case0", "case1", "case2", "case3", "case4"};

// 按样例名取字面量原始值（仅 kXpuCases 内有用例；case0 空文件不在表内，回空）
const std::vector<int>& rawOf(const char* node) {
    for (const CaseSpec& spec : kXpuCases) {
        if (std::string(spec.name) == node) {
            return spec.raw;
        }
    }
    static const std::vector<int> kEmpty;
    return kEmpty;
}

} // namespace

// ---------------------------------------------------------------------------
// S001-初始化测试
// ---------------------------------------------------------------------------
int initTest() {
    const std::string path = casesRoot() + "initCase";
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

    std::printf("ALL:%d passed %d fatal\n", total, fatal);
    return fatal;
}

// ---------------------------------------------------------------------------
// S002-频率表文件解析测试
// ---------------------------------------------------------------------------
int freqTableParseTest() {
    const std::string dir = casesRoot() + "freqTableCases/";

    int missing = 0;
    for (const char* n : kFreqCases) {
        if (!FileService::exists(dir + n)) {
            std::printf("S002: 缺少样例文件 %s%s\n", dir.c_str(), n);
            ++missing;
        }
    }
    if (missing != 0) {
        return missing;
    }

    int total = 0;
    int fatal = 0;

    // divisor / 样例 / 期望是否为空表
    const auto expect = [&](int divisor, const char* node, bool wantEmpty) {
        ++total;
        const std::vector<int> table =
            parseFreqTable(FileService::read(dir + node), divisor);

        // 空表期望只看「是否为空」；非空期望看「升序 + 无重复 + 全正」
        const bool ok = wantEmpty ? table.empty() : sortedUniquePositive(table);
        if (!ok) {
            ++fatal;
        }

        std::printf("exp %2d : divisor=%-5d %-6s %s %s => %s\n",
                    total, divisor, node,
                    wantEmpty ? "期望空表    " : "期望升序正数",
                    dump(table).c_str(), ok ? "passed" : "fatal");
    };

    // 步骤 1：非法 divisor 直接退化为空表（除零保护）
    expect(0, "case1", true);
    expect(-1, "case1", true);

    // 步骤 2~5：divisor=1 跑一遍；步骤 6：divisor=1000 原样再跑一遍
    for (const int divisor : {1, 1000}) {
        expect(divisor, "case0", true);                     // 步骤 2：空文件

        for (const char* n : {"case1", "case2", "case3", "case4", "case5"}) {
            expect(divisor, n, false);                      // 步骤 3：五种分隔符
        }

        expect(divisor, "case6", true);                     // 步骤 4：降序非正数

        for (const char* n : {"case7", "case8", "case9"}) {
            expect(divisor, n, false);                      // 步骤 5：有序 / 无序整数
        }
    }

    std::printf("ALL:%d passed %d fatal\n", total, fatal);
    return fatal;
}

// ---------------------------------------------------------------------------
// S003-频率表测试
// ---------------------------------------------------------------------------
int readFreqTableTest() {
    const std::string dir = casesRoot() + "xpuCases/";

    int missing = 0;
    for (const char* n : kXpuFiles) {
        if (!FileService::exists(dir + n)) {
            std::printf("S003: 缺少样例文件 %s%s\n", dir.c_str(), n);
            ++missing;
        }
    }
    if (missing != 0) {
        return missing;
    }

    // 一次 readFreqTable 调用 = 一条检查
    struct Step {
        int divisor;
        const char* node;
        bool wantEmpty;
    };
    // 顺序照抄《说明》：case0 → case1 → case2 →（divisor=1000）case0 → case3 → case4
    const Step kSteps[] = {
        {1, "case0", true}, {1, "case1", false}, {1, "case2", false},
        {1000, "case0", true}, {1000, "case3", false}, {1000, "case4", false},
    };

    // sampleB 的量程取「待测步骤实际覆盖到的值的并集区间」：既含 CPU 的 kHz 值
    // （divisor=1），也含 NPU 折成 kHz 之后的值（divisor=1000），从而收窄不丢项——
    // 「装载结果 == 文件对应序列」与「元素都在量程内」两条断言才能同时成立。
    int loAll = 0;
    int hiAll = 0;
    bool first = true;
    for (const Step& st : kSteps) {
        for (const int v : normalize(rawOf(st.node), st.divisor)) {
            loAll = first ? v : std::min(loAll, v);
            hiAll = first ? v : std::max(hiAll, v);
            first = false;
        }
    }

    struct Sample {
        const char* name;
        int freqMin;
        int freqMax;
    };
    const Sample kSamples[] = {
        {"A", 0, 0},            // 0 表示该侧不限
        {"B", loAll, hiAll},    // 声明量程，收窄后不得越界
    };

    int total = 0;
    int fatal = 0;

    for (const Sample& s : kSamples) {
        std::printf("sample %s (freqMin=%d, freqMax=%d)\n",
                    s.name, s.freqMin, s.freqMax);

        // 每个 sample 一个实例，跨步骤保留 freqTable_，以此验证装载是覆盖而非累加；
        // 「期望空表」的步骤另起新实例——readFreqTable 在解析失败时**不清空**旧表，
        // 沿用同一实例会读到上一步遗留的内容，与用例本意（空文件 → 空表）不符。
        std::unique_ptr<DeviceFileProbe> probe(
            new DeviceFileProbe(dir, "", s.freqMin, s.freqMax));

        // 步骤 1 / 步骤 8：构造后 freqTable_ 必须为空
        ++total;
        const bool initEmpty = probe->table().empty();
        if (!initEmpty) {
            ++fatal;
        }
        std::printf("  初始 freqTable_ = %s %s\n",
                    dump(probe->table()).c_str(), initEmpty ? "passed" : "fatal");

        for (const Step& st : kSteps) {
            if (st.wantEmpty) {
                probe.reset(new DeviceFileProbe(dir, "", s.freqMin, s.freqMax));
            }

            ++total;
            LockProbe<DeviceFileProbe> lock(*probe);
            const bool ret = probe->readFreqTable(st.divisor, st.node, lock);
            const std::vector<int> got = probe->table();
            const std::vector<int> want =
                st.wantEmpty ? std::vector<int>() : normalize(rawOf(st.node), st.divisor);

            bool ok = (got == want);
            ok = ok && (ret == !st.wantEmpty);      // 空表 → false，装载成功 → true
            // 装载成功必须经锁回调写入一次；解析失败时直接返回，不进锁回调
            ok = ok && (!st.wantEmpty || lock.calls() == 0);
            ok = ok && (st.wantEmpty || lock.calls() == 1);

            // 量程后置条件：每个元素都在 [freqMin_, freqMax_]（0 表示不限）
            bool inRange = true;
            for (const int v : got) {
                if ((s.freqMin > 0 && v < s.freqMin) || (s.freqMax > 0 && v > s.freqMax)) {
                    inRange = false;
                    break;
                }
            }
            ok = ok && inRange;

            if (!ok) {
                ++fatal;
            }

            std::printf("  divisor=%-5d %-6s %s%s => %s\n",
                        st.divisor, st.node,
                        st.wantEmpty ? "[新实例] " : "",
                        dump(got).c_str(), ok ? "passed" : "fatal");

            if (!ok) {
                std::printf("          期望 = %s / 返回值 = %s / 锁回调 = %d / 量程内 = %s\n",
                            dump(want).c_str(), ret ? "true" : "false", lock.calls(),
                            inRange ? "true" : "false");
            }
        }
        std::printf("\n");
    }

    std::printf("ALL:%d passed %d fatal\n", total, fatal);
    return fatal;
}

} // namespace devicefile
} // namespace test
} // namespace sched
