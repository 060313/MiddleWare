#pragma once

// 格式化：表格 / JSON 输出。
// 见《用户界面设计文档》9.1。

#include <string>

#include "Types.h"

namespace cli {

class OutputFormatter {
public:
    OutputFormatter() = default;
    ~OutputFormatter() = default;

    void setJson(bool on) { m_json = on; }
    bool json() const { return m_json; }

    std::string formatState(const common::SystemFeature& feature) const;
    std::string formatMode(common::SchedMode mode) const;
    std::string formatActionResult(const common::ActionInfo& info) const;

private:
    bool m_json = false;
};

}  // namespace cli
