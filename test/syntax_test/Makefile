# SPDX-License-Identifier: MIT
# ============================================================================
# 语法测试套件（test/syntax_test）
# ============================================================================
#
# 范围：**只做语法与编译层检查**，不测试功能行为、不测试 DQN 算法有效性。
#   - syntax ：全部源文件 -fsyntax-only 严格语法检查
#              （-Wall -Wextra -Werror，与顶层构建同一套诊断开关）；
#   - headers：全部模块头文件「自足性」检查——每个头单独作为翻译单元编译，
#              验证头文件自身不遗漏所需 include；
#   - objects：真实编译（-c 产出 .o），覆盖语法检查不含的汇编与目标文件层。
#
# 用法：
#   make -C test/syntax_test            # syntax + headers（无 SDK 环境也可跑）
#   make -C test/syntax_test syntax     # 仅语法检查
#   make -C test/syntax_test headers    # 仅头文件自足性
#   make -C test/syntax_test objects    # 编译为目标文件
#   make -C test/syntax_test clean
#
# 环境说明：
#   - ONNXruntime.cpp 依赖 ONNX Runtime SDK（<onnxruntime_cxx_api.h>）：
#     未安装 SDK 的环境（如 Windows 本机）自动 SKIP 该文件，容器内正常编译。
#   - Windows 本机跑 objects 时 .o 输出目录必须为 ASCII 路径
#     （中文路径汇编器报 can't create）：
#       make -C test/syntax_test objects OBJDIR=C:/Temp/wchk/syntax_obj
#
# 本目录不含 *_test.cpp，顶层 `make test` 不受影响。
# ============================================================================

SYNTAX_ROOT := $(abspath $(dir $(firstword $(MAKEFILE_LIST))))
ROOT  := $(abspath $(SYNTAX_ROOT)/../..)
BUILD ?= $(SYNTAX_ROOT)/build
OBJDIR ?= $(BUILD)/obj

CXX ?= g++

ONNXRUNTIME_ROOT ?= /opt/onnxruntime
HAS_ONNX_SDK := $(shell test -f "$(ONNXRUNTIME_ROOT)/include/onnxruntime_cxx_api.h" && echo 1 || echo 0)

# 复用顶层构建的模块源文件清单（module.mk 以 $(ROOT) 为基准展开）
include $(ROOT)/common/module.mk
include $(ROOT)/data/module.mk
include $(ROOT)/agent/module.mk
include $(ROOT)/system/module.mk
include $(ROOT)/service/module.mk
include $(ROOT)/app/module.mk

ALL_SRCS := $(COMMON_SRCS) $(DATA_SRCS) $(AGENT_SRCS) $(SYSTEM_SRCS) $(SERVICE_SRCS) $(APP_SRCS)

INC := $(COMMON_INC) $(DATA_INC) $(AGENT_INC) $(SYSTEM_INC) $(SERVICE_INC) $(APP_INC)

# 与顶层构建同一套诊断开关（去掉 -O2 -g：语法层检查不需要优化信息）
FLAGS := -std=c++17 -Wall -Wextra -Werror -I$(ROOT) $(INC)

HEADERS := $(wildcard $(ROOT)/common/include/*.h) \
           $(wildcard $(ROOT)/data/include/*.h) \
           $(wildcard $(ROOT)/agent/include/*.h) \
           $(wildcard $(ROOT)/system/include/*.h) \
           $(wildcard $(ROOT)/service/include/*.h) \
           $(wildcard $(ROOT)/app/include/*.h)

# 环境依赖的源文件（按文件基名登记，无对应环境时跳过，不计为失败）：
#   - ONNXruntime.cpp：依赖 ONNX Runtime SDK 头（<onnxruntime_cxx_api.h>）
#   - UDSService.cpp：依赖 POSIX 头（poll / epoll / eventfd / unix domain socket），
#     Windows 本机无这些头，容器（Linux）正常编译
NEEDS_SDK_FILE   := ONNXruntime.cpp
NEEDS_POSIX_FILE := UDSService.cpp
POSIX_OS := $(shell uname -s | grep -qE 'MINGW|MSYS|CYGWIN' && echo 0 || echo 1)

.PHONY: all syntax headers objects clean

all: syntax headers

syntax:
	@fail=0; pass=0; skip=0; \
	for s in $(ALL_SRCS); do \
	    if [ "$(HAS_ONNX_SDK)" != "1" ] && [ "$${s##*/}" = "$(NEEDS_SDK_FILE)" ]; then \
	        skip=$$((skip+1)); echo "SKIP(无 ONNX SDK): $${s#$(ROOT)/}"; continue; \
	    fi; \
	    if [ "$(POSIX_OS)" != "1" ] && [ "$${s##*/}" = "$(NEEDS_POSIX_FILE)" ]; then \
	        skip=$$((skip+1)); echo "SKIP(非 POSIX): $${s#$(ROOT)/}"; continue; \
	    fi; \
	    if $(CXX) $(FLAGS) -fsyntax-only "$$s"; then \
	        pass=$$((pass+1)); echo "PASS: $${s#$(ROOT)/}"; \
	    else \
	        fail=1; echo "FAIL: $${s#$(ROOT)/}"; \
	    fi; \
	done; \
	echo "== syntax：通过 $$pass，跳过 $$skip，失败 $$fail =="; \
	exit $$fail

headers:
	@fail=0; pass=0; \
	for h in $(HEADERS); do \
	    if $(CXX) $(FLAGS) -x c++ -fsyntax-only "$$h"; then \
	        pass=$$((pass+1)); echo "PASS: $${h#$(ROOT)/}"; \
	    else \
	        fail=1; echo "FAIL: $${h#$(ROOT)/}"; \
	    fi; \
	done; \
	echo "== headers：通过 $$pass，失败 $$fail =="; \
	exit $$fail

objects:
	@mkdir -p $(OBJDIR); \
	fail=0; pass=0; skip=0; \
	for s in $(ALL_SRCS); do \
	    if [ "$(HAS_ONNX_SDK)" != "1" ] && [ "$${s##*/}" = "$(NEEDS_SDK_FILE)" ]; then \
	        skip=$$((skip+1)); echo "SKIP(无 ONNX SDK): $${s#$(ROOT)/}"; continue; \
	    fi; \
	    if [ "$(POSIX_OS)" != "1" ] && [ "$${s##*/}" = "$(NEEDS_POSIX_FILE)" ]; then \
	        skip=$$((skip+1)); echo "SKIP(非 POSIX): $${s#$(ROOT)/}"; continue; \
	    fi; \
	    rel=$${s#$(ROOT)/}; \
	    obj=$(OBJDIR)/$${rel%.cpp}.o; \
	    mkdir -p $$(dirname $$obj); \
	    if $(CXX) $(FLAGS) -c "$$s" -o "$$obj"; then \
	        pass=$$((pass+1)); echo "PASS: $$rel"; \
	    else \
	        fail=1; echo "FAIL: $$rel"; \
	    fi; \
	done; \
	echo "== objects：通过 $$pass，跳过 $$skip，失败 $$fail =="; \
	exit $$fail

clean:
	rm -rf $(BUILD)
