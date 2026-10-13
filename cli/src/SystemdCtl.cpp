#include "SystemdCtl.h"

#include <cstdio>
#include <string>
#include <utility>

#if defined(_WIN32)
#define SCHED_POPEN _popen
#define SCHED_PCLOSE _pclose
#else
#include <sys/wait.h>
#define SCHED_POPEN popen
#define SCHED_PCLOSE pclose
#endif

namespace cli {
namespace {

// systemctl 的退出码约定：0 成功；非 0 由 systemd 给出具体原因。
// POSIX 下 pclose 返回的是 waitpid 状态字，必须过 WEXITSTATUS 才是退出码
// —— 直接用返回值会把"退出码 1"错读成 256。

// 单元名会被拼进 shell 命令行，故限定为 systemd 单元名的合法字符集。
// 当前 m_unit 只来自常量 "schedd"，加固是为了防止将来被外部输入（--unit）穿透。
bool isSafeUnitName(const std::string& unit) {
    if (unit.empty()) {
        return false;
    }
    for (char c : unit) {
        const bool allowed = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                             (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-' ||
                             c == '@';
        if (!allowed) {
            return false;
        }
    }
    return true;
}

// out 为 nullptr 时丢弃子进程输出，否则追加到 out。
int runShell(const std::string& command, std::string* out) {
    FILE* pipe = SCHED_POPEN(command.c_str(), "r");
    if (pipe == nullptr) {
        return -1;
    }
    char buffer[512];
    while (std::fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        if (out != nullptr) {
            *out += buffer;
        }
    }
    const int status = SCHED_PCLOSE(pipe);
    if (status == -1) {
        return -1;
    }
#if defined(_WIN32)
    return status;   // MSVCRT 的 _pclose 直接返回退出码
#else
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;   // 被信号终止按失败计
#endif
}

#if defined(_WIN32)
// Windows 的空设备名是 NUL；写 /dev/null 会被 cmd.exe 当作普通文件名。
constexpr const char* kNullDevice = "NUL";
#else
constexpr const char* kNullDevice = "/dev/null";
#endif

}  // namespace

SystemdCtl::SystemdCtl(std::string unit) : m_unit(std::move(unit)) {}

int SystemdCtl::systemctl(const std::string& verb) {
    if (!isSafeUnitName(m_unit)) {
        return -1;
    }
    // 用 popen 而非 system()：需要子进程退出码，也需要在 status() 里拿到输出。
    // 这里把输出丢掉，只取退出码。
    const std::string command =
        "systemctl " + verb + " " + m_unit + " >" + kNullDevice + " 2>&1";
    return runShell(command, nullptr);
}

int SystemdCtl::start() {
    return systemctl("start");
}

int SystemdCtl::stop() {
    return systemctl("stop");
}

int SystemdCtl::restart() {
    return systemctl("restart");
}

std::string SystemdCtl::status() {
    if (!isSafeUnitName(m_unit)) {
        return std::string();
    }
    std::string output;
    const int code = runShell("systemctl status " + m_unit + " 2>&1", &output);
    if (output.empty()) {
        // x86 仿真环境下没有 systemd，"命令不存在"的提示落在 stderr；
        // 若仍为空则说明连 shell 都没起来，补一条可诊断的信息。
        output = "error: 无法执行 systemctl（退出码 " + std::to_string(code) + "）\n";
    }
    return output;
}

}  // namespace cli
