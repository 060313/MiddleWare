# proto —— schedd / schedctl 共用的报文帧格式（长度前缀 + 类型头 + 载荷）
# 独立成薄库：只依赖 sched_common，不牵连 agent / system / ONNX Runtime，
# 因此命令行客户端 schedctl 可以安全地链接它。
add_library(sched_proto STATIC
  src/RequestCodec.cpp
)

target_include_directories(sched_proto PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_features(sched_proto PUBLIC cxx_std_17)
target_link_libraries(sched_proto PUBLIC sched_common)
