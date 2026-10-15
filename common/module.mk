# SPDX-License-Identifier: MIT
# common 模块构建片段（由顶层 Makefile include）
#
# 承载设计类：UDSService / FileService / ONNXruntime（详见详细设计 §7.2~§7.4）

COMMON_SRCS := \
    $(wildcard $(ROOT)/common/src/*.cpp)

COMMON_OBJS := $(patsubst $(ROOT)/%.cpp,$(BUILD)/%.o,$(COMMON_SRCS))

COMMON_INC := -I$(ROOT)/common/include

# ONNXruntime 依赖 ONNX Runtime SDK（硬依赖）
COMMON_INC += -I$(ONNXRUNTIME_ROOT)/include
