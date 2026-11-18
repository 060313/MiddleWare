# SPDX-License-Identifier: MIT
# service 模块构建片段（由顶层 Makefile include）
#
# 承载设计类：serviceInterface（详见详细设计 §3.1）

SERVICE_SRCS := \
    $(wildcard $(ROOT)/service/src/*.cpp)

SERVICE_OBJS := $(patsubst $(ROOT)/%.cpp,$(BUILD)/%.o,$(SERVICE_SRCS))

SERVICE_INC := -I$(ROOT)/service/include
