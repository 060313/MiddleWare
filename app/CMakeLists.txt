# app —— 守护进程 schedd
add_executable(schedd
  src/main.cpp
  src/ScheddApp.cpp
)

target_include_directories(schedd PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_features(schedd PRIVATE cxx_std_17)
target_link_libraries(schedd PRIVATE sched_service)
