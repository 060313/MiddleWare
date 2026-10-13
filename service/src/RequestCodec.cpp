#include "RequestCodec.h"

namespace service {

bool RequestCodec::decode(const std::string& frame,
                          common::MessageType& type,
                          std::string& payload) {
    // TODO: 校验长度前缀；解析类型头并转 MessageType；剩余部分为 JSON 载荷
    (void)frame;
    (void)type;
    (void)payload;
    return false;
}

std::string RequestCodec::encode(common::MessageType type, const std::string& payload) {
    // TODO: 拼 len(4) + type(4) + payload
    (void)type;
    return payload;
}

std::string RequestCodec::encodeActionInfo(const common::ActionInfo& info) {
    // TODO: ActionInfo → JSON
    (void)info;
    return std::string();
}

std::string RequestCodec::encodeSystemFeature(const common::SystemFeature& feature) {
    // TODO: SystemFeature → JSON
    (void)feature;
    return std::string();
}

}  // namespace service
