# agent —— Agent 子系统（调度决策层，DQN 推理）
add_library(sched_agent STATIC
  src/SchedManager.cpp
  src/SystemMonitor.cpp
  src/DecisionMaker.cpp
  src/FeatureEncoder.cpp
  src/ActionDecoder.cpp
  src/DecisionExecutor.cpp
)

target_include_directories(sched_agent PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_features(sched_agent PUBLIC cxx_std_17)
target_link_libraries(sched_agent PUBLIC sched_system)

# ONNX Runtime 为可选依赖：找到则编译真实推理路径，否则 DecisionMaker 退化为 stub
if(SCHED_ENABLE_ONNX)
  find_package(ONNXRuntime QUIET)
endif()

if(ONNXRuntime_FOUND)
  target_link_libraries(sched_agent PRIVATE ONNXRuntime::onnxruntime)
  target_compile_definitions(sched_agent PRIVATE SCHED_HAS_ONNX=1)
  message(STATUS "agent: 启用 ONNX Runtime ${ONNXRuntime_VERSION}")
else()
  message(STATUS "agent: 未启用 ONNX Runtime，DecisionMaker 使用 stub 实现")
endif()

if(SCHED_BUILD_TESTS)
  add_executable(agent_test test/agent_test.cpp)
  target_link_libraries(agent_test PRIVATE sched_agent)
  add_test(NAME agent_test COMMAND agent_test)
endif()
