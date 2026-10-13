#include "OutputFormatter.h"

namespace cli {

std::string OutputFormatter::formatState(const common::SystemFeature& feature) const {
    // TODO: JSON 或对齐表格；stale 时高亮提示
    (void)feature;
    return std::string();
}

std::string OutputFormatter::formatMode(common::SchedMode mode) const {
    return common::toString(mode);
}

std::string OutputFormatter::formatActionResult(const common::ActionInfo& info) const {
    // TODO: 逐设备结果表格
    (void)info;
    return std::string();
}

}  // namespace cli
