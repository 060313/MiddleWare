#include "INpu.h"

#include <cstdlib>

#include "RealNpu.h"
#include "VirtualNpu.h"

namespace sched {

std::unique_ptr<INpu> INpu::Create() {
    const char* mode = std::getenv("SCHED_NPU_MODE");
    if (mode != nullptr && std::string(mode) == "real") {
        return std::make_unique<RealNpu>();
    }
    // 默认走虚拟 NPU（x86 仿真）
    return std::make_unique<VirtualNpu>();
}

}  // namespace sched
