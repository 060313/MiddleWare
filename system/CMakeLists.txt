# system —— System 子系统（状态监管层 + 硬件服务层）
add_library(sched_system STATIC
  src/SystemInterface.cpp
  src/SystemManager.cpp
  src/OsMonitor.cpp
)

target_include_directories(sched_system PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_features(sched_system PUBLIC cxx_std_17)
# SystemManager 直接使用 common 的 SnapshotRegion / StateSnapshot，
# 显式声明而不依赖 sched_tool → sched_common 的传递可见性。
target_link_libraries(sched_system PUBLIC sched_tool sched_monitor sched_common)

if(SCHED_BUILD_TESTS)
  add_executable(system_test test/system_test.cpp)
  target_link_libraries(system_test PRIVATE sched_system)
  add_test(NAME system_test COMMAND system_test)
endif()
