# ===========================================================
#  MiddleWare · system 子系统单元测试
#
#  用法（在 test/system_tests 下执行）：
#    make                 编译并运行全部套件
#    make deviceFile_test 只跑一个套件
#    make list            列出套件与被测源码
#    make clean           删掉本目录的 build/
#
#  与上层的唯一耦合是 MW：指向上层代码根（含 common/ 与 system/）。
#  本目录自成一体，产物落在 ./build，跑测试时 cwd 就是本目录。
# ===========================================================

MW       ?= ../..
CXX      ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -O1 -g
CPPFLAGS := -I$(MW) -I$(MW)/test
BUILD    ?= build

# MinGW / MSYS / Cygwin 下产物必须带 .exe，否则 make 认为目标不存在、每次重编
UNAME := $(shell uname -s)
ifneq (,$(findstring MINGW,$(UNAME)))
SUFFIX := .exe
endif
ifneq (,$(findstring MSYS,$(UNAME)))
SUFFIX := .exe
endif
ifneq (,$(findstring CYGWIN,$(UNAME)))
SUFFIX := .exe
endif

# 一个子目录 = 一个套件 = 一个可执行文件，目录内所有 .cpp 一起链接，
# 目录内所有 .h 作为依赖（改头文件会触发重编）
SUITES := deviceFile_test

# 被测源码：deviceFile 是 header-only，只需 FileService 的实现
MW_SRC := $(MW)/common/src/FileService.cpp

.PHONY: all list clean $(SUITES) $(addprefix run-,$(SUITES))

all: $(addprefix run-,$(SUITES))

list:
	@echo suites: $(SUITES)
	@echo sources: $(MW_SRC)
	@echo cases: deviceFile_test/deviceFile_cases/initCase + freqTableCases/case0..9 + xpuCases/case0..4

clean:
	rm -rf $(BUILD)

define SUITE_RULES
$(BUILD)/$(1)$(SUFFIX): $$(wildcard $(1)/*.cpp) $$(wildcard $(1)/*.h) $(MW_SRC)
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -o $$@ $$(filter %.cpp,$$^)

run-$(1): $(BUILD)/$(1)$(SUFFIX)
	@./$(BUILD)/$(1)$(SUFFIX)

$(1): run-$(1)
endef

$(foreach s,$(SUITES),$(eval $(call SUITE_RULES,$(s))))
