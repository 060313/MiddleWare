# tool —— 基础设施：跨平台文件封装 + NPU 抽象（HAL）
add_library(sched_tool STATIC
  src/SystemFS.cpp
  src/INpu.cpp
  src/RealNpu.cpp
  src/VirtualNpu.cpp
)

target_include_directories(sched_tool PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_features(sched_tool PUBLIC cxx_std_17)
target_link_libraries(sched_tool PUBLIC sched_common)
