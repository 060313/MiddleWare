# SPDX-License-Identifier: MIT
# data 模块构建片段（由顶层 Makefile include）
#
# 数据操作封装类（common 基础设施层，不计入 12 个设计类）：
#   ConfigFile / ModelFile / ContractFile（详见详细设计 §4.5）

DATA_SRCS := \
    $(wildcard $(ROOT)/data/src/*.cpp)

DATA_OBJS := $(patsubst $(ROOT)/%.cpp,$(BUILD)/%.o,$(DATA_SRCS))

DATA_INC := -I$(ROOT)/data/include
