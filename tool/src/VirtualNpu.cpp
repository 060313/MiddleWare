#include "VirtualNpu.h"

#include "SystemFS.h"

namespace sched {

VirtualNpu::VirtualNpu() {
    // TODO: m_base = SystemFS::tempDir() + "/virtual_npu"；不存在则创建并写入
    //       默认文件：governor=powersave、cur_freq=1000000、
    //       available_frequencies="1000000 800000 600000"、task_count=0
}

VirtualNpu::~VirtualNpu() = default;

bool VirtualNpu::GetLoad(NpuState& out) {
    // TODO: 从 m_base/load 读取三核负载（无文件时随机/正弦模拟）
    (void)out;
    return false;
}

bool VirtualNpu::SubmitTask(const std::string& task) {
    // TODO: task_count 自增并写回 m_base/task_count
    (void)task;
    return false;
}

bool VirtualNpu::SetGovernor(const std::string& governor) {
    // TODO: 写 m_base/governor
    (void)governor;
    return false;
}

bool VirtualNpu::SetFrequency(int freqKHz) {
    // TODO: 写 m_base/cur_freq
    (void)freqKHz;
    return false;
}

std::string VirtualNpu::GetBasePath() const {
    return m_base;
}

}  // namespace sched
