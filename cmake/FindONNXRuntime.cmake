# 查找 ONNX Runtime（可选依赖）
#
# 提示顺序：-DONNXRUNTIME_ROOT=<dir> > 环境变量 ONNXRUNTIME_ROOT > 默认搜索路径
# 目标：ONNXRuntime::onnxruntime（IMPORTED）

find_path(ONNXRUNTIME_INCLUDE_DIR
  NAMES onnxruntime_cxx_api.h
  HINTS ENV ONNXRUNTIME_ROOT ${ONNXRUNTIME_ROOT}
        "${CMAKE_SOURCE_DIR}/third_party/onnxruntime"
  PATH_SUFFIXES include include/onnxruntime include/onnxruntime/core/session
)

find_library(ONNXRUNTIME_LIBRARY
  NAMES onnxruntime
  HINTS ENV ONNXRUNTIME_ROOT ${ONNXRUNTIME_ROOT}
        "${CMAKE_SOURCE_DIR}/third_party/onnxruntime"
  PATH_SUFFIXES lib lib64
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(ONNXRuntime
  REQUIRED_VARS ONNXRUNTIME_LIBRARY ONNXRUNTIME_INCLUDE_DIR
  VERSION_VAR ONNXRUNTIME_VERSION)

if(ONNXRuntime_FOUND AND NOT TARGET ONNXRuntime::onnxruntime)
  add_library(ONNXRuntime::onnxruntime UNKNOWN IMPORTED)
  set_target_properties(ONNXRuntime::onnxruntime PROPERTIES
    IMPORTED_LOCATION "${ONNXRUNTIME_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${ONNXRUNTIME_INCLUDE_DIR}")
endif()

mark_as_advanced(ONNXRUNTIME_INCLUDE_DIR ONNXRUNTIME_LIBRARY)
