#pragma once

// 对 System 子系统的门面（设计模式：外观）。
// Agent 内访问 System 的统一入口，降低跨子系统耦合。
// 见《软件详细设计文档》5.3、8。

#include <memory>

#include "Types.h"

namespace syscore {
class SystemInterface;
}  // namespace syscore

namespace agent {

class SystemMonitor {
public:
    explicit SystemMonitor(std::shared_ptr<syscore::SystemInterface> sys);
    ~SystemMonitor();

    bool getFeature(common::SystemFeature& out);
    common::ResultInfo applyAction(const common::DeviceAction& action);

private:
    std::shared_ptr<syscore::SystemInterface> m_sys;
};

}  // namespace agent
