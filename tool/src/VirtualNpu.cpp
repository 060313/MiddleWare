#include "VirtualNpu.h"

#include <cstdlib>
#include <filesystem>
#include <sstream>
#include <string>
#include <system_error>
#include <utility>

#include "SystemFS.h"

namespace sched {

VirtualNpu::VirtualNpu() {
    m_base = SystemFS::tempDir() + "/virtual_npu";

    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::u8path(m_base), ec);

    // sysfs 风格：目录下并列 5 个文件，一个文件一个值。
    // 已存在的文件不覆盖，避免进程重启后丢掉 mock 的运行时状态。
    const std::pair<const char*, const char*> defaults[] = {
        {"governor",              "powersave"},
        {"cur_freq",              "1000000"},
        {"available_frequencies", "1000000 800000 600000"},
        {"task_count",            "0"},
        {"load",                  "0 0 0"},
    };
    for (const auto& d : defaults) {
        const std::string path = m_base + "/" + d.first;
        if (!SystemFS::exists(path)) {
            SystemFS::write(path, d.second);
        }
    }
}

VirtualNpu::~VirtualNpu() = default;

bool VirtualNpu::GetLoad(NpuState& out) {
    // TODO: 三核负载目前由 /load 文件提供，后续可在此叠加随机/正弦模拟
    out.valid = false;                              // 失败路径上不留脏值

    const auto text = SystemFS::read(m_base + "/load");
    if (!text) return false;                        // 主文件缺失 → 整体失败

    std::istringstream iss(*text);
    for (double& v : out.load) {
        if (!(iss >> v)) return false;              // 三核必须都能解析出来
    }

    if (const auto s = SystemFS::read(m_base + "/cur_freq"))
        out.freqKHz = std::atoi(s->c_str());        // 辅助项失败就退回默认，不 overall 失败
    if (const auto s = SystemFS::read(m_base + "/task_count"))
        out.taskCount = std::atoi(s->c_str());
    if (const auto s = SystemFS::read(m_base + "/governor")) {
        if      (*s == "performance") out.governor = NpuGovernor::Performance;
        else if (*s == "powersave")   out.governor = NpuGovernor::Powersave;
        else                          out.governor = NpuGovernor::Unknown;
    }

    out.valid = true;
    return true;
}

bool VirtualNpu::SubmitTask(const std::string& task) {
    // 参数 task 在此处无内容语义，但签名需与 RealNpu 保持一致（RKLLM runtime 预留）
    (void)task;

    const auto text = SystemFS::read(m_base + "/task_count");
    if (!text) return false;                        // 文件缺失/读失败：不能解引用空 optional

    std::istringstream iss(*text);
    int cnt = 0;
    if (!(iss >> cnt)) return false;
    ++cnt;
    return SystemFS::write(m_base + "/task_count", std::to_string(cnt));
}

bool VirtualNpu::SetGovernor(const std::string& governor) {
    return SystemFS::write(m_base + "/governor", governor);
}

bool VirtualNpu::SetFrequency(int freqKHz) {
    if (freqKHz <= 0) return false;                 // 0 与负值都不是合法频率
    return SystemFS::write(m_base + "/cur_freq", std::to_string(freqKHz));
}

std::string VirtualNpu::GetBasePath() const {
    return m_base;
}

}  // namespace sched
