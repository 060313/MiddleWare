# SPDX-License-Identifier: MIT
# system 模块构建片段（由顶层 Makefile include）
#
# 承载设计类：systemInterface / systemManager / CPU / NPU（详见详细设计 §7.9~§7.12）

SYSTEM_SRCS := \
    $(wildcard $(ROOT)/system/src/*.cpp)

SYSTEM_OBJS := $(patsubst $(ROOT)/%.cpp,$(BUILD)/%.o,$(SYSTEM_SRCS))

SYSTEM_INC := -I$(ROOT)/system/include
