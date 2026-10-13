#pragma once

// 报文编解码：长度前缀(4B) + 类型头(4B) + JSON 载荷。
// 见《软件详细设计文档》5.2。

#include <string>

#include "Types.h"

namespace service {

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

}  // namespace service
