// SPDX-License-Identifier: MIT
// ConfigFile —— 配置文件封装（详细设计 §4.5.2 / D-23 / D-26）
//
// 属 common 基础设施层，**不计入 §1 的 13 个设计类**。
// 职责：解析 INI 文本格式并提供逐键取值。
// 边界：
//   - 只读、只校验，不写配置（写入由运维在文件层面完成）；
//   - **不设内置默认表**——缺省值一律由调用方经 def 给出，避免出现第二个配置真源；
//   - 生命周期**止于装配期**（D-23）：取完键即销毁，不进运行期对象图。

#ifndef SCHED_DATA_CONFIGFILE_H
#define SCHED_DATA_CONFIGFILE_H

#include <map>
#include <string>

namespace sched {

class ConfigFile {
public:
    ConfigFile() = default;

    // 读入并解析配置文件；失败不影响已有内容
    bool getConfig(const std::string& configPath);

    // 校验配置：**只报「节 / 键缺失」**，不校验取值格式（取值一律容错，§4.3.4）
    bool configValidate(const std::string& configPath);

    // 取字符串值；键缺失或值为空 → 返回 def
    std::string getString(const std::string& section,
                          const std::string& key,
                          const std::string& def) const;

    // 取整数值；键缺失、值为空、**非法字面量** → 返回 def（不报错）
    int getInt(const std::string& section, const std::string& key, int def) const;

    // 判断键是否存在——键存在但值为空串时**仍为 true**，据此区分「未配置」与「配置为空值」
    bool has(const std::string& section, const std::string& key) const;

private:
    // 把文本解析进给定映射（供 getConfig 与 configValidate 共用，后者用临时映射避免污染）
    static bool parse(const std::string& text,
                      std::map<std::string, std::map<std::string, std::string>>& out);

    std::string path_;                                          // 最近一次读取的配置路径
    std::map<std::string, std::map<std::string, std::string>> values_;  // (节, 键) -> 值
};

} // namespace sched

#endif // SCHED_DATA_CONFIGFILE_H
