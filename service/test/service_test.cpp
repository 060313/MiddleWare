// service 子系统冒烟测试
#include <cstdio>
#include <string>

#include "RequestCodec.h"
#include "RequestDispatcher.h"

int main() {
    service::RequestCodec codec;

    const std::string frame = codec.encode(common::MessageType::GetState, "{}");
    common::MessageType type = common::MessageType::Unknown;
    std::string payload;
    const bool decoded = codec.decode(frame, type, payload);
    std::printf("[service_test] encode/decode roundtrip ok=%d type=%d\n",
                static_cast<int>(decoded), static_cast<int>(type));

    // TODO: 断言编解码往返一致；用假 SchedManager 断言路由分发
    std::printf("[service_test] TODO: assertions pending\n");
    return 0;
}
