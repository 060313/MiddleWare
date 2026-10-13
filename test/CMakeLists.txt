# 批次验收测试 —— 按《软件详细设计文档》6.6.4 的批次切分，而非按模块切分。
# B1 一次覆盖拓扑层 L0 的叶子：跨 common / proto / agent / cli 四个模块，
# 若拆进各模块自己的 test/ 目录，同一份验收逻辑会被切碎且重复准备契约文件。
add_executable(b1_test b1_test.cpp)

target_include_directories(b1_test PRIVATE
  "${CMAKE_SOURCE_DIR}/cli/include"
)

# cli 的 OutputFormatter / SystemdCtl 目前编在 schedctl 可执行目标里、未独立成静态库，
# 这里直接引用其源文件，避免为测试而改动 cli 的既有构建结构。
target_sources(b1_test PRIVATE
  "${CMAKE_SOURCE_DIR}/cli/src/OutputFormatter.cpp"
  "${CMAKE_SOURCE_DIR}/cli/src/SystemdCtl.cpp"
)

target_compile_features(b1_test PRIVATE cxx_std_17)
target_link_libraries(b1_test PRIVATE sched_agent sched_proto sched_common)

add_test(NAME b1_test COMMAND b1_test)
# 契约与配置按相对路径读取，工作目录必须是项目根
set_tests_properties(b1_test PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
