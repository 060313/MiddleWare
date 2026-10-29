# CPU+XPU 异构调度中间件

面向 openEuler / aarch64（RK3588）的 CPU+XPU 异构调度中间件，以 DQN 作调度决策。
本仓库为**原型框架**：按《详细设计》落地 **13 个设计类**，类名、属性、方法签名与
设计文档 §3 逐条对齐；标注 `TODO(实现层)` 处为待填充的 I/O 与算法实现。

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
common/     UDSService / FileService / ONNXruntime / Timer  （公共基础设施，4 类，全静态）
agent/      schedManager / systemMonitor /
            decisionMaker / decisionExecutor              （Agent 子系统，4 类）
system/     systemInterface / systemManager /
            CPU / NPU                                       （System 子系统，4 类）
service/    serviceInterface                                （Service 子系统，1 类）
app/        ScheddApp 装配 + schedd 入口                    （进程装配层，非设计类）
data/       ConfigFile / ModelFile / ContractFile           （数据封装，非设计类）
cli/        （空 —— schedctl 界面，待建）
test/       单元测试
deploy/     schedd.conf / schedd.service
```

## 装配关系

```
serviceInterface ──关联──► schedManager
                             ├─关联──► systemMonitor ──关联──► systemInterface
                             ├─关联──► decisionMaker ──依赖──► ONNXruntime（静态）
                             └─关联──► decisionExecutor ──关联──► systemMonitor

systemInterface ──关联──► systemManager ──组合──► CPU / NPU
CPU / NPU ──依赖──► FileService（静态）
serviceInterface ──依赖──► UDSService（静态）
```

三条贯穿全局的语义约束：

| 约束 | 含义 |
|---|---|
| 一次决策只动一台设备 | `Decision` 承载唯一 `Freq`，CPU / NPU 二选一，`ActionInfo` 恒含「一条动作 + 一个结果」（D-13 / D-14） |
| Agent 通往 System 的出口唯一 | 读状态与下发频率都经 `systemMonitor`，不直连设备类（D-12） |
| 失败靠枚举而非文本 | `ResultInfo.reason`（`Reason`）→ 协议码由 `serviceInterface::handleFrame` 单点映射（D-33） |

## 运行模型

单进程守护（systemd `Type=simple`），**两线程**（D-35）：

| 线程 | 入口 | 职责 |
|---|---|---|
| 服务线程 | `serviceInterface::serve()` → `UDSService::serve()` | epoll 事件循环；推理在本线程内同步完成 |
| 监控线程 | `Timer::poll()` 循环 | 到期调 `systemManager::refreshSystemInfo()` 刷新缓存 |

跨线程共享只有 `systemManager::systemInfo` 一处，两侧持 `sysMutex`。
启动装配 14 步、停机 6 步见设计 §6.1 / §6.3。

## 设计文档

- 正本：`software-design-documents/design.md`（6 章：类清单 / 用例 / 类设计 / 数据 / 裁决汇总 / 运行模型）
- 副本：`Reference/document/02_软件设计/详细设计文档（新）.md`（10 章，整体滞后于正本）
- 《软件体系结构设计文档》《用户界面设计文档》位于 `Reference/document/`

## 状态

框架完成、装配层可运行：`schedd --config <路径>` 会读配置 → 装配 13 类 → 首次加载模型 →
建监听 → 起两线程 → 等 `SIGTERM`。各子系统按分工逐步落地实现与单元测试（`make test`）：

| 模块 | 状态 |
|---|---|
| `UDSService` | ✅ 已实现：epoll 事件循环、切帧、半帧超时、帧头验/剥/加（`test/udsservice_test.cpp`） |
| `ONNXruntime` | ✅ 已实现：会话建立、`Run` 调用、超时取消（局部 `RunOptions` + 看护线程）、输入维读取（`test/onnxruntime_test.cpp`） |
| `FileService` / `Timer` | ✅ 已实现（`test/fileservice_test.cpp` / `test/timer_test.cpp`） |
| `CPU` / `NPU` | 待补：sysfs 节点读写、`/proc/stat` 差分、`governor` 切换 |
| `ContractFile` | 待补：契约 JSON 解析 |
| `serviceInterface::handleFrame` | 待补：四键外壳与 JSON 编解码、错误码映射 |

`make test` 需要 ONNX Runtime SDK（测试与 `schedd` 一样链接 `-lonnxruntime`），
镜像内未挂 SDK 时请先挂载到 `/opt/onnxruntime`：
`docker run -v <sdk目录>:/opt/onnxruntime ...`；运行时需 `LD_LIBRARY_PATH=/opt/onnxruntime/lib`。
