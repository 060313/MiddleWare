# SPDX-License-Identifier: MIT
# app 模块构建片段（由顶层 Makefile include）
#
# 装配层：ScheddApp + schedd 进程入口（不对应设计类）

APP_SRCS := \
    $(wildcard $(ROOT)/app/src/*.cpp)

APP_OBJS := $(patsubst $(ROOT)/%.cpp,$(BUILD)/%.o,$(APP_SRCS))

APP_INC := -I$(ROOT)/app/include

# 可执行文件：schedd
SCHEDD_BIN := $(BUILD)/schedd
