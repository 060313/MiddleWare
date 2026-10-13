#pragma once

// schedd.conf（INI 风格）解析，见《软件详细设计文档》7.2。

#include <map>
#include <string>

namespace common {

class ConfigLoader {
public:
    ConfigLoader() = default;
    ~ConfigLoader() = default;

    // 读取并解析配置文件；文件不存在或不可读返回 false（调用方使用默认值）
    bool load(const std::string& path);

    std::string getString(const std::string& section,
                          const std::string& key,
                          const std::string& defaultValue) const;

    int getInt(const std::string& section,
               const std::string& key,
               int defaultValue) const;

    bool has(const std::string& section, const std::string& key) const;

    void clear();

private:
    std::map<std::string, std::map<std::string, std::string>> m_items;
};

}  // namespace common
