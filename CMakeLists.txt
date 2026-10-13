cmake_minimum_required(VERSION 3.16)

project(schedd_middleware
        VERSION 0.1.0
        DESCRIPTION "面向 CPU+XPU 异构系统的智能调度中间件"
        LANGUAGES CXX)

# ---------------------------------------------------------------- 语言标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "构建类型" FORCE)
endif()

# ---------------------------------------------------------------- 选项
option(SCHED_ENABLE_ONNX "启用 ONNX Runtime 推理后端" ON)
option(SCHED_BUILD_TESTS "构建单元 / 冒烟测试"        ON)

# ---------------------------------------------------------------- 全局告警
add_library(sched_warnings INTERFACE)
target_compile_options(sched_warnings INTERFACE
  $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall;-Wextra;-Wpedantic>)

find_package(Threads REQUIRED)

if(SCHED_BUILD_TESTS)
  enable_testing()
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# ---------------------------------------------------------------- 子模块（自下而上）
add_subdirectory(common)   # sched_common
add_subdirectory(tool)     # sched_tool
add_subdirectory(monitor)  # sched_monitor
add_subdirectory(system)   # sched_system
add_subdirectory(agent)    # sched_agent
add_subdirectory(service)  # sched_service
add_subdirectory(app)      # schedd
add_subdirectory(cli)      # schedctl

# ---------------------------------------------------------------- 可执行产物统一放 build/bin
set_target_properties(schedd schedctl PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

message(STATUS "schedd_middleware: BUILD_TYPE=${CMAKE_BUILD_TYPE} "
               "ONNX=${SCHED_ENABLE_ONNX} TESTS=${SCHED_BUILD_TESTS}")
