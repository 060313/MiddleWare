#pragma once

// 报文编解码：长度前缀(4B) + 类型头(4B) + JSON 载荷。
// 见《软件详细设计文档》5.2。
//
// 归属 proto 模块：帧格式是 schedd（服务端）与 schedctl（客户端）之间的共同契约，
// 两侧必须链接同一份实现 —— 各写一份，格式一改就会静默失配。

#include <string>

#include "Types.h"

namespace proto {

class RequestCodec {
public:
    RequestCodec() = default;
    ~RequestCodec() = default;

    static constexpr int kHeaderSize = 8;   // len(4) + type(4)

    bool decode(const std::string& frame, common::MessageType& type, std::string& payload);

    std::string encode(common::MessageType type, const std::string& payload);
    std::string encodeActionInfo(const common::ActionInfo& info);
    std::string encodeSystemFeature(const common::SystemFeature& feature);
};

}  // namespace proto
