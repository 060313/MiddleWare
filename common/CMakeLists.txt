# common —— 公共类型 / 共享内存状态快照 / 配置解析
add_library(sched_common STATIC
  src/Types.cpp
  src/StateSnapshot.cpp
  src/SnapshotRegion.cpp
  src/ConfigLoader.cpp
)

target_include_directories(sched_common PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_features(sched_common PUBLIC cxx_std_17)
target_link_libraries(sched_common PUBLIC Threads::Threads sched_warnings)

# Linux 上 shm_open / mmap 属 POSIX 实时扩展；glibc ≥ 2.34 已并入 libc，
# 旧发行版仍需 -lrt（openEuler 24.03 为 glibc 2.38，此处仅为兼容保留）。
if(UNIX AND NOT APPLE)
  target_link_libraries(sched_common PUBLIC rt)
endif()
