# 项目学习主线与下一阶段路线

记录日期：2026-08-29

## 原始想法

> 现在的任务是需要你快速预览整个大项目结构。由于这是一个完全由C组成的练手项目，我偏好于多方面推进，以模拟器理解状态机模型；UI理解高级编程；SDL虚拟屏幕和寄存器封装理解MMIO和寄存器总线。现在对于并发和高级设备，我的下一步是：1、在模拟器子项目中添加Sharp LR35902，玩GameBoy Color游戏；2、在tools中添加基于Bulk的DAPLink，也许需要FreeRTOS；3、还是在模拟器子项目中添加自己手写的调度模型，用于替代FreeRTOS。

## 项目定位

这是一个以 C 为中心的嵌入式练习场。主要实现是 C，底层启动和寄存器操作中允许使用内联汇编，根 CMake 工程也启用了 ASM 语言。项目不是围绕单一产品收敛，而是用几个相互呼应的子系统练习同一组基础能力：显式状态、内存所有权、总线访问、平台抽象、事件循环和可测试性。

三条已有学习主线是：

1. 用模拟器理解状态机、指令执行、异常、中断和设备协作。
2. 用 UI 理解数据结构、回调、树、脏区、资源生命周期和模块化设计。
3. 用 SDL 虚拟屏幕与 MCU 寄存器封装理解端口适配、MMIO 和寄存器总线。

## 当前仓库速览

```text
asm_stm32/
├─ CMakeLists.txt                 ARM 裸机根工程
├─ cmake/                         arm-none-eabi 工具链配置
├─ user/
│  ├─ inc/                        公共配置、编译器和入口头文件
│  └─ src/
│     ├─ main.c                   当前裸机实验入口
│     ├─ cortex_m0/               Cortex-M0 启动、内核寄存器和芯片寄存器
│     ├─ cortex_m4/               Cortex-M4 启动、内核寄存器和多芯片内存定义
│     └─ mipsel-emu/              独立构建的 MIPS32EL 模拟器
└─ tools/
   ├─ vdo2bin.py                  辅助转换工具
   └─ sdl-player/                 独立构建的 SDL2 framebuffer/UI 实验
```

### ARM 裸机层

- 根 CMake 工程使用 `arm-none-eabi`，产出 ELF、HEX 和 BIN。
- `cortex_m0/` 与 `cortex_m4/` 中包含 startup、SysTick、SCB、NVIC、内存映射和具体芯片头文件。
- STM32F042 的 RCC、GPIO、FLASH 等封装直接把地址映射成 C 结构或 `volatile` 寄存器访问，是观察 MMIO、位域语义和总线副作用的入口。
- 根工程当前仍带有实验性质：链接脚本、源码 glob、include 路径和 `main.c` 所代表的目标并未完全收敛到一个统一芯片配置。新增复杂固件时宜使用独立 target 或独立子工程，避免继续扩大目标混用。

### MIPS32EL 模拟器

- `Registers` 是 CPU 的显式状态，包含通用寄存器、PC、HI/LO、CP0、TLB、分支与异常相关状态。
- `mipsel_emu_step()` 和 `mipsel_emu_run_steps()` 提供单步和有预算的运行接口，适合作为状态机模型范本。
- `platform_memory_*()` 抽象原始物理内存后端；`platform_bus_read/write()` 在此之上处理 RAM 与 MMIO 分派。
- UART16550、异常、中断、镜像加载、反汇编、Monitor、TUI/宿主 run loop 已经分层，并有独立测试。
- 核心静态库不依赖 POSIX；宿主调试前端单独构建。该边界很适合复用到后续 CPU 模拟器。

CPU 的主链路可概括为：

```text
Registers -> fetch/translate -> bus read -> decode/execute
          -> exception/interrupt -> commit state -> cycle/device update
```

### SDL 虚拟屏幕与 UI

- `ui_core` 只包含 `UiSurface` 与 `UiBuffer`，不依赖 SDL。
- `UiSurface` 统一 framebuffer、stride、像素所有权和 dirty rectangle。
- `UiBuffer` 是共享 framebuffer 的子视图树，通过回调绘制，并向根 surface 传播脏区。
- 控件建树使用并查集把空间上邻近的控件归组，是一个很好的“数据结构服务于渲染策略”的练习。
- `sdl_platform` 是唯一正式接触 SDL 类型的层，负责窗口、texture、事件、时钟和 present。
- `main.c` 只负责组装应用与平台、驱动事件循环；`demo_core` 负责演示状态。
- `ui/experimental/` 目前只是 object/event/animation 和固定块池的草案，不属于正式运行时能力。

显示链路可概括为：

```text
Demo/UI -> CPU framebuffer -> dirty region -> SDL texture -> renderer/window
```

它与模拟器的总线链路在设计上是同构的：核心只操作稳定的数据契约，平台适配层负责把操作转换成真实宿主 API 或硬件副作用。

## 下一阶段的三个方向

### 方向 A：Sharp LR35902 / SM83 与 Game Boy Color

建议把新模拟器做成与 `mipsel-emu` 并列的独立子项目，而不是把两套 ISA 塞入同一个 CPU 状态结构。可以复用设计方法、测试方式和少量无架构属性的工具，但保持 CPU、总线和设备模型独立。

推荐边界：

```text
lr35902-emu/
├─ cpu.*             寄存器、标志、取指、译码、执行和中断
├─ bus.*             64 KiB 地址空间和设备分派
├─ timer.*           DIV/TIMA/TMA/TAC
├─ ppu.*             LCD 时序与 framebuffer 输出
├─ cartridge.*       ROM/RAM 与 MBC
├─ joypad.*          输入与中断
├─ emulator.*        整机状态和有预算的推进接口
├─ platform/         ROM、输入、显示、时钟适配
└─ tests/            指令、时序、总线和设备测试
```

实施顺序：

1. 先完成 CPU 状态、64 KiB 总线、普通指令、CB 前缀指令、中断和 HALT 行为，并用公开 CPU 测试 ROM 验证。
2. 加入 timer、joypad、最小 cartridge 和 MBC1，先以 DMG 兼容模式跑通基础 ROM。
3. 加入按 dot/机器周期推进的 PPU，将 160×144 framebuffer 接到现有 `UiSurface`/SDL 后端。
4. 再补 CGB 的 VRAM/WRAM banking、彩色 palette、double speed、HDMA 和 CGB cartridge 行为。
5. APU 声音放在画面和时序稳定之后，避免一开始同时调试过多设备。

“能玩 GBC 游戏”的目标不只需要 LR35902 指令集，还需要准确的 PPU、timer、中断、DMA、内存 banking 和 MBC。第一个可交付里程碑应是“CPU 测试通过”，第二个是“DMG 画面可见”，第三个才是“CGB 特性可用”。

建议保留与现有 MIPS 核心相似的有界接口：

```c
void lr35902_step(Lr35902 *cpu);
uint32_t gbc_run_cycles(GbcMachine *machine, uint32_t cycle_budget);
```

这样 CPU、PPU、timer 和输入都能由同一个确定性时间轴推进，也便于桌面测试和未来嵌入 MCU。

### 方向 B：基于 USB Bulk 的 DAPLink / CMSIS-DAP

这里需要区分两个概念：DAPLink 是完整的调试探针固件工程，CMSIS-DAP v2 是其中常见的 USB Bulk 传输协议。若目标是手写练习，建议先把范围定义成“可工作的 CMSIS-DAP v2 Bulk 调试探针”，再逐步增加 DAPLink 的拖拽烧录、串口和板级管理能力。

建议把宿主验证工具与探针固件分开：

- `tools/dap-bulk/`：USB 枚举检查、命令生成、抓包分析、吞吐和回环测试等 PC 工具。
- 独立 firmware target：USB descriptor、Bulk OUT/IN 队列、CMSIS-DAP command engine、SWD/JTAG 物理层和目标板配置。

最小推进顺序：

1. 完成 USB device 枚举和一对 Bulk IN/OUT endpoint 的可靠回环。
2. 实现 `DAP_Info`、`DAP_Connect`、`DAP_Disconnect`、`DAP_SWJ_Clock` 与基础 SWJ sequence。
3. 实现 `DAP_TransferConfigure`、`DAP_Transfer` 和 `DAP_TransferBlock`，验证 SWD 读写 IDCODE 与目标内存。
4. 增加 packet queue、超时、错误恢复和吞吐统计，再决定是否引入 RTOS。
5. 最后再扩展 CDC 串口、拖拽烧录或 trace 等高级设备。

FreeRTOS 不是 Bulk 传输成立的前提。第一版可用中断 + 环形队列 + cooperative service loop 做出确定性数据路径；当 USB、SWD、CDC、trace 等确实需要独立阻塞流程、优先级和背压管理时，再实现一版 FreeRTOS 后端进行对照。

### 方向 C：手写调度模型

先把目标定义为“小型、确定性、可测试的调度模型”，而不是立即完整替代 FreeRTOS。首版不需要上下文切换汇编，可以从 cooperative、run-to-completion 的任务开始；这已经足够研究状态迁移、事件、等待队列、timer 和公平性。

建议的任务状态：

```text
NEW -> READY -> RUNNING -> READY
                 |  |
                 |  +-> SLEEPING --timer--> READY
                 +----> BLOCKED  --event--> READY
                 +----> STOPPED
```

最小接口可以围绕显式时间和单步推进设计：

```c
bool scheduler_post(Scheduler *scheduler, Event event);
void scheduler_wake_at(Task *task, uint64_t deadline);
uint32_t scheduler_run(Scheduler *scheduler,
                       uint64_t now, uint32_t work_budget);
```

推荐阶段：

1. 固定容量任务表、READY 队列、显式 task state 和确定性的 round-robin。
2. 注入式单调时钟、sleep queue、事件队列和超时。
3. ISR-safe 的单生产者/单消费者队列，用测试模拟中断到任务的交接。
4. 优先级、饥饿检测、背压和 trace；所有调度决定都能重放。
5. 只有需要真正抢占时，再为 Cortex-M 增加 SysTick/PendSV 上下文切换，并与 cooperative 版本保持相同的上层事件模型。

这个调度器可先用于 LR35902 整机的 CPU/PPU/timer 协同，随后用于 DAP Bulk 的 USB RX、command engine、SWD engine 和 USB TX 服务。这样它不是孤立练习，而会在两个真实消费者中暴露设计问题。

## 多方向并进方式

三个方向可以并行推进，但每轮只让它们在稳定接口处汇合：

| 轮次 | LR35902/GBC | Bulk DAP | Scheduler |
| --- | --- | --- | --- |
| 1：状态可见 | CPU state、单步和总线测试 | USB packet 回环与统计 | task state、READY 队列和 trace |
| 2：设备协作 | timer/interrupt/PPU 时序 | command/SWD/TX 分段服务 | event、sleep、deadline 和 budget |
| 3：平台接入 | SDL framebuffer 与输入 | MCU USB/SWD 寄存器后端 | ISR queue 与硬件时钟后端 |
| 4：高级能力 | CGB banking、DMA、颜色 | CDC、trace、吞吐优化 | 优先级或 Cortex-M 抢占实验 |

建议始终遵守以下约束：

- 核心状态显式存放在结构体中，不依赖不可观察的全局状态。
- 所有长流程都提供 `step`、`poll` 或带 budget 的运行接口。
- 核心通过 bus/callback/ops 接口访问外部世界，不直接依赖 SDL、USB 或某个 MCU。
- 平台层负责真实副作用，核心层可在宿主测试中用 fake backend 替换。
- 时间从调用方注入；测试不依赖真实 sleep。
- 固定宽度整数、端序、寄存器副作用和内存所有权都写进接口契约。
- 每增加一个设备，先写寄存器级或状态迁移测试，再接真实平台。

## 当前建议优先级

1. 先搭一个很小的 cooperative scheduler，并把状态迁移和虚拟时钟测试做扎实。
2. 同时建立 LR35902 的 CPU/bus 骨架，让 scheduler 或统一 cycle budget 驱动 CPU、timer 和 PPU。
3. Bulk DAP 先做无 RTOS 的 endpoint 回环和 CMSIS-DAP 最小命令集。
4. 当 DAP 出现多个真实并发服务后，同时实现手写 scheduler 与 FreeRTOS 两种后端，比较代码量、延迟、吞吐、内存和可调试性。

最终目标不是证明某一种调度方式永远更好，而是通过相同工作负载理解 superloop、cooperative scheduler、抢占式 RTOS 各自解决的问题。
