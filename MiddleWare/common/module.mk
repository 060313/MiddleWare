# SPDX-License-Identifier: MIT
# common 模块构建片段（由顶层 Makefile include）
#
# 承载设计类：UDSService / FileService / ONNXruntime / Timer（详见详细设计 §3.2~§3.5）
# 四者均为**全静态类**：不实例化、无构造与析构

COMMON_SRCS := \
    $(wildcard $(ROOT)/common/src/*.cpp)

COMMON_OBJS := $(patsubst $(ROOT)/%.cpp,$(BUILD)/%.o,$(COMMON_SRCS))

COMMON_INC := -I$(ROOT)/common/include

# ONNXruntime 依赖 ONNX Runtime SDK（硬依赖）
COMMON_INC += -I$(ONNXRUNTIME_ROOT)/include
