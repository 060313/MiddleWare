# service —— Service 子系统（服务接口层，UDS）
add_library(sched_service STATIC
  src/ServiceInterface.cpp
  src/RequestDispatcher.cpp
)

target_include_directories(sched_service PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_features(sched_service PUBLIC cxx_std_17)
target_link_libraries(sched_service PUBLIC sched_agent sched_proto)

if(SCHED_BUILD_TESTS)
  add_executable(service_test test/service_test.cpp)
  target_link_libraries(service_test PRIVATE sched_service)
  add_test(NAME service_test COMMAND service_test)
endif()
