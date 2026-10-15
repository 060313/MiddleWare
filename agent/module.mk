# SPDX-License-Identifier: MIT
# agent 模块构建片段（由顶层 Makefile include）
#
# 承载设计类：schedManager / systemMonitor / decisionMaker / decisionExecutor
#             （详见详细设计 §7.5~§7.8）

AGENT_SRCS := \
    $(wildcard $(ROOT)/agent/src/*.cpp)

AGENT_OBJS := $(patsubst $(ROOT)/%.cpp,$(BUILD)/%.o,$(AGENT_SRCS))

AGENT_INC := -I$(ROOT)/agent/include
