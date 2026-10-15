# 详细设计

## 1. 类设计

### 1.1 设计类清单

| 设计类 | 构造型 | 所属子系统 | 可见范围 |
|---|---|---|---|
| `serviceInterface` | boundary | Service | public |
| `UDSService` | common | common | public |
| `FileService` | common | common | public |
| `ONNXruntime` | common | common | public |
| `schedManager` | controller | Agent | public |
| `systemMonitor` | entity | Agent | private |
| `decisionMaker` | entity | Agent | private |
| `decisionExecutor` | entity | Agent | private |
| `systemInterface` | entity | System | public |
| `systemManager` | controller | System | private |
| `CPU` | boundary | System | private |
| `NPU` | boundary | System | private |

## 2. 用例设计

本节给出各用例在**设计类层**的实现方案：由哪些设计类协作、按什么顺序交互、以及前置 / 后置 / 异常流。用例分析阶段的用例模型见《软件需求规格说明书》，本节是其到设计类的落地。

### 2.1 UC1 请求推理调度（askSched）

**实现方案**：推理引擎经 UDS 提交任务特征。`UDSService` 完成报文接收与协议分帧后，交给 `serviceInterface`；`serviceInterface` 将 `taskAsk(TaskFeature)` 转发给 Agent 的 `schedManager`。`schedManager` 依次索取系统状态（展开为 UC2）、执行 DQN 推理、下发动作（展开为 UC3），最终回传 `ActionInfo`。

| 步 | 设计类协作 | 说明 |
|---|---|---|
| 1 | 推理引擎 → `UDSService` | 经 UDS 发起调度请求 |
| 2 | `UDSService` → `serviceInterface` | 协议分帧后转交服务入口 |
| 3 | `serviceInterface` → `schedManager::taskAsk(TaskFeature)` | 路由到调度控制器 |
| 4 | `schedManager` → `systemMonitor::getSystemFeature()` | 索取系统状态（展开为 UC2） |
| 5 | `systemMonitor` → `schedManager` | 返回 `SystemInfo` |
| 6 | `schedManager` → `decisionMaker::makeDecision(TaskFeature, SystemFeature)` | 经 `ONNXruntime` 执行模型推理（特征编码 → 推理 → 动作解码） |
| 7 | `decisionMaker` → `schedManager` | 返回 `Decision` |
| 8 | `schedManager` → `decisionExecutor::execDecision(Decision)` | 拆解为设备动作 |
| 9 | `decisionExecutor` → `schedManager` | 返回 `ActionInfo` |
| 10 | `schedManager` → `serviceInterface` | 回传结果 |
| 11 | `serviceInterface` → `UDSService` | 经 UDS 回写 |
| 12 | `UDSService` → 推理引擎 | 返回 `ActionInfo` |

后置条件：推理引擎收到与请求一一对应的 `ActionInfo`；Agent 内部模式状态不变。

用例设计顺序图：

![请求推理调度用例设计](images/请求推理调度用例设计.png)

### 2.2 UC2 采集系统状态（SystemFeature）

**实现方案**：`systemInterface` 向 `systemManager` 请求系统状态。`systemManager` 分别经 `NPU` 与 `CPU` 边界读取设备参数；`NPU`/`CPU` 通过 `FileService` 完成底层文件 I/O，将原始数据返回给 `systemManager` 后，由 `systemManager` 换算、组装为 `SystemInfo` 返回。

| 步 | 设计类协作 | 说明 |
|---|---|---|
| 1 | `systemInterface` → `systemManager::askSystemInfo()` | 请求系统状态 |
| 2 | `systemManager` → `NPU::getNPUInfo()` | 读取 NPU 参数 |
| 3 | `NPU` → `FileService` | 经文件服务读 NPU 设备节点 |
| 4 | `FileService` → `NPU` | 返回 `NPUInfo` |
| 5 | `NPU` → `systemManager` | 返回 `NPUInfo` |
| 6 | `systemManager` → `CPU::getCPUInfo()` | 读取 CPU 参数 |
| 7 | `CPU` → `FileService` | 经文件服务读 CPU 设备节点 |
| 8 | `FileService` → `CPU` | 返回 `CPUInfo` |
| 9 | `CPU` → `systemManager` | 返回 `CPUInfo` |
| 10 | `systemManager` → `systemInterface` | 返回 `SystemInfo` |

异常流：设备文件不可读 → 返回最近一次有效快照并标记为过期（stale），不阻塞调度。

用例设计顺序图：

![采集系统状态用例设计](images/采集系统状态用例设计.png)

### 2.3 UC3 调整处理器频率（changeFreq）

**实现方案**：`systemInterface` 向 `systemManager` 发起频率设置请求。`systemManager` 经 `CPU` 边界调用 `FileService` 写入频率节点，并回读校验；结果以 `ResultInfo` 逐级返回。NPU 调频与此流程对称，由 `NPU` 边界经 `FileService` 写入 NPU devfreq 节点。

| 步 | 设计类协作 | 说明 |
|---|---|---|
| 1 | `systemInterface` → `systemManager::askSet(Freq)` | 请求设置频率 |
| 2 | `systemManager` → `CPU::setFreq(Freq)` | 转发到 CPU 边界 |
| 3 | `CPU` → `FileService` | 系统调用：写入频率节点 |
| 4 | `FileService` → `CPU` | 返回 `ResultInfo` |
| 5 | `CPU` → `systemManager` | 返回 `ResultInfo` |
| 6 | `systemManager` → `systemInterface` | 返回 `ResultInfo` |

说明：NPU 调频与此流程对称，由 `NPU::setFrequency` 经 `FileService` 写入 NPU devfreq 节点。

异常流：频率越界 → 夹取到最近合法档位并标记为已调整（adjusted）；写入失败 → 返回失败的 `ResultInfo`。**单台设备失败不中断动作组内其余动作**（`decisionExecutor` 仍走完整个动作组，保证逐设备结果完整、且与动作顺序一一对应），整条请求的成败由 `ActionInfo` 汇总（各结果的逻辑与）统一给出，是否降级由 `schedManager` 判定。

用例设计顺序图：

![设置处理器频率用例设计](images/设置处理器频率用例设计.png)

### 2.4 UC4 / UC5 / UC6 管理类用例

| 用例 | 实现方案 |
|---|---|
| UC4 查询系统状态 | 管理人员在 `schedctl` 发起查询，经 `UDSService`（UDS）发 `GET_STATE` → `serviceInterface` → `schedManager` → `systemMonitor::getlastInfo()` → 原路返回，由界面侧格式化输出 |
| UC5 启停服务 | 管理人员在 `schedctl` 发起启停，由界面侧调用 `systemctl <动作> schedd`；守护进程生命周期由 systemd 管理，不进入 `schedd` 内部 |
| UC6 选择模型 | 管理人员在 `schedctl` 发起选择，经 `UDSService` 发 `SetModel` → `serviceInterface` → `schedManager::setModel()`；模型非法或不可用时返回错误 |


## 3. 类设计

本章逐类整理 12 个设计类的**属性**与**方法**：每个类一张表，第一列标识行性质（`类` / `属性` / `方法`，同类行纵向合并），之后依序罗列该类全部属性、全部方法。属性行给出名称、类型、可见性、初值；方法行按七要素给出名称、参数、返回、作用范围、前置、后置、**功能说明**。方法的具体执行过程另见各类表后的「**算法步骤**」小节。

### 3.1 `serviceInterface`（boundary · Service · public）

| 类 / 属性 / 方法 | 名称 | 类型 / 参数 | 可见性 / 返回 | 初值 / 前置 | — / 后置 | — / 功能说明 |
|---|---|---|---|---|---|---|
| 类 | `serviceInterface` | 门面类 · Service 子系统 | public | — | — | — |
| 属性 | `schedManager` | `schedManager*` | private | 初值 `nullptr` | — | 关联，构造装配注入 |
| 属性 | `running` | `bool` | private | 初值 `false` | — | 服务运行标志 |
| 属性 | `socketPath` | `std::string` | private | 初值 `""` | — | **全系统单存于此**，调 `UDSService` 时传参 |
| 方法 | `taskAsk` | `const TaskRequest&` | public → `TaskResult` | 服务已启动、`schedManager` 非空 | 返回一次推理结果，不改自身状态 | 受理 → 转交 `schedManager` → 返回其产出 |
| 方法 | `getSystemState` | 无 | public → `SystemFeaturedState` | 同上 | 返回快照，无副作用 | 转交 `schedManager`（其经 `systemMonitor` 取特征） |
| 方法 | `setModel` | `const std::string&`（模型名） | public → `bool` | 服务已启动；模型名在白名单内 | 成功则所选模型更新 | 转交 `schedManager`，由其后置校验并热切换 |
| 方法 | `start` | 无 | public → `bool` | `running == false`、`socketPath` 非空 | 成功则 `running = true`，UDS 服务就绪 | 装配 `UDSService`（传 `socketPath`）→ 监听 → 置标志 |
| 方法 | `stop` | 无 | public → `void` | `running == true` | `running = false`，端点与连接全释放 | 置标志 → 关闭端点与连接 → 唤醒服务循环 |

> **交互风格裁决**：`taskAsk` / `getSystemState` 采用**同步**语义——方法阻塞至结果就绪后直接返回，与用例设计顺序图"一问一答"一致。

**算法步骤**

**`serviceInterface::taskAsk(request)`**
- S1. 前置校验：`running == true` 且内部 `schedManager*` 非空；任一不满足 → 置结果 `ok=false`、`message="service not running"`，返回（不抛异常）。
- S2. 调用 `schedManager::makeDecision(request)`，**同步阻塞**至返回。
- S3. 原样返回其产出 `TaskResult`；本方法**不改动自身任何属性**（无状态）。

**`serviceInterface::getSystemState()`**
- S1. 前置校验：`running == true` 且 `schedManager*` 非空；不满足 → 返回零值 `SystemFeaturedState`（`ok=false`）。
- S2. 调用 `schedManager::getFeature()`；**同步阻塞**。
- S3. 返回 `SystemFeaturedState`（已含当前模型名），无副作用。

**`serviceInterface::setModel(modelName)`**
- S1. 前置校验：`running == true` 且 `schedManager*` 非空；不满足 → 返回 `false`。
- S2. 调用 `schedManager::setModel(modelName)`（**白名单校验与热切换均在彼侧完成**，本方法不重复校验）。
- S3. 返回其布尔结果；成功与否**本方法不改自身状态**。

**`serviceInterface::start()`**
- S1. 幂等保护：若 `running == true` → 直接返回 `true`（重复启动视为成功）。
- S2. 校验 `socketPath` 非空；为空 → 返回 `false`（无法确定监听地址）。
- S3. 装配 `UDSService`：以 `socketPath` 与 `this` 的报文处理入口构造注入。
- S4. 调 `UDSService::start(socketPath)` 建立监听；失败 → 返回 `false`（`running` 保持 `false`）。
- S5. 置 `running = true`；返回 `true`。

**`serviceInterface::stop()`**
- S1. 幂等保护：若 `running == false` → 直接返回（无事可做）。
- S2. 先置 `running = false`（**先置标志再拆资源**，避免新请求进入）。
- S3. 调 `UDSService::stop()` 关闭监听端点与全部已接受连接（内部唤醒阻塞的服务循环）。
- S4. 释放 `UDSService` 实例；返回。

### 3.2 `UDSService`（common · common · public）

| 类 / 属性 / 方法 | 名称 | 类型 / 参数 | 可见性 / 返回 | 初值 / 前置 | — / 后置 | — / 功能说明 |
|---|---|---|---|---|---|---|
| 类 | `UDSService` | 公共基础设施 · common 子系统 | public | — | — | — |
| 属性 | `listener` | `Socket` | private | 初值无效端点 | — | 监听端点；`start()` 创建、`stop()` 关闭 |
| 属性 | `conns` | `std::vector<Socket>` | private | 初值空 | — | 已接受连接集合，持有至对端断开 |
| 属性 | `running` | `bool` | private | 初值 `false` | — | 收发循环运行标志 |
| 属性 | `handler` | `Handler` | private | 初值空 | — | 报文处理回调（构造注入，由 `serviceInterface` 提供） |
| 方法 | `start` | `const std::string&`（套接字路径） | public → `bool` | `running == false`、路径非空 | 建立 `listener`、`running = true` | 建监听端点 → 绑定 / 监听 → 置标志 |
| 方法 | `stop` | 无 | public → `void` | `running == true` | 关闭 `listener` 与全部 `conns`、`running = false` | 置标志 → 逐个关闭连接 → 唤醒阻塞的服务循环 |
| 方法 | `route` | `const std::string&`（请求帧） | public → `std::string`（应答帧） | `running == true`、`handler` 非空 | 无状态改变 | 解析帧 → 调 `handler` → 组应答帧；失败回可读失败体而非断连 |

> **回调归属裁决**：报文处理回调以**构造注入**方式存入 `handler` 属性（由 `serviceInterface` 装配时提供），不新增类间关系、不改设计类图。

**算法步骤**

**`UDSService::start(socketPath)`**
- S1. 幂等保护：`running == true` → 返回 `true`。
- S2. 校验 `socketPath` 非空；为空 → 返回 `false`。
- S3. 创建监听套接字（`AF_UNIX` / `SOCK_STREAM`）；失败 → 返回 `false`。
- S4. 若路径上已有残留套接字文件，先 `unlink` 再绑定（避免 `EADDRINUSE`）。
- S5. `bind(socketPath)` → `listen(backlog)`；任一步失败 → 关闭套接字、返回 `false`。
- S6. 将套接字存入 `listener`，置 `running = true`；返回 `true`。

**`UDSService::stop()`**
- S1. 幂等保护：`running == false` → 直接返回。
- S2. 置 `running = false`（先置标志，令收发循环退出）。
- S3. 对 `conns` 中**每个**连接逐个 `close()`（遍历，不受单个关闭失败影响），清空 `conns`。
- S4. 关闭 `listener`；如实现使用阻塞等待循环，则通过 `eventfd`/自管道**唤醒**之，避免 `stop()` 阻塞。
- S5. 从文件系统移除套接字文件（`unlink`），返回。

**`UDSService::route(requestFrame)`**
- S1. 前置校验：`running == true` 且 `handler` 非空；不满足 → 返回可读失败帧（**不断连**）。
- S2. 解析 `requestFrame`（报文类型 + 载荷）；解析失败 → 返回 `{"ok":false,"message":"bad frame"}` 帧，**连接保持**。
- S3. 按报文类型分派：`GET_STATE` → `handler(GetState)`；`SetModel` → `handler(SetModel, 载荷)`；未知类型 → 返回失败帧。
- S4. 调 `handler(...)` 取得业务侧结果（同步）。
- S5. 将结果编码为应答帧（成功体或可读失败体）返回；**失败一律回帧，不主动断开连接**（只有帧级故障才由上层断连）。

> **裁决依据**：S2/S5 的"失败回帧不断连"对应《软件需求规格说明书》中"请求被拒绝 ≠ 连接失败"的约定，保证客户端能读到结构化错误而非空连接。

### 3.3 `FileService`（common · common · public）

| 类 / 属性 / 方法 | 名称 | 类型 / 参数 | 可见性 / 返回 | 初值 / 前置 | — / 后置 | — / 功能说明 |
|---|---|---|---|---|---|---|
| 类 | `FileService` | 公共基础设施 · common 子系统 | public | — | — | — |
| 属性 | — | — | — | — | — | **无设计层属性**（无状态工具类） |
| 方法 | `read` | `const std::string&`（路径） | public → `std::string` | 路径非空 | 无副作用 | 打开 → 读全文 → 返回；**失败回空串**，不抛异常 |
| 方法 | `write` | `const std::string&`（路径）, `const std::string&`（内容） | public → `bool` | 路径非空 | 文件内容已更新 | 打开可写 → 写入 → 返回结果 |
| 方法 | `exists` | `const std::string&`（路径） | public → `bool` | — | 无副作用 | 探测路径可访问性 → 返回布尔 |

**算法步骤**

**`FileService::read(path)`**
- S1. 校验 `path` 非空；为空 → 返回空串 `""`。
- S2. 以只读方式打开文件；打开失败（不存在 / 无权限）→ 返回空串，**不抛异常**。
- S3. 定位文件末尾取长度 → 一次性读全文字节。
- S4. 关闭文件句柄（RAII 或显式关闭，保证异常路径也能释放）。
- S5. 按原样返回内容字符串；**不 trim、不做编码转换**。

**`FileService::write(path, content)`**
- S1. 校验 `path` 非空；为空 → 返回 `false`。
- S2. 以覆盖写方式打开文件（`truncate` 语义）；打开失败 → 返回 `false`。
- S3. 写入 `content` 全部字节，检查写入长度是否等于内容长度。
  - S3.1 若短写 → 视为失败（继续尝试与否由实现定，但返回值须为 `false`）。
- S4. 关闭句柄；返回写入是否完整成功。

**`FileService::exists(path)`**
- S1. 用可访问性探测（如 `access(path, F_OK)` 或 `stat`）判断路径是否存在。
- S2. 返回布尔结果；调用失败（如中间目录不可达）一律按"不存在"处理，返回 `false`。

### 3.4 `ONNXruntime`（common · common · public）

| 类 / 属性 / 方法 | 名称 | 类型 / 参数 | 可见性 / 返回 | 初值 / 前置 | — / 后置 | — / 功能说明 |
|---|---|---|---|---|---|---|
| 类 | `ONNXruntime` | 公共基础设施 · common 子系统 | public | — | — | — |
| 属性 | `session` | `Session` | private | 初值空会话 | — | 推理会话；`load()` 建立、`infer()` 反复使用 |
| 方法 | `load` | `const std::string&`（模型路径） | public → `bool` | 路径非空且模型文件可读 | `session` 建立 | 读模型 → 建推理会话 → 存 `session` |
| 方法 | `infer` | `const std::vector<float>&`（特征向量） | public → `std::vector<float>`（动作值） | `session` 有效 | 无副作用 | 输入张量 → 执行会话 → 取出输出张量转列表返回 |

> **路径传参对照**：`ONNXruntime` 不持模型路径属性，故 `load` 以**参数**接收路径；`decisionMaker` 以**属性** `modelPath` 持路径、其 `load` 无参。

**算法步骤**

**`ONNXruntime::load(modelPath)`**
- S1. 校验 `modelPath` 非空且文件可读；不满足 → 返回 `false`。
- S2. 创建 ONNX Runtime 环境与推理会话（`CreateSession`）；创建失败 → 保留旧 `session` 不变，返回 `false`。
- S3. 会话创建成功 → 将新会话存入 `session`（**替换语义**：失败则旧会话不动，成功则整体替换）。
- S4. 返回 `true`。

**`ONNXruntime::infer(features)`**
- S1. 前置校验：`session` 有效；无效 → 返回空 `std::vector<float>`（调用方据空判失败）。
- S2. 将 `features` 装入输入张量（形状按模型输入维度；维度不符 → 返回空列表）。
- S3. 执行会话推理（`Run`）；执行失败 → 返回空列表，不抛异常。
- S4. 取出**首个输出张量**，按行展开为 `std::vector<float>` 返回（动作值列表）。

> **失败语义**：`infer` 一律以"空列表"表达失败，避免在热路径上使用异常；调用方（`decisionMaker`）以"空列表"判定本次推理失败并计入失败计数。

### 3.5 `schedManager`（controller · Agent · public）

| 类 / 属性 / 方法 | 名称 | 类型 / 参数 | 可见性 / 返回 | 初值 / 前置 | — / 后置 | — / 功能说明 |
|---|---|---|---|---|---|---|
| 类 | `schedManager` | 总控类 · Agent 子系统 | public | — | — | — |
| 属性 | `monitor` | `systemMonitor*` | private | 初值 `nullptr` | — | 关联，构造注入 |
| 属性 | `maker` | `decisionMaker*` | private | 初值 `nullptr` | — | 关联，构造注入 |
| 属性 | `executor` | `decisionExecutor*` | private | 初值 `nullptr` | — | 关联，构造注入 |
| 属性 | `model` | `std::string` | private | 初值 `dqn` | — | 当前所选模型名；**单一事实源**；`static` 仅作标识、不触发分支 |
| 属性 | `failCount` | `int` | private | 初值 `0` | — | 成功清零，达阈值 N 自动降级 `static` |
| 方法 | `makeDecision` | `const TaskRequest&` | public → `TaskResult` | 三引用已装配 | 返回结果；`failCount` 按成败清零或递增 | 取特征 → 模型判断 → 失败则 `handleDecideFailure()` → `execDecision()` → 组装返回 |
| 方法 | `getFeature` | 无 | public → `SystemFeaturedState` | `monitor` 已装配 | 无副作用 | `monitor->getFeature()` + 附当前 `model` 返回 |
| 方法 | `getModel` | 无 | public → `std::string` | — | 无副作用 | 直接读 `model` |
| 方法 | `setModel` | `const std::string&`（模型名） | public → `bool` | 取值在白名单内；请求非 `static` 时 `decisionMaker` 须就绪 | `model` 更新（**先建后换**原子切换） | 白名单校验 → 就绪性判断 → 新建会话加载新模型 → 成功才原子替换、失败丢弃新会话 |
| 方法 | `handleDecideFailure` | 无 | private → `void` | 一次推理刚失败 | `failCount` 加一，达阈值 N 触发兜底 | `++failCount`；达 N 则兜底并重置计数 |

**算法步骤**

**`schedManager::makeDecision(request)`**
- S1. 调用 `monitor->getFeature()` 取当前系统特征 `SystemFeature`（含采样，失败时回兜底值，故本步不返回失败）。
- S2. **模型判断**：若 `model == "static"`（不指向任何模型）→ 构造"静态模式"结果（不下发推理），跳到 S7。
- S3. 检查 `maker->isReady()`；未就绪 → 调 `handleDecideFailure()`，构造失败结果，跳到 S7。
- S4. 调 `maker->decide(feature)` 取得 `Decision`；返回**空动作集**（推理失败）→ 调 `handleDecideFailure()`，构造失败结果，跳到 S7。
- S5. 推理成功 → **重置失败计数** `failCount = 0`。
- S6. 调 `executor->execDecision(decision)` 下发动作，取得 `std::vector<ExecResult>`（**结果与动作严格对位、大小恒等**）。
- S7. 组装 `TaskResult`（含 `request.taskId`、动作/结果、`ok` 标志与 `message`），返回。

**`schedManager::getFeature()`**
- S1. 调 `monitor->getFeature()` 取 `SystemFeature`（内部先 `collect` 采样）。
- S2. 附上当前 `model` 字段，组装 `SystemFeaturedState` 返回；**无副作用**。

**`schedManager::getModel()`**
- S1. 直接返回 `model` 成员副本；无副作用（供界面侧 `get` 读取）。

**`schedManager::setModel(modelName)`**
- S1. **白名单校验**：`modelName` 不在白名单（如 `dqn` / `static` 及合法模型名集合）内 → 返回 `false`，**`model` 保持不变**。
- S2. **就绪性判断**：若 `modelName != "static"`，则要求 `maker->isReady()`（或新模型可加载）；不可用 → 返回 `false`，`model` 保持不变。
- S3. **先建后换（原子切换）**：先新建会话加载 `modelName` 对应模型；
  - S3.1 加载失败 → **丢弃新会话**，旧模型与 `model` 原封不动，返回 `false`；
  - S3.2 加载成功 → 才把 `model` 原子更新为 `modelName`（**无回滚逻辑，因为旧状态从未被破坏**）。
- S4. 若 `modelName == "static"`：仅置标识、不加载模型（`static` 不触发分支以外的特殊逻辑）。
- S5. 返回 `true`。

**`schedManager::handleDecideFailure()`**
- S1. `++failCount`（连续推理失败计数加一）。
- S2. 判断 `failCount >= N`（阈值 N，如 3）：
  - S2.1 未达阈值 → 直接返回，**保持当前模型**等待下次重试；
  - S2.2 达到阈值 → 执行兜底：将 `model` 置为 `"static"`（自动降级，停用模型推理），并**重置 `failCount = 0`**。
- S3. 返回（回 `auto`/正常模型须**人工**再次 `setModel`，不自动恢复）。

> **降级语义**：连续 3 次推理失败自动降级 `static`，成功一次即清零计数。计数清零只发生在 `makeDecision` S5（成功路径）与 `handleDecideFailure` S2.2（降级时）。

### 3.6 `systemMonitor`（entity · Agent · private）

| 类 / 属性 / 方法 | 名称 | 类型 / 参数 | 可见性 / 返回 | 初值 / 前置 | — / 后置 | — / 功能说明 |
|---|---|---|---|---|---|---|
| 类 | `systemMonitor` | 监控门面 · Agent 子系统 | private | — | — | — |
| 属性 | `sys` | `systemInterface*` | private | 初值 `nullptr` | — | 关联，构造注入 |
| 属性 | `lastInfo` | `SystemInfo` | private | 初值零值快照 | — | 最近一次有效 `SystemInfo`，采集失败作 stale 兜底 |
| 方法 | `getFeature` | 无 | 同子系统 → `SystemFeature` | `sys` 已装配 | 无副作用（采样副作用落在 `collect`） | **内部先调 `collect`**，再据 `lastInfo` 返回；失败回兜底值 |
| 方法 | `collect` | 无 | private → `bool` | `sys` 已装配 | 成功更新 `lastInfo`；失败保持不变 | `sys->getInfo()` → 有效性判断 → 算特征 → 写 `lastInfo` |

**算法步骤**

**`systemMonitor::getFeature()`**
- S1. 调 `collect()` 尝试刷新 `lastInfo`（**采样副作用集中于此**）。
- S2. 无论 `collect` 成败，均据当前 `lastInfo`（成功则为新值，失败则为上次有效值）计算 `SystemFeature`。
- S3. 若 `lastInfo` 有效性不足（如从未采集成功）→ 回**兜底特征值**返回，保证本方法**永不失败**。
- S4. 返回 `SystemFeature`；本方法自身无副作用。

**`systemMonitor::collect()`**
- S1. 前置校验：`sys` 非空；为空 → 返回 `false`。
- S2. 调 `sys->getInfo()` 取 `SystemInfo`（分别读 CPU/NPU）。
- S3. **有效性判断**：若取得的样本非法（如频率读数为 0、字段缺失）→ 返回 `false`，**`lastInfo` 保持不变**。
- S4. 由有效样本计算特征量（利用率、频率档位等）。
- S5. 以新值**整体写入** `lastInfo`（覆盖式更新）；返回 `true`。

### 3.7 `decisionMaker`（entity · Agent · private）

| 类 / 属性 / 方法 | 名称 | 类型 / 参数 | 可见性 / 返回 | 初值 / 前置 | — / 后置 | — / 功能说明 |
|---|---|---|---|---|---|---|
| 类 | `decisionMaker` | 推理器 · Agent 子系统 | private | — | — | — |
| 属性 | `modelPath` | `std::string` | private | 初值 `""` | — | ONNX 模型路径（配置注入）；空串表示未就绪 |
| 方法 | `isReady` | 无 | 同子系统 → `bool` | — | 无副作用 | 判断模型是否已成功加载 |
| 方法 | `load` | 无（路径取自 `modelPath`） | 同子系统 → `bool` | `modelPath` 非空 | 建会话、`isReady()` 转真；失败保持未就绪 | 路径非空校验 → 经 `ONNXruntime` 建会话 → 置就绪标志 |
| 方法 | `decide` | `const SystemFeature&` | 同子系统 → `Decision` | `isReady() == true` | 无副作用 | 特征编码 → `ONNXruntime` 推理 → 动作解码 → 返回 |

**算法步骤**

**`decisionMaker::isReady()`**
- S1. 判断 `modelPath` 非空**且**底层 `ONNXruntime` 会话已成功建立（本类内维护就绪标志）。
- S2. 返回该布尔判断；**无副作用**。

**`decisionMaker::load()`**
- S1. 路径校验：`modelPath` 非空；为空 → 保持未就绪，返回 `false`。
- S2. 经 `ONNXruntime::load(modelPath)` 建立推理会话。
- S3. 会话建立成功 → 置就绪标志为真，返回 `true`；失败 → **保持未就绪**（旧状态不变），返回 `false`。

**`decisionMaker::decide(feature)`**
- S1. 前置校验：`isReady() == true`；不就绪 → 返回空 `Decision`（空动作集，表示推理失败）。
- S2. **特征编码**：将 `SystemFeature` 展平/归一化为 `std::vector<float>`（按模型输入维度）。
- S3. 调 `ONNXruntime::infer(encoded)` 执行推理。
  - S3.1 返回空列表（推理失败）→ 返回空 `Decision`。
- S4. **动作解码**：对输出动作值取 `argmax`（或按契约解码），映射为 `Decision`（含设备动作、批大小等）。
- S5. 返回 `Decision`；无副作用。

### 3.8 `decisionExecutor`（entity · Agent · private）

| 类 / 属性 / 方法 | 名称 | 类型 / 参数 | 可见性 / 返回 | 初值 / 前置 | — / 后置 | — / 功能说明 |
|---|---|---|---|---|---|---|
| 类 | `decisionExecutor` | 执行器 · Agent 子系统 | private | — | — | — |
| 属性 | — | — | — | — | — | **无设计层属性**（纯执行器；设备访问通道由实现层提供） |
| 方法 | `execDecision` | `const Decision&` | 同子系统 → `std::vector<ExecResult>` | 设备访问通道可用（实现层提供） | **结果与动作严格对位、大小恒等**；单设备失败不短路 | 遍历动作 → 逐设备施加（调频 / 批大小）→ 收集结果，不因单个失败中断 |

> **通道归属裁决**：设备访问通道**不在设计层表达**（不新增属性、不新增类间关系），由实现层提供。

**算法步骤**

**`decisionExecutor::execDecision(decision)`**
- S1. 初始化结果容器 `results`，**预留与动作数等长**的空间（保证对位）。
- S2. **遍历** `decision` 中的每个 `DeviceAction`（下标 `i` 从 0 到 n-1）：
  - S2.1 解析动作目标设备（CPU / NPU）与目标值（频率 / 批大小）。
  - S2.2 **施加动作**：经实现层提供的设备访问通道下发（如经 `CPU::apply` / `NPU::apply`）。
  - S2.3 收集该动作的 `ExecResult`（含成功标志与错误信息）写入 `results[i]`。
  - S2.4 **单设备失败不短路**：即使 `results[i]` 失败，**继续处理下一个动作**，不提前退出循环。
- S3. 循环结束，返回 `results`——**其大小恒等于动作数，且第 i 项严格对应第 i 个动作**。

> **对位要求**：调用方（`schedManager::makeDecision` S6）据"`results` 与动作严格对位、大小恒等"的约定解析结果，因此本方法**不得**跳过或过滤任何动作，失败也须占位返回。

### 3.9 `systemInterface`（entity · System · public）

| 类 / 属性 / 方法 | 名称 | 类型 / 参数 | 可见性 / 返回 | 初值 / 前置 | — / 后置 | — / 功能说明 |
|---|---|---|---|---|---|---|
| 类 | `systemInterface` | 门面类 · System 子系统 | public | — | — | — |
| 属性 | `mgr` | `systemManager*` | private | 初值 `nullptr` | — | 关联，构造注入 |
| 方法 | `getInfo` | 无 | public → `SystemInfo` | `mgr` 已装配 | 无副作用 | 转交 `mgr->getInfo()`，汇聚设备状态返回 |
| 方法 | `apply` | `const DeviceAction&` | public → `bool` | `mgr` 已装配；`action` 设备存在 | 设备状态改变 | 转交 `mgr->apply(action)`，由其路由到具体设备 |

**算法步骤**

**`systemInterface::getInfo()`**
- S1. 前置校验：`mgr` 非空；为空 → 返回零值 `SystemInfo`。
- S2. 转交 `mgr->getInfo()`，由其汇聚 CPU/NPU 设备状态。
- S3. 原样返回汇聚结果；**本方法只做转发，不做加工**（门面零逻辑透传）。

**`systemInterface::apply(action)`**
- S1. 前置校验：`mgr` 非空；为空 → 返回 `false`。
- S2. 转交 `mgr->apply(action)`，由其按目标设备类型路由到 `CPU` 或 `NPU`。
- S3. 返回布尔结果；本方法不改自身状态。

### 3.10 `systemManager`（controller · System · private）

| 类 / 属性 / 方法 | 名称 | 类型 / 参数 | 可见性 / 返回 | 初值 / 前置 | — / 后置 | — / 功能说明 |
|---|---|---|---|---|---|---|
| 类 | `systemManager` | 管理器 · System 子系统 | private | — | — | — |
| 属性 | `cpu` | `CPU*` | private | 初值 `nullptr` | — | **组合**（生存期由本类负责） |
| 属性 | `npu` | `NPU*` | private | 初值 `nullptr` | — | **组合**（生存期由本类负责） |
| 属性 | `sampleIntervalMs` | `int` | private | 初值 `500` | — | 采样间隔（毫秒），驱动后台采集 |
| 方法 | `getInfo` | 无 | 同子系统 → `SystemInfo` | `cpu`、`npu` 已创建（构造装配） | 无副作用 | 分别调 `cpu->getInfo()`、`npu->getInfo()` → 汇聚返回 |
| 方法 | `apply` | `const DeviceAction&` | 同子系统 → `bool` | `action` 目标设备已创建 | 目标设备状态改变 | 按目标设备类型路由 → `cpu->apply()` 或 `npu->apply()` |

**算法步骤**

**`systemManager::getInfo()`**
- S1. 前置校验：`cpu`、`npu` 均非空（构造装配保证）；任一为空 → 返回已取得的部分信息并不崩溃。
- S2. 调 `cpu->getInfo()` 取得 CPU 的 `DeviceInfo`。
- S3. 调 `npu->getInfo()` 取得 NPU 的 `DeviceInfo`。
- S4. **汇聚**两项为 `SystemInfo` 返回；无副作用。

**`systemManager::apply(action)`**
- S1. 读取 `action` 的目标设备标识。
- S2. **按设备类型路由**：
  - S2.1 目标为 CPU → 调 `cpu->apply(action)`；
  - S2.2 目标为 NPU → 调 `npu->apply(action)`；
  - S2.3 未知设备类型 → 返回 `false`（不误投到具体设备）。
- S3. 返回路由目标设备的执行结果。

### 3.11 `CPU`（boundary · System · private）

| 类 / 属性 / 方法 | 名称 | 类型 / 参数 | 可见性 / 返回 | 初值 / 前置 | — / 后置 | — / 功能说明 |
|---|---|---|---|---|---|---|
| 类 | `CPU` | 设备边界类 · System 子系统 | private | — | — | — |
| 属性 | `devPath` | `std::string` | private | 构造注入 | — | 频率设备节点路径；默认即生产路径，测试可注入伪 sysfs |
| 属性 | `procStatPath` | `std::string` | private | 构造注入 | — | 状态文件路径（`/proc/stat`） |
| 属性 | `freqTable` | `std::vector<int>` | private | 初值空 | — | 可用频率表（Hz），首次采集后记住 |
| 属性 | `curFreq` | `int` | private | 初值 `0` | — | 当前频率（Hz） |
| 属性 | `prevTotal` / `prevIdle` | `uint64` | private | 初值 `0` | — | 上次采样的累计 jiffies（差分基准） |
| 属性 | `hasPrevSample` | `bool` | private | 初值 `false` | — | 是否已有差分基准 |
| 方法 | `getInfo` | 无 | 同子系统 → `DeviceInfo` | `devPath` 已注入 | 无副作用 | 确保频率表已读 → 读状态文件原始累计值 + 读当前频率 → 组装返回 |
| 方法 | `apply` | `const DeviceAction&` | 同子系统 → `bool` | `devPath` 已注入；目标频率在可用范围内 | `curFreq` 更新；失败保持原值 | 校验目标频率 ∈ `freqTable` → `setFreq()` → 回读更新 `curFreq` |
| 方法 | `readFreqTable` | 无 | private → `bool` | `devPath` 已注入 | 填充 `freqTable` | 读可用频率节点 → 解析列表 → 写入 `freqTable` |
| 方法 | `setFreq` | `int`（Hz） | private → `bool` | `freqHz` 已在 `freqTable` 内 | 频率节点已写入 | 经 `FileService` 写频率节点 → 返回写入结果 |

**算法步骤**

**`CPU::getInfo()`**
- S1. 若 `freqTable` 为空 → 调 `readFreqTable()` 确保频率表已读；失败不阻断（`freqTable` 保持空，后续校验会兜底）。
- S2. 读 `/proc/stat` 首行（`cpu` 汇总行）得累计 `total` 与 `idle` jiffies。
- S3. **利用率差分计算**：
  - S3.1 若 `hasPrevSample == true`：`dTotal = total - prevTotal`、`dIdle = idle - prevIdle`；
  - S3.2 `util = (dTotal > 0) ? (1 - dIdle / dTotal) : 0`（除零保护）；
  - S3.3 若 `hasPrevSample == false`（首次）→ `util = 0`（无基准无法算差分）。
- S4. 更新差分基准：`prevTotal = total`、`prevIdle = idle`、`hasPrevSample = true`。
- S5. 读当前频率节点得 `curFreq`。
- S6. 组装 `DeviceInfo`（利用率、当前频率、频率表）返回；本方法自身不改变对外可见状态（基准更新属内部采样所需）。

**`CPU::apply(action)`**
- S1. 校验目标频率 `freqHz ∈ freqTable`；不在表内 → 返回 `false`，`curFreq` 保持原值。
- S2. 调 `setFreq(freqHz)` 写入频率节点；失败 → 返回 `false`，`curFreq` 保持原值。
- S3. **回读**频率节点确认生效，更新 `curFreq`；返回 `true`。

**`CPU::readFreqTable()`**
- S1. 经 `FileService::read` 读可用频率节点；读失败或为空 → 返回 `false`。
- S2. 解析节点内容（空白/逗号分隔的频率列表）为整数数组。
- S3. 写入 `freqTable`；返回 `true`。

**`CPU::setFreq(freqHz)`**
- S1. 前置校验：`freqHz` 已在 `freqTable` 内；否则返回 `false`（不允许设表外频率）。
- S2. 经 `FileService::write` 将 `freqHz` 写入频率节点。
- S3. 返回写入是否成功。

> **实现层提示（已识别未解决项）**：CPU 侧频率节点名须用 **cpufreq** 语义（`scaling_available_frequencies` / `scaling_cur_freq` / `scaling_setspeed`），**不可沿用 devfreq 命名**；`scaling_setspeed` 直接位于 policy 目录下，无 `userspace/` 子目录。NPU 侧为 devfreq 挂点，命名不同。

### 3.12 `NPU`（boundary · System · private）

| 类 / 属性 / 方法 | 名称 | 类型 / 参数 | 可见性 / 返回 | 初值 / 前置 | — / 后置 | — / 功能说明 |
|---|---|---|---|---|---|---|
| 类 | `NPU` | 设备边界类 · System 子系统 | private | — | — | — |
| 属性 | `devPath` | `std::string` | private | 构造注入 | — | 设备节点路径（devfreq 挂点），测试可注入伪 sysfs |
| 属性 | `freqTable` | `std::vector<int>` | private | 初值空 | — | 可用频率表（Hz），首次采集后记住 |
| 属性 | `curFreq` | `int` | private | 初值 `0` | — | 当前频率（Hz） |
| 方法 | `getInfo` | 无 | 同子系统 → `DeviceInfo` | `devPath` 已注入 | 无副作用 | 确保频率表已读 → 读当前频率 → 组装返回 |
| 方法 | `apply` | `const DeviceAction&` | 同子系统 → `bool` | `devPath` 已注入；目标频率在可用范围内 | `curFreq` 更新；失败保持原值 | 校验目标频率 ∈ `freqTable` → `setFreq()` → 回读更新 `curFreq` |
| 方法 | `readFreqTable` | 无 | private → `bool` | `devPath` 已注入 | 填充 `freqTable` | 读可用频率节点 → 解析列表 → 写入 `freqTable` |
| 方法 | `setFreq` | `int`（Hz） | private → `bool` | `freqHz` 已在 `freqTable` 内 | 频率节点已写入 | 经 `FileService` 写频率节点 → 返回写入结果 |

> **同构说明**：`CPU` 与 `NPU` 两类**方法集合一致**（`getInfo` / `apply` / `readFreqTable` / `setFreq`），**属性差异**：`CPU` 多出 `procStatPath` / `prevTotal` / `prevIdle` / `hasPrevSample`（用于读 `/proc/stat` 差分算利用率）；NPU 无 procfs，故无此四项。设备节点语义亦不同（CPU 为 cpufreq 挂点、NPU 为 devfreq 挂点）。

**算法步骤**

**`NPU::getInfo()`**
- S1. 若 `freqTable` 为空 → 调 `readFreqTable()`；失败不阻断。
- S2. 读当前频率节点得 `curFreq`。**NPU 无 `/proc/stat` 差分**，故不做利用率差分计算（与 `CPU::getInfo` 的差异点）。
- S3. 组装 `DeviceInfo`（当前频率、频率表；利用率字段视实现填负载或留空）返回。

**`NPU::apply(action)`**
- S1. 校验目标频率 `freqHz ∈ freqTable`；不在表内 → 返回 `false`，`curFreq` 保持原值。
- S2. 调 `setFreq(freqHz)` 写入频率节点；失败 → 返回 `false`，`curFreq` 保持原值。
- S3. **回读**频率节点确认生效，更新 `curFreq`；返回 `true`。

**`NPU::readFreqTable()`**
- S1. 经 `FileService::read` 读可用频率节点（devfreq 挂点）；读失败或为空 → 返回 `false`。
- S2. 解析节点内容为整数频率数组。
- S3. 写入 `freqTable`；返回 `true`。

**`NPU::setFreq(freqHz)`**
- S1. 前置校验：`freqHz` 已在 `freqTable` 内；否则返回 `false`。
- S2. 经 `FileService::write` 将 `freqHz` 写入频率节点。
- S3. 返回写入是否成功。

> **与 `CPU` 的差异小结**：`NPU` 的 `getInfo` **不做** `/proc/stat` 差分（无 procfs）；其余三个方法（`apply` / `readFreqTable` / `setFreq`）算法与 `CPU` **完全同构**，仅设备节点路径不同。

### 3.13 设计类图与类间关系

用例设计涉及的 12 个设计类及其静态关系如下图所示。图中**实线**表示子系统内或跨子系统的设计类关联；**虚线箭头**表示设计类对 common 子系统公共基础设施（`UDSService`、`FileService`、`ONNXruntime`）的依赖。

![设计类图（用例设计视图）](images/类设计类间关系.png)

#### 关系说明（13 条边）

- `serviceInterface` **依赖** `UDSService` 完成 UDS 报文收发；
- `serviceInterface` **关联** `schedManager`，作为推理请求入口；
- `schedManager` **关联** `systemMonitor`（获取状态）、`decisionExecutor`（执行动作）、`decisionMaker`（发起 DQN 推理）；
- `systemMonitor` **关联** `systemInterface`（进入 System 子系统）；
- `decisionExecutor` **依赖** `decisionMaker`（使用其生成的 `Decision`）；
- `systemInterface` **关联** `systemManager`；
- `systemManager` **组合** `CPU`、`NPU`（管理器强拥有设备边界对象）；
- `CPU`、`NPU` **依赖** `FileService` 完成底层文件 I/O；
- `decisionMaker` **依赖** `ONNXruntime` 完成模型推理。

#### 关系汇总表

| # | 边 | 语义 | 数量 |
|---|---|---|---|
| 1 | `serviceInterface` → `UDSService` | 依赖 | 1:1 |
| 2 | `serviceInterface` — `schedManager` | 关联 | 1:1 |
| 3 | `schedManager` — `systemMonitor` | 关联 | 1:1 |
| 4 | `schedManager` — `decisionExecutor` | 关联 | 1:1 |
| 5 | `schedManager` — `decisionMaker` | 关联 | 1:1 |
| 6 | `systemMonitor` — `systemInterface` | 关联 | 1:1 |
| 7 | `systemInterface` — `systemManager` | 关联 | 1:1 |
| 8 | `systemManager` ◆— `CPU` | 组合 | 1:1 |
| 9 | `systemManager` ◆— `NPU` | 组合 | 1:1 |
| 10 | `CPU` → `FileService` | 依赖 | 1:1 |
| 11 | `NPU` → `FileService` | 依赖 | 1:1 |
| 12 | `decisionExecutor` → `decisionMaker` | 依赖 | 1:1 |
| 13 | `decisionMaker` → `ONNXruntime` | 依赖 | 1:1 |

## 4. 数据设计

### 4.1 需要持久保存的数据

中间件为瞬时状态与点对点通信系统，无业务数据库需求。需要持久保存的仅为配置与模型资产，全部为 `schedd` 的只读输入。

| 数据 | 持久性 | 载体 | 写入方 | 读取方 |
|---|---|---|---|---|
| 调度配置 | 持久 | `schedd.conf` | 运维 | `systemManager` |
| 模型文件 | 持久 | `<model_dir>/<model_file>`（如 `dqn.onnx`） | Python 训练侧 | `ONNXruntime::load`（经 `decisionMaker`） |
| 模型契约 | 持久 | `<contract_dir>/<contract_file>`（如 `dqn.json`） | Python 训练侧 | `decisionMaker` |
| 实时状态量 | 易失 | `systemMonitor::lastInfo`（进程内） | 采集链路 | 决策链路 |

运行日志**不保存**。日志仅输出至 stdout/stderr，由 systemd 的 `StandardOutput=journal` 接管，属诊断流，不参与决策与恢复逻辑。`schedd` 自身不持有任何日志文件。

不引入数据库：单机单进程、无并发写、无查询需求。

### 4.2 持久数据的访问方式

三类持久数据均可读、可写、可检查；区别仅在于**写的方式**。

| 数据 | 读 | 写 | 检查 | 写的方式 |
|---|---|---|---|---|
| 配置数据 | ✔ | ✔ | ✔ | 逐键就地改写（唯一可局部修改者） |
| 模型文件（`.onnx`） | ✔ | ✔ | ✔ | **整体文件替换** |
| 模型契约（`.json`） | ✔ | ✔ | ✔ | **整体文件替换** |

**配置数据（`schedd.conf`）**
读、写、检查均可。写的方式为逐键就地改写，运维可单独调整某个键的值（如 `default_model`、`cpu_freq_max`）。检查指打开文件核对当前取值。

**模型文件（`<model_dir>/<model_file>`）**
读、写、检查均可。写的方式为整体文件替换——训练侧产出新模型后替换旧文件。检查指加载校验：模型能否成功加载，以及实际输入维度是否与契约的 `input_dim` 一致。

**模型契约（`<contract_dir>/<contract_file>`）**
读、写、检查均可。写的方式为整体文件替换——不做单键局部修改，整份文件替换。检查指内部一致性校验。

**设计侧对应**

| 数据 | 设计中对应的接口 | 替换后的生效方式 |
|---|---|---|
| 配置 | `ConfigFile::getConfig` / `configValidate` | 重启 `schedd` 后生效（无热重载） |
| 模型 | `ONNXruntime::load(模型路径)` | **热切换**（经 `SetModel`）；加载失败保持旧模型 |
| 契约 | `decisionMaker` 加载契约并校验 | 随模型成对切换；校验失败保持旧契约 |

模型与契约的替换通过改配置的 `model_dir`/`model_file`（契约同理）指向新文件，或直接覆盖同路径文件。运行期亦可通过 `SetModel` **热切换**：**先建后换**——新建会话加载新模型，成功才原子替换，失败则丢弃新会话、旧模型原封不动（无需回滚逻辑）。

### 4.3 配置文件设计（`/etc/schedd/schedd.conf`）

采用 INI 风格，由配置加载器解析，启动时读取一次。

#### 4.3.1 `[server]` 节

| 键 | 类型 | 默认值 | 说明 | 承载类 |
|---|---|---|---|---|
| `socket_path` | string | `/run/schedd.sock` | UDS 监听路径 | `serviceInterface::socketPath` |
| `max_conn` | int | 16 | 最大并发连接数 | `serviceInterface` |

#### 4.3.2 `[agent]` 节

| 键 | 类型 | 默认值 | 说明 | 承载类 |
|---|---|---|---|---|
| `default_model` | string | **dqn** | 启动默认模型；取值经校验后写入，非法取值仅告警并保持现状 | `schedManager::model` |
| `infer_timeout_us` | int | 5000 | 推理超时（微秒） | `decisionMaker` |
| `model_dir` | string | `/usr/lib/schedd/models` | 模型目录（路径第一段） | `decisionMaker` |
| `model_file` | string | `dqn.onnx` | 模型文件名（路径第二段） | `decisionMaker::modelPath` |
| `contract_dir` | string | `/usr/lib/schedd/models` | 契约目录（路径第一段） | `decisionMaker` |
| `contract_file` | string | `dqn.json` | 契约文件名（路径第二段） | `decisionMaker` |

**路径拆两段的用意**：`model_dir` / `contract_dir` 随**部署环境**变化（换机器只改目录），`model_file` / `contract_file` 随**算法**变化（换算法只改文件名），两段各自独立。完整路径由**调用方**取两段后拼接（`dir + "/" + file`），**以完整路径传入中间件**；`ConfigFile` 不提供拼接方法，中间件不参与拼接。

**取值语义**：`default_model = static` 表示不指向任何模型（仅作标识，不触发特殊逻辑）。模型或契约留空则不加载，由降级链兜底。

#### 4.3.3 `[system]` 节

| 键 | 类型 | 默认值 | 说明 | 承载类 | 注入至 |
|---|---|---|---|---|---|
| `cpu_freq_min` | int | 0（不限） | CPU 档位收窄下限（Hz） | `systemManager` | 档位校验 |
| `cpu_freq_max` | int | 0（不限） | CPU 档位收窄上限（Hz） | `systemManager` | 档位校验 |
| `npu_freq_min` | int | 0（不限） | NPU 档位收窄下限（Hz） | `systemManager` | 档位校验 |
| `npu_freq_max` | int | 0（不限） | NPU 档位收窄上限（Hz） | `systemManager` | 档位校验 |
| `cpu_freq_path` | string | 空（用编译期默认） | CPU 频率设备节点路径 | `systemManager` | → `CPU::devPath` |
| `npu_freq_path` | string | 空（用编译期默认） | NPU devfreq 节点路径 | `systemManager` | → `NPU::devPath` |
| `proc_stat_path` | string | 空（用 `/proc/stat`） | CPU 统计文件路径 | `systemManager`（配置入口） | → `CPU::procStatPath` |
| `sample_interval_ms` | int | 500 | 采样间隔（毫秒）；非正值回退 500 | `systemManager::sampleIntervalMs` | 采样循环 |

#### 4.3.4 配置解析约定

**未配置与配置为空值需明确区分。** 整型项遇留空或非法字面量一律返回默认值，四个频率上下限与三个路径项正是靠这一点表达未配置。另有 `has(section, key)` 用于判断键是否存在。

**四个频率上下限的作用方式**为：装配期收窄设备档位表，而非另设限制表；随后所有档位夹取自然落进允许区间。过滤后档位表为空则保留原表并返回失败，不计为配置错误。

**配置无热重载**，运行中修改不生效。

### 4.4 模型契约设计（`<contract_dir>/<contract_file>`）

契约采用**表格法**实现行为与网络的分离：契约承载状态定义与动作定义，`.onnx` 文件只承载网络参数。此处的「表格法」指**契约的表示形式**（以表格描述状态/动作），与「表格法 Q 学习」这种算法后端是两回事。

| 字段 | 分类 | 说明 |
|---|---|---|
| `input_dim` | 状态定义 | 输入向量维度 |
| `state_bounds` | 状态定义 | 各输入分量的归一化上下界，`fields` 逐项与输入向量位置对应 |
| `output_dim` | 动作定义 | 动作数 |
| `actions` | 动作定义 | 动作编号到语义的映射（CPU 份额、批大小、CPU 与 NPU 档位） |

**分离与降耦合**：C++ 侧仅读契约即可获知维度约定，不依赖训练侧代码或网络结构。

**三道一致性校验**：

1. 要求 `input_dim` 等于 `state_bounds.fields` 项数；
2. 要求 `actions` 元素数等于 `output_dim`；
3. 校验模型的实际输入维度与 `input_dim` 一致。

任一校验失败则保持原模型与原契约（热切换失败即丢弃新会话），不进入运行态。

**契约的边界**：不含物理量纲极值。`state_bounds` 描述的是归一化之后的取值范围。各分量归一化所用的标度常量因此固化在编码器实现内并标注为契约冻结项，换平台时必须与训练侧同步修改。

**与模型成对**：契约必须与模型**成对切换**（DQN 与表格法的 `input_dim` / `actions` 大概率不同），故两者路径结构一致（同为两段式）。

### 4.5 数据操作的类封装

> **边界提示**：本节三个封装类（`ConfigFile` / `ModelFile` / `ContractFile`）**属 common 基础设施层，不计入 §2 的 12 个设计类**。


#### 4.5.1 封装类与数据操作对照

| 数据操作 | 封装类 | 方法名 | 参数 |
|---|---|---|---|
| 读取配置 | `ConfigFile` | `getConfig` | `configPath` |
| 校验配置 | `ConfigFile` | `configValidate` | `configPath` |
| 读取模型 | `ModelFile` | `loadModel` | `modelPath` |
| 校验模型 | `ModelFile` | `modelValidate` | `modelPath` |
| 读取契约 | `ContractFile` | `loadContract` | `contractPath` |
| 校验契约 | `ContractFile` | `contractValidate` | `contractPath` |

#### 4.5.2 `ConfigFile` —— 配置文件封装

覆盖 `/etc/schedd/schedd.conf`，负责**解析 INI 文本格式**并提供逐键取值。

| 方法名 | 参数 | 返回 | 说明 |
|---|---|---|---|
| `getConfig` | `const std::string& configPath` | bool | 读入并解析配置文件；失败不影响已有内容 |
| `configValidate` | `const std::string& configPath` | bool | 校验配置：节与键是否存在、取值格式是否合法 |

覆盖的配置键：`[server]` 的 `socket_path`、`max_conn`；`[agent]` 的 `default_model`、`infer_timeout_us`、`model_dir`、`model_file`、`contract_dir`、`contract_file`；`[system]` 的四个频率上下限、`cpu_freq_path`、`npu_freq_path`、`proc_stat_path`、`sample_interval_ms`。

访问方式：可读、可检查。

**算法步骤**

**`ConfigFile::getConfig(configPath)`**
- S1. 校验 `configPath` 非空；为空 → 返回 `false`（不清空已解析内容）。
- S2. 经 `FileService::read` 读全文；读失败 → 返回 `false`，**已有解析结果保持不变**。
- S3. 逐行扫描：
  - S3.1 跳过空行与 `#`/`;` 起始的注释行；
  - S3.2 遇 `[section]` → 切换当前节名；
  - S3.3 遇 `key = value` → 按第一个 `=` 切分，`trim` 两侧空白，存入 `(section, key) → value` 映射；
  - S3.4 行格式非法（无 `=` 且非节头/注释）→ **跳过该行**（不整体失败，容错解析）。
- S4. 解析完成 → 以新映射**整体替换**内部存储；返回 `true`。

**`ConfigFile::configValidate(configPath)`**
- S1. 校验 `configPath` 非空；为空 → 返回 `false`。
- S2. 读入并解析（可复用 `getConfig` 逻辑，但**不污染**已生效的配置）。
- S3. **逐节逐键校验**：三个节 `[server]` / `[agent]` / `[system]` 是否存在；各节关键键是否存在（缺失 → 记录错误）。
- S4. **取值格式校验**：整数键（如 `max_conn`、`sample_interval_ms`）须能解析为整数；路径键非空。
- S5. 全部通过 → 返回 `true`；任一失败 → 返回 `false`（同时可输出具体失败键名，便于排障）。

#### 4.5.3 `ModelFile` —— 模型文件封装

覆盖 `<model_dir>/<model_file>`（如 `dqn.onnx`），负责**模型文件的读取与校验**。**不区分内部是 DQN 网络还是表格法 Q 表**——对运行时而言都只是一张 `.onnx` 计算图。

| 方法名 | 参数 | 返回 | 说明 |
|---|---|---|---|
| `loadModel` | `const std::string& modelPath` | bool | 读入模型文件（收完整路径） |
| `modelValidate` | `const std::string& modelPath` | bool | 校验模型：路径非空、文件可读 |

访问方式：可读、可检查。

**算法步骤**

**`ModelFile::loadModel(modelPath)`**
- S1. 校验 `modelPath` 非空；为空 → 返回 `false`。
- S2. 经 `FileService::exists` 探测文件是否存在；不存在 → 返回 `false`。
- S3. 读入模型文件（建立可加载句柄/缓冲）；读失败 → 返回 `false`。
- S4. 返回 `true`（**本类只负责读入与文件级就绪，不建立推理会话**——会话由 `ONNXruntime` 承担）。

**`ModelFile::modelValidate(modelPath)`**
- S1. 校验 `modelPath` 非空；为空 → 返回 `false`。
- S2. 校验文件可读（存在 + 可打开）；不可读 → 返回 `false`。
- S3. 可选的轻量格式校验（如文件非空、魔数/头部合法）；通过 → 返回 `true`。

#### 4.5.4 `ContractFile` —— 契约文件封装

覆盖 `<contract_dir>/<contract_file>`（如 `dqn.json`），负责**解析契约文本格式**。

| 方法名 | 参数 | 返回 | 说明 |
|---|---|---|---|
| `loadContract` | `const std::string& contractPath` | bool | 解析契约；文件缺失、格式非法、字段缺失均返回 false（收完整路径） |
| `contractValidate` | `const std::string& contractPath` | bool | 校验内部一致性（见 §4.4 三道校验前两条） |

访问方式：可读、可检查。

**算法步骤**

**`ContractFile::loadContract(contractPath)`**
- S1. 校验 `contractPath` 非空；为空 → 返回 `false`。
- S2. 经 `FileService::read` 读全文；读失败（文件缺失）→ 返回 `false`。
- S3. 解析 JSON（`dqn.json`）；**格式非法**（括号/引号不配对等）→ 返回 `false`。
- S4. 校验必需字段存在（`input_dim`、`output_dim`、状态上下界、动作定义等）；**字段缺失** → 返回 `false`。
- S5. 解析成功 → 存入内部结构；返回 `true`。

**`ContractFile::contractValidate(contractPath)`**
- S1. 校验 `contractPath` 非空；为空 → 返回 `false`。
- S2. 读入并解析契约（不污染已生效内容）。
- S3. 校验内部一致性（§4.4 三道校验前两条）：
  - S3.1 `input_dim` 与状态上下界数组长度一致；
  - S3.2 `output_dim` 与动作定义条数一致。
- S4. 全部一致 → 返回 `true`；否则 → 返回 `false`。
