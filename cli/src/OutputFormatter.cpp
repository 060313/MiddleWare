#include "OutputFormatter.h"

#include <cstddef>
#include <cstdio>

#include "RequestCodec.h"

namespace cli {
namespace {

// ---------------- 终端列宽 ----------------
// "2000 年代终端 + UTF-8" 的老办法（数字节数）会让中文列错位：
// 一个汉字占 3 字节但只占 2 列。此处按 Unicode East Asian Width 的常用范围
// 判定码点是否占 2 列，再累加列宽 —— 表头与数据行才能对齐。

bool isWideCodePoint(unsigned int cp) {
    return (cp >= 0x1100u && cp <= 0x115Fu) ||    // 谚文字母
           (cp >= 0x2E80u && cp <= 0xA4CFu) ||    // CJK 部首 / 假名 / 汉字
           (cp >= 0xAC00u && cp <= 0xD7A3u) ||    // 谚文音节
           (cp >= 0xF900u && cp <= 0xFAFFu) ||    // CJK 兼容表意
           (cp >= 0xFE30u && cp <= 0xFE6Fu) ||    // CJK 兼容形式
           (cp >= 0xFF00u && cp <= 0xFF60u) ||    // 全角形式
           (cp >= 0xFFE0u && cp <= 0xFFE6u) ||    // 全角货币符号
           (cp >= 0x20000u && cp <= 0x3FFFDu);    // CJK 扩展 B 及以后
}

std::size_t displayWidth(const std::string& text) {
    std::size_t width = 0;
    std::size_t i = 0;
    while (i < text.size()) {
        const unsigned char lead = static_cast<unsigned char>(text[i]);
        unsigned int cp = 0;
        std::size_t length = 1;
        if (lead < 0x80u) {
            cp = lead;
        } else if ((lead & 0xE0u) == 0xC0u) {
            cp = lead & 0x1Fu;
            length = 2;
        } else if ((lead & 0xF0u) == 0xE0u) {
            cp = lead & 0x0Fu;
            length = 3;
        } else if ((lead & 0xF8u) == 0xF0u) {
            cp = lead & 0x07u;
            length = 4;
        } else {
            // 非法首字节：剩余的按单列 ASCII 计，保证不越界
            return width + (text.size() - i);
        }
        if (i + length > text.size()) {
            return width + 1;
        }
        for (std::size_t k = 1; k < length; ++k) {
            cp = (cp << 6) |
                 (static_cast<unsigned int>(static_cast<unsigned char>(text[i + k])) & 0x3Fu);
        }
        width += isWideCodePoint(cp) ? 2u : 1u;
        i += length;
    }
    return width;
}

std::string padRight(const std::string& text, std::size_t width) {
    const std::size_t current = displayWidth(text);
    if (current >= width) {
        return text;
    }
    return text + std::string(width - current, ' ');
}

// ---------------- 数值格式 ----------------

std::string formatPercent(float ratio) {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%.1f%%", static_cast<double>(ratio) * 100.0);
    return buf;
}

std::string formatKHz(int kHz) {
    char buf[24];
    std::snprintf(buf, sizeof(buf), "%.0f MHz", static_cast<double>(kHz) / 1000.0);
    return buf;
}

std::string yesNo(bool value) {
    return value ? "是" : "否";
}

}  // namespace

std::string OutputFormatter::formatState(const common::SystemFeature& feature) const {
    if (m_json) {
        // JSON 分支复用协议层的 SystemFeature 序列化：CLI 的 --json 与
        // schedd 回传给推理引擎的报文必须是同一套字段名，否则脚本要维护两套解析。
        // RequestCodec 无状态（无实例属性），栈上临时对象即可，不必持有成员。
        proto::RequestCodec codec;
        return codec.encodeSystemFeature(feature);
    }

    // 标签列宽取最长的 "CPU 利用率"（显示宽度 10）
    constexpr std::size_t kLabelWidth = 10;

    std::string npuLine;
    for (std::size_t i = 0; i < feature.unpu.size(); ++i) {
        if (i != 0) {
            npuLine += "  ";
        }
        npuLine += "core" + std::to_string(i) + " " + formatPercent(feature.unpu[i]);
    }

    std::string out;
    out += padRight("CPU 利用率", kLabelWidth) + ": " + formatPercent(feature.ucpu) + "\n";
    out += padRight("NPU 负载", kLabelWidth) + ": " + npuLine + "\n";
    out += padRight("频率", kLabelWidth) + ": CPU " + formatKHz(feature.cpuFreqKHz) +
           "   NPU " + formatKHz(feature.npuFreqKHz) + "\n";
    // stale 语义见 §6.3.1：采集失败时下游沿用上一份快照，
    // 必须显式提示，否则运维会把陈旧值当成实时值。
    out += padRight("状态", kLabelWidth) +
           (feature.stale ? ": 陈旧（采集失败，沿用上次值）\n" : ": 正常\n");
    return out;
}

std::string OutputFormatter::formatMode(common::SchedMode mode) const {
    return common::toString(mode);
}

std::string OutputFormatter::formatActionResult(const common::ActionInfo& info) const {
    if (m_json) {
        proto::RequestCodec codec;
        return codec.encodeActionInfo(info);
    }

    constexpr std::size_t kDeviceWidth = 8;
    constexpr std::size_t kResultWidth = 6;
    constexpr std::size_t kAdjustWidth = 6;
    constexpr std::size_t kFreqWidth = 12;

    std::string out;
    out += "任务 " + std::to_string(info.taskId) + "（动作 " + std::to_string(info.actionIndex) +
           "）：" + (info.ok ? "成功" : "失败") + "\n";
    if (!info.message.empty()) {
        out += "  提示: " + info.message + "\n";
    }

    out += padRight("设备", kDeviceWidth) + padRight("结果", kResultWidth) +
           padRight("夹取", kAdjustWidth) + padRight("实际频率", kFreqWidth) + "提示\n";

    for (const common::ResultInfo& result : info.results) {
        out += padRight(result.device, kDeviceWidth) + padRight(yesNo(result.ok), kResultWidth) +
               padRight(yesNo(result.adjusted), kAdjustWidth) +
               padRight(result.ok ? formatKHz(result.appliedFreqKHz) : std::string("-"),
                        kFreqWidth) +
               (result.message.empty() ? std::string("-") : result.message) + "\n";
    }
    return out;
}

}  // namespace cli
