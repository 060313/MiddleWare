# monitor —— CPU / NPU 利用率与负载计算
add_library(sched_monitor STATIC
  src/Monitor.cpp
)

target_include_directories(sched_monitor PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_features(sched_monitor PUBLIC cxx_std_17)
target_link_libraries(sched_monitor PUBLIC sched_tool)

if(SCHED_BUILD_TESTS)
  add_executable(monitor_test test/monitor_test.cpp)
  target_link_libraries(monitor_test PRIVATE sched_monitor)
  add_test(NAME monitor_test COMMAND monitor_test)
endif()
