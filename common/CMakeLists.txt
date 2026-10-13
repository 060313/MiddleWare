# common —— 公共类型 / 共享内存状态快照 / 配置解析
add_library(sched_common STATIC
  src/Types.cpp
  src/StateSnapshot.cpp
  src/ConfigLoader.cpp
)

target_include_directories(sched_common PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_features(sched_common PUBLIC cxx_std_17)
target_link_libraries(sched_common PUBLIC Threads::Threads sched_warnings)
