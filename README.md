# CPU+XPU 异构调度中间件（MiddleWare）

面向 openEuler / RK3588 的轻量化调度中间件原型。守护进程 `schedd` 通过 DQN 推理为
大模型推理任务动态分配 CPU/NPU 资源、批大小与频率档位；管理人员通过 `schedctl` 运维。

设计依据：《软件需求规格说明书》《软件体系结构设计文档》《软件详细设计文档》。
正本位于 `Reference/document/01_需求分析/` 与 `Reference/document/02_软件设计/`，
仓库内副本见 `software-design-documents/`。

## 目录结构

```
MiddleWare/
├── CMakeLists.txt          顶层构建（C++17，唯一真相）
├── Makefile                便捷包装：configure / build / test / clean / docker
├── Dockerfile              x86_64 开发+仿真镜像（openEuler 24.03-LTS）
├── cmake/                  FindONNXRuntime.cmake（可选依赖）
├── common/     sched_common   公共类型 / 共享内存快照 / 配置解析 / 极简 JSON
├── proto/      sched_proto    报文帧编解码（schedd 与 schedctl 共用同一份）
├── tool/       sched_tool     基础设施：SystemFS、INpu 及其实现
├── monitor/    sched_monitor  CPU/NPU 利用率与负载计算
├── system/     sched_system   System 子系统（状态监管层 + 硬件服务层）
├── agent/      sched_agent    Agent 子系统（调度决策层）
├── service/    sched_service  Service 子系统（服务接口层，UDS）
├── app/        schedd         守护进程主程序
├── cli/        schedctl       CLI 客户端
├── test/       b1_test        批次验收测试（按详细设计 §6.6.4 的批次组织）
├── deploy/     schedd.service / schedd.conf
└── model/      model_contract.json
```

## 依赖

| 依赖 | 必需 | 说明 |
|---|---|---|
| C++17 编译器（g++ ≥ 9） | 是 | openEuler 自带 / w64devkit |
| CMake ≥ 3.16 | 是 | 顶层构建 |
| pthread | 是 | CMake 自动查找 |
| ONNX Runtime | 否 | 缺失时 `DecisionMaker` 退化为 stub，仍可完整编译 |

**无第三方 C++ 库依赖**：JSON 由其自带的 `common::Json` 承担（约 470 行），未引入
nlohmann/json 一类外部库。

启用 ONNX Runtime：

```bash
tar xzf Reference/onnxruntime-linux-x64-1.30.0.tgz -C third_party/
cmake -B build -DONNXRUNTIME_ROOT=$PWD/third_party/onnxruntime-linux-x64-1.30.0
```

## 构建与运行

```bash
make                # = configure + build
make test           # ctest
make clean          # 清理 build/
make docker-shell   # 进入 openEuler 容器内构建（WSL 缺 cmake 时用）
make help           # 列出全部目标

# 等价的手工 CMake 流程
cmake -B build && cmake --build build

# 可选开关
cmake -B build -DSCHED_BUILD_TESTS=OFF   # 不构建测试
cmake -B build -DSCHED_ENABLE_ONNX=OFF   # 不启用 ONNX Runtime

./build/bin/schedd   --config deploy/schedd.conf
./build/bin/schedctl status
```

跨平台 NPU 切换：环境变量 `SCHED_NPU_MODE=real` 使用 `RealNpu`，否则使用
`VirtualNpu`（在 `$TMPDIR/virtual_npu/` 下模拟 sysfs，用于 x86 仿真）。

## 实现进度（详细设计 §6.6.4 的实现批次）

按依赖拓扑切成 8 个批次，逐批交付；每批必须自带断言，"编译过"不算完成。
批次划分、依赖边全表与逐批验收标准见《软件详细设计文档》§6.6。

| 批次 | 拓扑层 | 内容 | 状态 |
|---|---|---|---|
| **B1** | L0 | `common::Json`、`ConfigLoader`、`proto::RequestCodec`、`FeatureEncoder`、`ActionDecoder`、`OutputFormatter`、`SystemdCtl`（`cli::Command` 为纯抽象接口，无需实现） | ✅ 已完成（`b1_test` 124 项断言） |
| B2 | L1 | `OsMonitor`、`CtlClient`、`ServiceCommand`、`DecisionMaker` | 待实现 |
| B3 | L2 | `SystemManager` 三方法（`collect`/`applyAction`/`clampToNearest`）、`StatusCommand`、`ModeCommand` | 骨架就位 |
| B4 | L3–L4 | `SystemInterface`、`CtlMain`、`SystemMonitor` | — |
| B5 | L5 | `DecisionExecutor` | 半成品 |
| B6 | L6 | `SchedManager` | — |
| B7 | L7–L8 | `RequestDispatcher`、`ServiceInterface` | — |
| B8 | L9 | `ScheddApp` | — |

三个里程碑：

1. **参数可读、可写、可回读**（B2 完成，x86 仿真态）
2. **UDS 链路闭环**——`schedctl status` / `schedctl mode` 可用（B7 完成）
3. **整机可启动并跑通 UC1**——起 `schedd` → `schedctl mode auto` → 提交 `AskSched`
   → 收到 `ActionInfo`（B8 完成）

### 已完成组件

| 组件 | 状态说明 |
|---|---|
| `common::StateSnapshot` + 4 个自由函数 | seqlock 无锁快照；1 写 1 读 20 万轮并发撕裂检测通过（撕裂 0 次） |
| `common::SnapshotRegion` | Linux 走 `shm_open`+`mmap` → `/dev/shm/schedd_state`；Windows 自动退化为进程内对象 |
| `common::ConfigLoader` | INI 解析；剥 UTF-8 BOM；**失败不清表**；`getInt` 严格转换（非法值退回默认） |
| `common::Json` | 极简 JSON DOM（解析 + 序列化），零第三方依赖；失败不抛异常 |
| `tool::SystemFS` | 跨平台文件读写；`u8path` 加固（中文路径下 `ifstream` 走 CRT 窄接口会静默失败） |
| `tool::INpu` / `VirtualNpu` | HAL 抽象 + 工厂；虚拟设备在 `$TMPDIR/virtual_npu/` 下模拟 sysfs |
| `monitor::Monitor` | 无状态纯计算器（CPU 利用率换算 / NPU 三核取最大） |
| `proto::RequestCodec` | 长度前缀分帧（`len(4B) + type(4B) + payload`）；`schedd` 与 `schedctl` 共用同一份实现 |
| `agent::FeatureEncoder` / `ActionDecoder` | 契约驱动的归一化编码与 Q 值 → 设备动作解码 |
| `cli::OutputFormatter` / `SystemdCtl` | UTF-8 显示宽度感知的列对齐输出 / `systemctl` 封装 |

`tool::RealNpu` 需 12 月拿到 RK3588 后上板验证（接口不变，仅路径常量待核对）。

## 目标与依赖关系（详细设计 §5.5）

| 目标 | 类型 | 依赖 |
|---|---|---|
| `sched_common` | 静态库 | — |
| `sched_proto` | 静态库 | `sched_common` |
| `sched_tool` | 静态库 | `sched_common` |
| `sched_monitor` | 静态库 | `sched_tool` |
| `sched_system` | 静态库 | `sched_tool`、`sched_monitor`、`sched_common`（显式） |
| `sched_agent` | 静态库 | `sched_system`（+ ONNX Runtime 可选） |
| `sched_service` | 静态库 | `sched_agent`、`sched_proto` |
| `schedd` | 可执行 | `sched_service` |
| `schedctl` | 可执行 | `sched_common`、`sched_proto` |
| `monitor_test` / `system_test` / `agent_test` / `service_test` | 可执行 | 对应库 |
| `b1_test` | 可执行 | `sched_agent`、`sched_proto`、`sched_common` |

依赖方向严格单向、无环（环检测结论见详细设计 §6.6.2）。`sched_proto` 独立成薄库，
使 `schedctl` 复用协议实现时不会被动拖入 `agent`/`system`/ONNX Runtime 依赖链。

## 关键约定

- **契约冻结项**：`model_contract.json` 的 `input_dim`/`state_bounds`/`actions` 必须与
  Python 训练侧逐项一致；`FeatureEncoder.cpp` 的 4 个归一化标度常量
  （`1800000`/`1000000`/`8`/`2048`）与 `ActionDecoder.cpp` 的 2 张档位表同属冻结项——
  换 SoC 或调整训练采样区间时必须两侧同步，否则推理输入分布与训练分布错位，
  表现为"能跑但决策退化为噪声"。
- **失败不动出参**：`RequestCodec::decode`、`Json::parse`、`snapshotTryRead` 失败时
  一律不修改出参，调用方沿用旧值（与 `SystemFeature::stale` 语义一致）。
- **跨平台无需分支**：共享内存在 Windows 退化为进程内对象，`SnapshotRegion::open()`
  的返回值仅用于诊断日志，调用方不写失败分支。
- **测试可直达源文件**：`b1_test` 直接引用 `cli/src/OutputFormatter.cpp` 与
  `SystemdCtl.cpp`（`cli` 的类编在可执行目标内、未独立成静态库），为测试而改动
  既有构建结构不划算。

## 新增模块约定

每个模块固定三件套：`include/`（对外头文件）、`src/`（实现）、`CMakeLists.txt`
（`add_library` + `target_include_directories(PUBLIC include)`），并在顶层
`CMakeLists.txt` 的 `add_subdirectory` 列表中按依赖顺序登记；模块自带测试时加
`test/` 与 `add_executable` + `add_test`。

需要跨模块共用的新库参照 `proto/` 的写法：只依赖 `sched_common`，不向下游牵入
多余依赖。
