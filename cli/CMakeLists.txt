# cli —— 管理人员命令行客户端 schedctl
add_executable(schedctl
  src/main.cpp
  src/CtlMain.cpp
  src/Command.cpp
  src/StatusCommand.cpp
  src/ModeCommand.cpp
  src/ServiceCommand.cpp
  src/CtlClient.cpp
  src/SystemdCtl.cpp
  src/OutputFormatter.cpp
)

target_include_directories(schedctl PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_features(schedctl PRIVATE cxx_std_17)
# sched_proto 提供帧编解码：客户端与 schedd 共用同一份格式定义，
# 避免在 CtlClient 里重复实现长度前缀 / 读满 N 字节的循环。
target_link_libraries(schedctl PRIVATE sched_common sched_proto)
