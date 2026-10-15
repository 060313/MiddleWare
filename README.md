# CPU+XPU 异构调度中间件

面向 openEuler / aarch64（RK3588）的 CPU+XPU 异构调度中间件，以 DQN 作调度决策。
本仓库为**原型框架**：完整落地《详细设计文档（新）》的 **A 方案 12 个设计类**，
接口按设计文档方法表逐条声明，业务实现待逐批填充。

## 构建

**不使用 CMake**，纯手写 Makefile（各模块以 `module.mk` 片段提供源文件）。

```bash
make            # 编译全部，产出 build/schedd
make test       # 编译并运行单元测试
make clean      # 清理
make rebuild    # 清理后重编
make status     # 打印当前配置
```

## Docker（openEuler 24.03-LTS）

```bash
make docker-build        # 构建开发镜像 schedd-dev:latest
make docker-shell        # 进入容器（源码挂载到 /work）
```

容器内构建（避免宿主 CMakeCache 干扰）：

```bash
make -C /work
```

ONNX Runtime SDK 为**硬依赖**，挂载到 `/opt/onnxruntime`：

```bash
docker run --rm -it \
    -v "$PWD":/work -w /work \
    -v /path/to/onnxruntime-linux-x64-<ver>:/opt/onnxruntime \
    schedd-dev:latest bash
```

## 目录结构

```
common/     UDSService / FileService / ONNXruntime      （公共基础设施，3 类）
agent/      schedManager / systemMonitor /
            decisionMaker / decisionExecutor            （Agent 子系统，4 类）
system/     systemInterface / systemManager /
            CPU / NPU                                   （System 子系统，4 类）
service/    serviceInterface                            （Service 子系统，1 类）
app/        ScheddApp 装配 + schedd 入口                （进程装配层）
cli/        （空 —— 界面设计，待建）
data/       （空 —— 数据设计，待建）
test/       单元测试
deploy/     schedd.conf / schedd.service
```

## 装配关系

```
ServiceInterface ──关联──► schedManager
                              ├─关联──► systemMonitor ──关联──► SystemInterface
                              ├─关联──► decisionMaker ──依赖──► ONNXruntime
                              └─关联──► decisionExecutor

SystemInterface ──关联──► SystemManager ──组合──► CPU / NPU
CPU / NPU ──依赖──► FileService
ServiceInterface ──依赖──► UDSService
```

## 设计文档

- 《详细设计文档（新）》—— A 方案 12 类 + 数据设计
- 《软件体系结构设计文档》
- 《用户界面设计文档》

文档位于正本 `Reference/document/02_软件设计/`，本目录下 `software-design-documents/` 为副本。

## 状态

框架搭建阶段：接口已按设计文档声明，标注 `TODO(实现层)` 处为待填充实现。
