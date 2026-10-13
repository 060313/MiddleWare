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
target_link_libraries(schedctl PRIVATE sched_common)
