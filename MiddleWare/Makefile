# SPDX-License-Identifier: MIT
# ============================================================================
# CPU+XPU 异构调度中间件 —— 顶层构建入口（纯手写 Makefile，不使用 CMake）
# ============================================================================
#
# 结构：本 Makefile 定义通用编译规则与目标，各模块以 module.mk 片段提供
#       源文件列表（SERVICE_SRCS / AGENT_SRCS / ...）。
#
# 用法：
#   make            编译全部（产出 build/schedd）
#   make test       编译并运行单元测试
#   make clean      清理构建产物
#   make rebuild    清理后重编
#   make docker-build   构建开发镜像
#   make docker-shell   进入 openEuler 容器
#   make help       列出全部目标
# ============================================================================

# ---------------------------------------------------------------------------
# 路径与工具
# ---------------------------------------------------------------------------
ROOT       := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
BUILD      ?= $(ROOT)/build
CXX        ?= g++
CXXFLAGS   ?= -std=c++17 -Wall -Wextra -O2 -g -I$(ROOT)
LDFLAGS    ?=
JOBS       ?= 4
DOCKER_IMG ?= schedd-dev:latest

# ONNX Runtime SDK 根目录（硬依赖）
ONNXRUNTIME_ROOT ?= /opt/onnxruntime

# ---------------------------------------------------------------------------
# 模块片段（各模块提供 *_SRCS / *_INC / *_OBJS）
# ---------------------------------------------------------------------------
include $(ROOT)/common/module.mk
include $(ROOT)/data/module.mk
include $(ROOT)/agent/module.mk
include $(ROOT)/system/module.mk
include $(ROOT)/service/module.mk
include $(ROOT)/app/module.mk

# ---------------------------------------------------------------------------
# 汇总
# ---------------------------------------------------------------------------
ALL_SRCS := $(COMMON_SRCS) $(DATA_SRCS) $(AGENT_SRCS) $(SYSTEM_SRCS) $(SERVICE_SRCS) $(APP_SRCS)
ALL_OBJS := $(COMMON_OBJS) $(DATA_OBJS) $(AGENT_OBJS) $(SYSTEM_OBJS) $(SERVICE_OBJS) $(APP_OBJS)

# app/src/main.cpp 单独链接进可执行文件，不进入通用对象池
LIB_SRCS := $(filter-out $(ROOT)/app/src/main.cpp,$(ALL_SRCS))

INC := $(COMMON_INC) $(DATA_INC) $(AGENT_INC) $(SYSTEM_INC) $(SERVICE_INC) $(APP_INC) $(TEST_INC)

# 库对象（除 main）
LIB_OBJS  := $(patsubst $(ROOT)/%.cpp,$(BUILD)/%.o,$(LIB_SRCS))
MAIN_OBJ  := $(patsubst $(ROOT)/%.cpp,$(BUILD)/%.o,$(ROOT)/app/src/main.cpp)

# ONNX Runtime 库（硬依赖）
ONNX_LIBS := -L$(ONNXRUNTIME_ROOT)/lib -lonnxruntime

# ---------------------------------------------------------------------------
# 目标
# ---------------------------------------------------------------------------
.PHONY: all build test clean rebuild docker-build docker-shell status help
.PHONY: print-srcs

all: build

build: $(SCHEDD_BIN)

# 链接可执行文件
$(SCHEDD_BIN): $(LIB_OBJS) $(MAIN_OBJ)
	@mkdir -p $(dir $@)
	$(CXX) $^ -o $@ $(LDFLAGS) $(ONNX_LIBS)

# 通用编译规则
$(BUILD)/%.o: $(ROOT)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(INC) -c $< -o $@

# 单元测试（test/ 下若存在 *_test.cpp 则编译运行）
TEST_SRCS := $(wildcard $(ROOT)/test/*_test.cpp)
TEST_INC  := -I$(ROOT)/test

test: $(SCHEDD_BIN)
ifneq ($(strip $(TEST_SRCS)),)
	@for t in $(TEST_SRCS); do \
	    name=$$(basename $$t .cpp); \
	    echo "编译测试: $$name"; \
	    $(CXX) $(CXXFLAGS) $(INC) $$t $$(filter-out $(MAIN_OBJ),$(LIB_OBJS)) \
	        -o $(BUILD)/$$name $(LDFLAGS) $(ONNX_LIBS) || exit 1; \
	    echo "运行测试: $$name"; \
	    $(BUILD)/$$name || exit 1; \
	done
else
	@echo "（暂无测试：test/*_test.cpp 为空）"
endif

clean:
	rm -rf $(BUILD)

rebuild: clean build

docker-build:
	docker build -t $(DOCKER_IMG) .

docker-shell: docker-build
	docker run --rm -it -v "$(ROOT)":/work -w /work $(DOCKER_IMG) bash

status:
	@echo "ROOT             = $(ROOT)"
	@echo "BUILD            = $(BUILD)"
	@echo "CXX              = $(CXX)"
	@echo "CXXFLAGS         = $(CXXFLAGS)"
	@echo "ONNXRUNTIME_ROOT = $(ONNXRUNTIME_ROOT)"
	@echo "DOCKER_IMG       = $(DOCKER_IMG)"
	@echo "源文件数         = $(words $(ALL_SRCS))"

print-srcs:
	@for s in $(ALL_SRCS); do echo $$s; done

help:
	@grep -E '^## ' $(MAKEFILE_LIST) | sed 's/## //'
