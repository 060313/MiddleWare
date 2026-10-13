#include "ConfigLoader.h"

namespace common {

bool ConfigLoader::load(const std::string& path) {
    // TODO: 按行读取，跳过空行与 '#' 注释，解析 [section] 与 key=value（去首尾空白）
    (void)path;
    return false;
}

std::string ConfigLoader::getString(const std::string& section,
                                    const std::string& key,
                                    const std::string& defaultValue) const {
    // TODO: 查表，未命中返回 defaultValue
    (void)section;
    (void)key;
    return defaultValue;
}

int ConfigLoader::getInt(const std::string& section,
                         const std::string& key,
                         int defaultValue) const {
    // TODO: 查表并 std::stoi，失败或缺失返回 defaultValue
    (void)section;
    (void)key;
    return defaultValue;
}

bool ConfigLoader::has(const std::string& section, const std::string& key) const {
    // TODO: 查表判断存在性
    (void)section;
    (void)key;
    return false;
}

void ConfigLoader::clear() {
    m_items.clear();
}

}  // namespace common
