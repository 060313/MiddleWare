# CPU+XPU 异构调度中间件 —— 便捷构建入口
# CMake 是构建的唯一真相；本 Makefile 只做转发封装。

BUILD_DIR  ?= build
BUILD_TYPE ?= Debug
JOBS       ?= 4
CMAKE      ?= cmake
DOCKER_IMG ?= schedd-dev:latest

.PHONY: all configure build test clean rebuild docker-build docker-shell status help

all: build

## configure : 生成构建目录
configure:
	$(CMAKE) -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=$(BUILD_TYPE)

## build     : 编译全部目标
build: configure
	$(CMAKE) --build $(BUILD_DIR) -j$(JOBS)

## test      : 运行 ctest
test: build
	cd $(BUILD_DIR) && ctest --output-on-failure

## clean     : 删除构建目录
clean:
	rm -rf $(BUILD_DIR)

## rebuild   : 清理后重新编译
rebuild: clean build

## docker-build  : 构建开发镜像
docker-build:
	docker build -t $(DOCKER_IMG) .

## docker-shell  : 进入 openEuler 容器（源码目录挂载到 /work）
docker-shell: docker-build
	docker run --rm -it -v "$(CURDIR)":/work -w /work $(DOCKER_IMG) bash

## status    : 打印当前配置
status:
	@echo "BUILD_DIR  = $(BUILD_DIR)"
	@echo "BUILD_TYPE = $(BUILD_TYPE)"
	@echo "JOBS       = $(JOBS)"
	@echo "CMAKE      = $(CMAKE)"
	@echo "DOCKER_IMG = $(DOCKER_IMG)"

## help      : 列出可用目标
help:
	@grep -E '^## ' $(MAKEFILE_LIST) | sed 's/## //'
