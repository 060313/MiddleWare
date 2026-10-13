#include "RequestCodec.h"

#include <cstdint>
#include <utility>

#include "Json.h"

namespace proto {
namespace {

// 手写大端转换而不用 htonl / ntohl：
//   * MinGW 下这些符号在 <winsock2.h> 里，引入会拖入 winsock 链接依赖；
//   * 帧格式是冻结契约，字节序逻辑就这 8 行，自己写反而更容易核对。
// 统一用"网络序 = 大端"（§5.2），保证 x86 仿真与 aarch64 上板行为一致。
void putU32(std::string& out, std::uint32_t value) {
    out.push_back(static_cast<char>((value >> 24) & 0xFFu));
    out.push_back(static_cast<char>((value >> 16) & 0xFFu));
    out.push_back(static_cast<char>((value >> 8) & 0xFFu));
    out.push_back(static_cast<char>(value & 0xFFu));
}

std::uint32_t getU32(const char* p) {
    return (static_cast<std::uint32_t>(static_cast<unsigned char>(p[0])) << 24) |
           (static_cast<std::uint32_t>(static_cast<unsigned char>(p[1])) << 16) |
           (static_cast<std::uint32_t>(static_cast<unsigned char>(p[2])) << 8) |
           static_cast<std::uint32_t>(static_cast<unsigned char>(p[3]));
}

}  // namespace

bool RequestCodec::decode(const std::string& frame,
                          common::MessageType& type,
                          std::string& payload) {
    if (frame.size() < static_cast<std::size_t>(kHeaderSize)) {
        return false;
    }

    const std::uint32_t length = getU32(frame.data());
    // 长度前缀必须与实到字节数严格相等。不足 = 半包被误当整帧；
    // 超长 = 粘包（本次只到了一部分）。两种都不能放行，否则后续帧全部错位。
    if (length != frame.size()) {
        return false;
    }

    const std::uint32_t rawType = getU32(frame.data() + 4);
    common::MessageType parsed = common::MessageType::Unknown;
    switch (rawType) {
        case static_cast<std::uint32_t>(common::MessageType::AskSched):
            parsed = common::MessageType::AskSched;
            break;
        case static_cast<std::uint32_t>(common::MessageType::GetState):
            parsed = common::MessageType::GetState;
            break;
        case static_cast<std::uint32_t>(common::MessageType::SetMode):
            parsed = common::MessageType::SetMode;
            break;
        default:
            return false;   // 未识别类型：连 Unknown 也拒收，交由对端重新组帧
    }

    // 出参只在全部校验通过后写入 —— 失败时调用方持有的仍是原值（§6.3.6 后置条件）。
    payload.assign(frame, static_cast<std::size_t>(kHeaderSize),
                   frame.size() - static_cast<std::size_t>(kHeaderSize));
    type = parsed;
    return true;
}

std::string RequestCodec::encode(common::MessageType type, const std::string& payload) {
    const std::size_t total = static_cast<std::size_t>(kHeaderSize) + payload.size();
    std::string frame;
    frame.reserve(total);
    putU32(frame, static_cast<std::uint32_t>(total));            // 长度含帧头，接收方可先读 8B 再读剩余
    putU32(frame, static_cast<std::uint32_t>(type));
    frame += payload;
    return frame;
}

std::string RequestCodec::encodeActionInfo(const common::ActionInfo& info) {
    common::Json root = common::Json::object();
    // taskId 以 double 承载：Json 只有一种数值类型，量级 ≤ 2^53 时无精度损失，
    // 而 taskId 是进程内单调递增的计数器，不可能逼近该量级。
    root.set("taskId", common::Json(static_cast<double>(info.taskId)));
    root.set("ok", common::Json(info.ok));
    root.set("actionIndex", common::Json(info.actionIndex));

    common::Json results = common::Json::array();
    for (const common::ResultInfo& result : info.results) {
        common::Json item = common::Json::object();
        item.set("device", common::Json(result.device));
        item.set("ok", common::Json(result.ok));
        item.set("adjusted", common::Json(result.adjusted));
        item.set("appliedFreqKHz", common::Json(result.appliedFreqKHz));
        item.set("message", common::Json(result.message));
        results.push(std::move(item));
    }
    root.set("results", std::move(results));
    root.set("message", common::Json(info.message));

    return root.dump();
}

std::string RequestCodec::encodeSystemFeature(const common::SystemFeature& feature) {
    common::Json root = common::Json::object();
    root.set("ucpu", common::Json(static_cast<double>(feature.ucpu)));

    common::Json unpu = common::Json::array();
    for (float load : feature.unpu) {
        unpu.push(common::Json(static_cast<double>(load)));
    }
    root.set("unpu", std::move(unpu));

    root.set("cpuFreqKHz", common::Json(feature.cpuFreqKHz));
    root.set("npuFreqKHz", common::Json(feature.npuFreqKHz));
    // stale 必须一并下发：CLI 据此高亮"状态陈旧"，脚本也可据此判定数据可信度。
    root.set("stale", common::Json(feature.stale));

    return root.dump();
}

}  // namespace proto
