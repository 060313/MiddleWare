# CPU+XPU 异构调度中间件（MiddleWare）

面向 openEuler / RK3588 的轻量化调度中间件原型。守护进程 `schedd` 通过 DQN 推理为
大模型推理任务动态分配 CPU/NPU 资源、批大小与频率档位；管理人员通过 `schedctl` 运维。

设计依据：《软件需求规格说明书》《软件体系结构设计文档》《软件详细设计文档》
（见 `Reference/document/01_需求分析/` 与 `Reference/document/02_软件设计/`）。

## 目录结构

```
MiddleWare/
├── CMakeLists.txt          顶层构建（C++17，唯一真相）
├── Makefile                便捷包装：configure / build / test / clean / docker
├── Dockerfile              x86_64 开发+仿真镜像（openEuler 24.03-LTS）
├── cmake/                  FindONNXRuntime.cmake（可选依赖）
├── common/     sched_common   公共类型 / 共享内存快照 / 配置解析
├── tool/       sched_tool     基础设施：SystemFS、INpu 及其实现
├── monitor/    sched_monitor  CPU/NPU 利用率与负载计算
├── system/     sched_system   System 子系统（状态监管层 + 硬件服务层）
├── agent/      sched_agent    Agent 子系统（调度决策层）
├── service/    sched_service  Service 子系统（服务接口层，UDS）
├── app/        schedd         守护进程主程序
├── cli/        schedctl       CLI 客户端
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

启用 ONNX Runtime：

```bash
tar xzf Reference/onnxruntime-linux-x64-1.30.0.tgz -C third_party/
cmake -B build -DONNXRUNTIME_ROOT=$PWD/third_party/onnxruntime-linux-x64-1.30.0
```

## 构建与运行

```bash
make            # = configure + build
make test       # ctest
make clean      # 清理 build/
make docker-shell   # 进入 openEuler 容器内构建（WSL 无 cmake 时用）

./build/bin/schedd   --config deploy/schedd.conf
./build/bin/schedctl status
```

跨平台 NPU 切换：环境变量 `SCHED_NPU_MODE=real` 使用 `RealNpu`，否则使用
`VirtualNpu`（在 `$TMPDIR/virtual_npu/` 下模拟 sysfs，用于 x86 仿真）。

## 目标与依赖关系（详细设计 5.5）

| 目标 | 类型 | 依赖 |
|---|---|---|
| `sched_common` | 静态库 | — |
| `sched_tool` | 静态库 | `sched_common` |
| `sched_monitor` | 静态库 | `sched_tool` |
| `sched_system` | 静态库 | `sched_tool`、`sched_monitor` |
| `sched_agent` | 静态库 | `sched_system`（+ ONNX Runtime 可选） |
| `sched_service` | 静态库 | `sched_agent` |
| `schedd` | 可执行 | `sched_service` |
| `schedctl` | 可执行 | `sched_common` |
| `*_test` | 可执行 | 对应库 |

## 新增模块约定

每个模块固定三件套：`include/`（对外头文件）、`src/`（实现）、`CMakeLists.txt`
（`add_library` + `target_include_directories(PUBLIC include)`），并在顶层
`CMakeLists.txt` 的 `add_subdirectory` 列表中按依赖顺序登记。
