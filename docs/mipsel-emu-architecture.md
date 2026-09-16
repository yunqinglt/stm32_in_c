# MIPS 模拟器、SDL 与开源 GPU 微架构建议

本文是面向当前仓库的设计建议，不改变现有实现。建议先保持一个确定性的单线程模式作为测试基线，再逐步增加 SDL 独立线程、外部中断控制器、多核和开源 GPU 设备。

## 结论先行

1. SDL 暂时不需要更换渲染引擎。当前主要问题是呈现线程和 CPU 模拟线程耦合，以及 VSync 可能阻塞 guest CPU。
2. 不要让每一次 RAM/MMIO 访问都经过宿主 IPC。RAM 和常规 MMIO 应保留低延迟直接路径，IPC 或无锁队列只用于 DMA、宿主事件、帧提交等异步操作。
3. Vortex 和 Nyuzi 都不是 MIPS 的 drop-in replacement，应作为 MIPS SoC 中的片上 GPU/加速器，通过 MMIO、共享内存命令环和 IRQ 接入。
4. Linux 侧建议先做最小 DRM/KMS scanout 驱动，再按选定 GPU 增加 compute/render 提交接口，并启用 fbdev emulation。
5. 多核、MT ASE、VI/EIC 和 SmartMIPS 应按阶段实现。先完成设备模型、PIC、IPI 和确定性双核，再考虑宿主并行线程。

## 当前代码的性能落点

当前实现中有几个明确的热点：

- `user/src/mipsel-emu/emu.c` 的 `cpu_step()` 每条指令都经过函数指针表，并调用 observer、地址翻译和 `platform_update_interrupts()`。
- `update_cycle()` 每条指令都递增 Count 并检查 Compare。
- `user/src/mipsel-emu/runloop.c` 在同一线程中执行最多 65536 条指令，然后调用 SDL service。
- `user/src/mipsel-emu/platform.c` 用硬编码分支检查 framebuffer、UART 和 RAM，每次 framebuffer 写入都重新做范围判断和 dirty rectangle 合并。
- `tools/sdl-player/platform/sdl/sdl_display.c` 使用 streaming texture，并在 `SDL_RenderPresent()` 中启用 VSync；如果每个执行批次都产生 dirty region，VSync 会反向限制模拟器速度。

因此，首先应该做 profiling，而不是直接更换 SDL、OpenGL 或 Vulkan。建议至少分别测量：guest instructions/s、MMIO writes/s、dirty region 合并次数、`SDL_UpdateTexture()` 时间和 `SDL_RenderPresent()` 阻塞时间。

### 当前已落地的宿主边界

Qt6 图形前端已经作为宿主层接入 `mipsel-emu`，而 `mipsel-emu_core` 保持纯 C、无 Qt
依赖。无参数时启动 Qt 监视器；带参数时仍转发到原有 CLI/TUI。GUI 的连续执行调用
`mipsel_emu_run_steps()`，一次定时器回调提交一批 guest 指令，避免每条指令都跨越
C++ 包装层。寄存器、UART 和 Qt framebuffer 视图在同一宿主线程刷新；发现 SDL2 时，
同一个 RGB565 framebuffer 还会通过现有 SDL surface 在伴随窗口显示。

这不是最终的并行架构：当前仍是确定性单线程模式，SDL/Qt 刷新发生在批次边界。后续
引入 frame mailbox、独立 SDL 线程或设备线程时，必须保留 `mipsel_emu_run_steps()` 的
批量接口和共享内存语义，不能退回到每次 RAM/MMIO 访问都经过 IPC 的设计。

## SDL 优化方案

### 推荐的线程边界

SDL 线程独占以下对象：

- `SDL_Window`
- `SDL_Renderer`
- `SDL_Texture`
- SDL 事件队列

vCPU 线程只修改 guest framebuffer，并发布帧描述：

```c
struct frame_update {
    uint64_t sequence;
    uint32_t x;
    uint32_t y;
    uint32_t width;
    uint32_t height;
};
```

SDL 线程以固定频率取最新描述。中间已经过时的帧可以丢弃，避免模拟器等待显示设备。

### 现有 SDL 路径的低风险优化

1. 不要让每个 CPU batch 都调用 `SDL_RenderPresent()`；按宿主单调时钟以 30/60 Hz 刷新。
2. 可以暂时去掉 `SDL_RENDERER_PRESENTVSYNC`，改由 SDL 线程手动限速。
3. `SDL_RenderCopy()` 已经覆盖目标区域时，通常不需要额外的 `SDL_RenderClear()`。
4. 保留 dirty rectangle，但将 dirty tracking 从逐次像素写入改成每个 guest batch 合并。
5. guest framebuffer 是 canonical buffer；SDL 线程使用双缓冲或 dirty-region staging buffer，避免 CPU 写入和 SDL 读取同时发生。
6. 保留 RGB565 streaming texture。640x480 的完整帧只有约 600 KiB，60 Hz 的带宽并不高，GPU API 替换通常不是主要收益来源。

如果最后发现 SDL 使用的是 software renderer，先检查 `SDL_GetRendererInfo()` 和宿主驱动，再决定是否切换后端。不要在没有 profile 数据前引入 Vulkan、OpenGL context 管理或新的渲染引擎。

## 总线和设备模型

`platform_bus_read()`/`platform_bus_write()` 当前是固定条件链。增加 GPU、PIC、DMA、timer 后，建议改成设备区域表：

```c
struct mmio_region {
    uint32_t base;
    uint32_t size;
    bool (*read)(void *opaque, uint32_t offset,
                 unsigned width, uint32_t *value);
    bool (*write)(void *opaque, uint32_t offset,
                  unsigned width, uint32_t value);
    void (*reset)(void *opaque);
    void *opaque;
};
```

总线需要明确以下规则：

- RAM 使用直接 host pointer 或 bulk callback 快速路径。
- MMIO 只允许设备声明的宽度和对齐方式。
- 设备 read 可以有副作用，Monitor 读取 MMIO 也必须遵守这一点。
- MMIO write 的可见性、DMA 和 IRQ 状态必须有明确的内存屏障语义。
- 设备 reset、tick、IRQ 和 bus decode 不应依赖 Linux 驱动的实现细节。

不要把每个总线请求放入 pipe、socket 或进程间消息。同步 MMIO read 需要等待返回值，IPC 往返会远慢于当前 C callback。若将来确实需要跨进程，使用共享内存 ring buffer 加 `eventfd`/futex，并只传输批量 DMA 或设备事件。

## 开源 GPU 选型与微架构边界

Vortex 和 Nyuzi 都包含 RTL、软件和模拟器，但它们不是可以直接替换 MIPS 指令集的 CPU。当前项目最稳妥的组合是：MIPS 负责 Linux、驱动和设备控制，GPU 负责独立的 kernel/render workload。

| 项目 | Vortex | Nyuzi |
| --- | --- | --- |
| ISA | RISC-V RV32/RV64 基础 ISA 加 GPGPU 扩展 | 自有向量/GPGPU ISA |
| 硬件形态 | 可配置 core、warp、thread、ALU/FPU/LSU/SFU，支持缓存、本地存储和可选图形单元 | 参数化 GPGPU core，向量执行、缓存和可综合 SystemVerilog 核 |
| 软件形态 | C++ SimX、RTL simulator、OpenCL/LLVM 生态，具备较完整的 GPU 软件方向 | emulator、cycle simulator、LLVM 工具链、软件库和测试，偏研究与教学 |
| 图形能力 | 有 rasterizer、texture 等可选单元，但完整图形栈集成复杂 | 主要面向计算和实验性图形应用，需要自己定义 render/scanout 协议 |
| 适合当前项目 | 希望以后接 OpenCL/Vulkan/HIP、可扩展多个 GPU core | 希望用较小的自有向量核和 C/RTL reference model 快速验证 |
| 主要风险 | 当前主线以 RISC-V/PCIe/主机 runtime 为中心，片上 MMIO 接入需要适配 | Linux GPU 驱动和标准 UAPI 生态较少，需要自行定义主机接口 |

如果目标是 LVGL 只使用 2D 绘制，Vortex/Nyuzi 都可能过重。一个更实际的第一版可以只实现 shared framebuffer、blit、矩形填充和 fence；GPU 的完整 SIMT/向量执行则作为第二阶段。

### 选型建议和上游代码映射

- 选择 Vortex：目标是 OpenCL、Vulkan/HIP 方向，愿意维护 RISC-V GPU kernel 工具链，并接受较复杂的 warp、cache、memory coalescer 和图形流水线。
- 选择 Nyuzi：目标是先得到可读、可控的向量/GPGPU 微架构，用 C reference model、emulator 和 RTL 对照验证，暂时不追求标准 GPU 用户态生态。
- 以当前 MIPS/LVGL 项目为优先时，建议先做 Nyuzi 风格的最小向量/2D backend，等命令协议、PIC、DMA 和 DRM/KMS 稳定后，再接 Vortex 的完整 SIMT 组件。

上游工程的代码应作为参考实现或 git submodule 固定到明确 commit，不要把大规模外部工程直接复制进 `user/src/mipsel-emu`：

- Vortex：`hw/rtl/cache`、`hw/rtl/mem`、`hw/rtl/raster`、`hw/rtl/tex`、`sim/simx`。其中 PCIe/host runtime 是上层封装，片上版本应保留内部 command/scheduler/memory model，另写 platform-bus wrapper。
- Nyuzi：`hardware/core` 是 GPGPU 核，`hardware/fpga` 是板级 SoC 外设包装，`hardware/testbench` 是仿真环境，`software` 包含 emulator、编译器、库和应用。当前项目应移植 core 协议，不必保留 DE2-115 或 VGA 板级包装。

两者的开源许可、子模块许可和工具链许可仍需在选定版本上逐项核对。开源不等于可以忽略许可证、生成文件或外部依赖的再分发条件。

## MIPS CP1 与 GPU 浮点边界

适配 Vortex 或 Nyuzi 不会自动要求实现 MIPS CP1 FPU。两者属于不同的执行域：

```text
MIPS CPU
  └── CP1：MIPS 用户态浮点指令

Vortex/Nyuzi GPU
  └── 自己的 vector/FPU/ALU 和 GPU kernel ISA
```

MIPS 侧只需要分配 GPU buffer、写入 command ring、触发 doorbell，并等待 fence/IRQ。GPU kernel 在 Vortex 或 Nyuzi 的独立 ISA 上执行，不应由 `mipsel-emu` 的 MIPS 指令解释器执行。因此 GPU 使用浮点运算时，应在 GPU functional backend 或 GPU RTL 中实现，而不是把它转化为 MIPS CP1 指令。

当前仓库已经适合延后原生 CP1：

- `tools/lvgl-demo/Makefile` 使用 `-msoft-float`；
- LVGL framebuffer path 不要求 MIPS CP1；
- `user/src/mipsel-emu/linux/emu.config` 保留 `CONFIG_MIPS_FP_SUPPORT=y`，可让 Linux 接管 hard-float 用户 ELF 的软件浮点模拟；
- 当前 COP1 路径可以先产生 Coprocessor Unusable 异常，由 Linux 软件 FPU 路径处理。

建议保持以下策略：

1. MIPS 用户态和 GPU 驱动 runtime 尽量使用 soft-float。
2. MIPS `Config1.FP` 不宣称存在硬件 FPU。
3. 保留 `CONFIG_MIPS_FP_SUPPORT=y`，直到所有 initramfs ELF 都确认是 soft-float。
4. 将 GPU 的浮点能力放在 Vortex/Nyuzi backend 中单独实现。
5. GPU、总线、IRQ 和 DRM/KMS 稳定后，再评估原生 CP1。

只有以下目标才值得实现原生 CP1：

- 需要运行依赖硬件浮点的第三方 MIPS 用户程序；
- 需要硬浮点性能，而不能接受 Linux 软件浮点模拟；
- 需要验证 MIPS FPU 上下文切换、异常和 ABI；
- 需要让内核或测试程序观察真实 CP1 能力。

原生 CP1 不是只增加几条指令，还需要实现 32 个浮点寄存器、FCSR、条件位、`Status.CU1`、`FR`、舍入模式、NaN/无穷/非规格化数、浮点异常、延迟槽和用户态上下文保存。不要直接用宿主机 `float` 结果冒充确定性的 MIPS FPU；以后实现 CP1 时应使用 SoftFloat 或独立的 IEEE-754 reference model。

推荐的优先级为：

```text
GPU command ring
    -> DMA/共享内存
    -> 外部 PIC 和 GPU IRQ
    -> DRM/KMS scanout
    -> GPU functional model
    -> GPU vector/FPU
    -> MIPS CP1
```

GPU 拥有自己的浮点和向量单元，而 MIPS CPU 暂时保持无 CP1，不会违背当前 MIPS/LVGL 目标。

### 共同的片上接口

不使用 PCIe 时，最合适的抽象是一个片上 `platform_device`：

```text
guest driver
    |
    | MMIO registers + IRQ
    v
 open GPU device
    |
    | shared command ring / framebuffer in guest RAM
    v
shared memory / scanout buffer
```

第一版不要尝试复刻完整 Vortex 图形栈或 Nyuzi 的全部实验功能。先定义一个与具体 GPU ISA 解耦、可以由纯 C reference model 执行的主机协议：

### 建议的主机寄存器组

```text
REG_ID             只读设备标识
REG_VERSION        IP/寄存器协议版本
REG_CONTROL        enable/reset/stop
REG_STATUS         idle/busy/fault/vblank
REG_IRQ_STATUS     pending 状态
REG_IRQ_ENABLE     中断屏蔽
REG_IRQ_ACK        写 1 清除或确认中断
REG_CMD_BASE       命令环物理地址
REG_CMD_SIZE       命令环大小
REG_CMD_HEAD       设备消费位置
REG_CMD_TAIL       驱动提交位置
REG_DOORBELL       通知设备检查命令环
REG_FB_BASE        scanout 或 render target 地址
REG_FB_STRIDE      stride
REG_FB_FORMAT      RGB565/RGB888 等
REG_FAULT_CODE     最近一次命令错误
REG_CAPS           core/warp/lane/format 能力位
```

命令环放在 guest RAM 中，而不是把完整数据帧逐字写入 MMIO。驱动提交命令时应遵守：

```text
写入 descriptor/payload
    -> dma_wmb()/wmb()
更新 CMD_TAIL
    -> 写 DOORBELL
设备中断
    -> 读取 STATUS/IRQ_STATUS
    -> ACK/EOI
```

命令 descriptor 建议固定长度、自然对齐、带 magic/version/length/sequence，便于模拟器、硬件 IP 和 Linux 驱动共享验证代码。第一版可以只支持：清屏、矩形、blit、提交 framebuffer、vblank 和 fault。

### Vortex 的适配方式

Vortex 的微架构适合将 GPU 内部分成以下几层：

```text
command processor
    -> kernel/workgroup scheduler
    -> warp scheduler
    -> scalar/vector ALU + FPU + SFU
    -> LSU / memory coalescer
    -> local memory / L1 / L2 / system memory
    -> optional rasterizer / texture / output merger
```

建议以 Vortex SimX 作为功能参考模型，以 RTL simulator 作为周期/接口参考模型；不要把 RTL 内部模块直接暴露给 MIPS 总线。MIPS 驱动只需要看到 command ring、buffer address、launch、fence、fault 和 interrupt。

Vortex 的 kernel binary 是 RISC-V GPU 侧代码，不是 MIPS ELF。驱动需要区分：

- MIPS Linux driver/utility ELF，由当前 musl 工具链编译；
- Vortex GPU kernel，由 Vortex/VOLT 或匹配的 LLVM 工具链编译。

第一版不要尝试让 MIPS 的 `mipsel-emu` 指令解释器执行 Vortex GPU 指令。模拟器应该有独立的 `vortex_device` 和可替换的 C/C++ functional backend。

如果未来需要图形，按以下顺序启用：

1. global memory、local memory 和 kernel launch；
2. buffer copy、clear、fence 和 interrupt；
3. texture/sample；
4. rasterizer、tile buffer 和 output merger；
5. OpenCL/图形 runtime 的正式集成。

这样可以先验证总线和驱动，而不被完整 Vulkan/OpenGL 依赖阻塞。

### Nyuzi 的适配方式

Nyuzi 更适合先做小型、可读的参考设备：

```text
MIPS MMIO command block
    -> Nyuzi work queue
    -> vector lanes / thread contexts
    -> scalar control + vector ALU
    -> cache / shared memory
    -> guest RAM or render target
```

Nyuzi 的 hardware 目录已经包含可配置 core/cache、FPGA SoC 外设和仿真 testbench，software 目录包含 emulator、编译器、库和应用。对本项目建议先抽取以下最小子集：

- 一个或少量 vector core；
- 固定向量宽度；
- global/shared memory；
- command queue 和 completion record；
- framebuffer/scanout 写入；
- fault、halt 和 IRQ。

Nyuzi 的自有 ISA 和工具链意味着 Linux 不能把它当作普通 MIPS process 执行。MIPS 驱动应只负责加载已验证的 Nyuzi image、设置参数和等待 completion；kernel image 的格式、ABI 和 relocation 要固定在协议版本中。

### 建议抽取的微架构模块

不要把 Vortex 或 Nyuzi 的全部 RTL 一次性移植到模拟器。可以先把两者共同需要的执行模型抽象成以下模块：

```text
fetch/decode
    -> workgroup/thread context
    -> issue scheduler
    -> scalar/vector register file
    -> ALU/FPU/SFU
    -> LSU + request coalescer
    -> local/shared memory
    -> cache or direct guest-memory backend
    -> completion/fault/IRQ
```

Vortex 适配时重点保留：

- warp 中的 active-lane mask；
- branch/reconvergence 状态；
- warp scheduler 与 scoreboard；
- vector register file、scalar control path；
- LSU 的 lane request 合并和 memory ordering；
- local memory、L1/L2 与 system-memory adapter。

光栅器、texture sampler、early-Z、tile buffer 等图形模块应晚于 compute path 接入，并通过独立 command opcode 暴露。这样 Vortex 的 SIMT kernel 可以先用 compute/fence 测试，不会被图形格式和 raster corner case 阻塞。

Nyuzi 适配时重点保留：

- 固定向量宽度和 lane active mask；
- scalar/vector register file 的读写规则；
- vector ALU、compare、shuffle、load/store；
- thread context、barrier 和 atomic；
- cache/shared-memory 一致性模型；
- fault PC、halt 原因和 completion record。

Nyuzi 不需要先实现完整 OpenCL runtime。可以用一个固定 kernel ABI：入口 PC、参数块地址、global memory 范围、work-item 数量和返回 fence。以后若要接标准工具链，再增加 kernel metadata、address space 和 barrier 语义。

两种 backend 都应提供两种执行模式：

- functional mode：一次执行一个命令，快速验证结果；
- timing mode：模拟 warp/vector issue、memory latency、队列深度和 IRQ 延迟。

SDL/LVGL 联调使用 functional mode 即可；性能和调度研究再启用 timing mode。不要让每个宿主线程直接代表一个 GPU lane，lane 应保存在一个 warp/vector context 中，由 scheduler 批量执行。

### 纯 C reference model

Vortex 和 Nyuzi 的实际硬件实现都不是纯 C；它们的 RTL 仍需要 SystemVerilog。纯 C 开发可以承担三个角色：

1. 作为 emulator 中的 functional GPU backend；
2. 作为 Linux driver 与 RTL 共用的 command validator；
3. 作为 golden model，对比 RTL simulator 输出。

建议先实现如下接口，而不是在 `platform.c` 中散落 GPU 状态：

```c
struct gpu_backend_ops {
    bool (*reset)(void *opaque);
    bool (*submit)(void *opaque, uint32_t ring_base,
                   uint32_t head, uint32_t tail);
    bool (*read_status)(void *opaque, uint32_t *status);
    void (*tick)(void *opaque, uint64_t cycles);
};
```

同一个 command descriptor parser 可以被 MIPS 模拟器、Linux driver test、Vortex functional model 和 Nyuzi functional model 复用。硬件 RTL 只需要满足同一份 descriptor/IRQ/fence contract。

### 地址和内存

当前目标是 MIPS32 小端，因此第一版可只支持 32 位物理地址。命令环和 framebuffer 建议使用：

- 设备树中的 `reserved-memory`；或
- Linux DMA coherent/CMA 内存；或
- 模拟器中预留的 guest RAM 区域。

如果未来需要 64 位物理地址或 IOMMU，应在协议版本中增加，而不是改变已有寄存器含义。GPU 不应直接依赖宿主 SDL 指针；模拟器设备只看到 guest physical address，再通过 emulator bus 访问 guest RAM。

### 片上总线连接

真实 RTL 可以使用适合 SoC 的 AXI-Lite/AXI、Wishbone 或其他本地总线：

- 控制寄存器使用低带宽、可强序的寄存器总线。
- 命令环和 framebuffer 使用可突发的 memory master 端口。
- doorbell 写入只提交 tail/head，不携带完整 descriptor。
- vblank、fence、fault 和 DMA 完成进入统一 PIC。

模拟器不需要把 AXI channel 逐拍模拟出来；只需在设备接口层保留读写顺序、burst 可见性、DMA 范围检查和 IRQ 延迟参数。若将来要验证 RTL，再增加一个 AXI/总线适配器和协议 checker。

## Linux 驱动建议

### 设备树

建议让 GPU 作为普通 platform device 出现：

```dts
gpu@1f003000 {
    compatible = "stm32-in-c,vortex-lite";
    reg = <0x1f003000 0x1000>;
    interrupts = <5 1>;
    interrupt-parent = <&pic>;
    memory-region = <&gpu_reserved>;
    status = "okay";
};

gpu_reserved: gpu-memory@02000000 {
    compatible = "shared-dma-pool";
    reg = <0x02000000 0x00800000>;
    no-map;
};
```

实际 `interrupts` 格式要跟所实现的 PIC binding 一致。不要先写一个没有 Linux irqchip 驱动的自定义 interrupt-controller 节点，再期待通用内核自动处理。

### DRM/KMS 而不是新 fbdev ABI

如果目标只是让 LVGL 输出，可以做极简 framebuffer driver；但既然目标是可扩展开源 GPU，长期建议使用 DRM/KMS：

- `platform_driver` + `devm_platform_ioremap_resource()`
- `platform_get_irq()` + `devm_request_irq()`
- `drm_device`、`drm_driver`
- `drm_simple_display_pipe`
- `drm_gem_shmem_helper` 或合适的 DMA helper
- dumb buffer、format、stride、mmap、vblank
- `drm_fbdev_generic_setup()` 提供 framebuffer console 兼容层

这样当前 Linux framebuffer console、LVGL 和以后真正的 DRM userspace 可以共享同一套 buffer 模型。不要先设计一个只供 LVGL 使用的私有 ioctl；如果需要命令提交，优先使用 DRM GEM buffer 和受控的 private ioctl，后续再迁移到 DRM scheduler。

### 驱动 probe 顺序

驱动至少应分成这些阶段：

1. 读取并验证 `REG_ID`/`REG_VERSION`。
2. 复位设备，清理 pending IRQ 和 fault 状态。
3. 设置 DMA mask 和 coherent memory。
4. 分配命令环、fence/status 区和 scanout buffer。
5. 初始化 IRQ 与 vblank。
6. 注册 DRM device 和 connector/pipe。
7. 最后 enable device，避免设备在驱动尚未准备好时产生中断。

fault、hang 和 reset 路径必须从第一版就保留。GPU 命令错误不能让 Linux 永久等待 fence。

## 中断控制器与向量化

MIPS 没有 ARM NVIC 意义上的单一 NVIC。需要区分 CP0 的 `Cause.IP[7:0]`、外部 PIC 的 pending/priority，以及 MIPS32 的 VI/EIC 模式。

建议先实现一个外部优先级 PIC，再把它级联到一个 CP0 硬件中断线：

```text
UART ─┐
Timer ├─> external PIC ──> CP0 IP2
GPU  ─┘
```

PIC 至少需要：

- pending
- enable/mask
- level/edge trigger
- priority
- acknowledge/vector
- EOI
- 可选的 target CPU mask

第一阶段使用普通 CP0 interrupt compatibility mode：

```text
Cause.IP[2] = PIC.pending != 0
Status.IM[2] = enable PIC
```

当前 `registers.h` 已经包含 `Cause.IV`、`IntCtl`、`SRSCtl`、`Config3` 等字段，但 `exception.c` 仍主要使用 `EBase + 0x200` 的统一中断入口。建议顺序是：

1. 外部 PIC 和 Linux irqchip 驱动。
2. 设备级 priority/EOI/level-trigger 语义。
3. `Cause.IV`、`IntCtl.VS`、`EBase` 的 VI 向量计算。
4. EIC 模式、shadow register set 和真正的嵌套中断。

Linux 通常可以在异常入口保存上下文后重新打开中断，因此不必一开始就模拟所有硬件 shadow register 行为。先保证 PIC 的优先级、屏蔽、ack 和 EOI 正确。

## 多核心、MT ASE 和 SmartMIPS

建议把单一 `Registers` 拆成每个虚拟 CPU 独立的上下文：

```c
struct mips_cpu {
    Registers regs;
    uint32_t cpu_id;
    uint32_t pending_ipi;
    uint64_t local_cycles;
};

struct mips_machine {
    struct mips_cpu *cpus;
    unsigned cpu_count;
    struct bus bus;
    struct irq_controller irq;
    struct shared_memory memory;
};
```

每核独立 GPR、CP0、Count/Compare、TLB、EBase 和 pending interrupt；RAM、外部设备、PIC 和 IPI 控制器共享。

第一版使用确定性轮询：

```text
CPU0 执行 N 条指令
CPU1 执行 N 条指令
处理设备事件和中断
重复
```

确定性模式应该永久保留，用于测试和故障复现。确认 Linux SMP 和 IPI 正确后，再增加 parallel mode，把不同 vCPU 放到不同宿主线程。

MT ASE 与多核不同：MT ASE 是一个物理核心中的多个 VPE/TC，需要建模共享 core 资源、VPE 状态和 TC 上下文；不能只复制 `Registers` 并声称支持 MT。建议先实现独立 SMP vCPU，再实现 VPE/TC 调度。

SmartMIPS 也应独立处理：

- 在 `Config3` 中报告真实支持的能力。
- 未实现的 SmartMIPS 指令触发 Reserved Instruction。
- 按目标工具链实际生成的指令逐条补齐。
- 每条新增指令都增加编码、异常和结果测试。

对 Linux/LVGL 目标而言，SmartMIPS 优先级低于 RAM fast path、basic-block cache、PIC、IPI 和多核启动。

## 宿主线程和 affinity 方案

建议提供两种模式：

### Deterministic mode

- 一个 scheduler 线程。
- 所有 vCPU 轮询运行。
- 设备 tick 和 IRQ 在固定边界处理。
- SDL 独立线程只负责窗口和帧显示。

这是默认模式，也是所有单元测试和 Linux 启动回归的基线。

### Parallel mode

- 每个 vCPU 可以绑定一个宿主线程。
- 共享 RAM 使用明确的原子/屏障语义。
- PIC 使用 per-CPU pending 位和唤醒机制。
- 设备线程只处理异步设备事件。
- SDL 线程使用 frame mailbox。

不要一开始就使用 `SCHED_FIFO`。线程 affinity 只应在 profile 证明有收益后启用，并确保宿主物理核心足够，避免 vCPU、设备线程和 SDL 线程互相抢占。跨线程总线请求不应成为每条指令的必经路径。

## 推荐实施顺序

```text
1. SDL 独立线程、frame mailbox、手动帧率限制
2. RAM fast path、MMIO 设备区域表、设备 reset/tick 接口
3. 外部 PIC、timer/UART/GPU IRQ、Linux irqchip binding
4. Vortex 或 Nyuzi 命令环、scanout、fault/reset 和最小 DRM/KMS 驱动
5. 确定性双核、IPI、每核 timer 和 Linux SMP 启动
6. basic-block/threaded interpreter，减少函数指针和逐指令通用路径
7. parallel vCPU、host affinity 和共享内存同步
8. MT ASE、VI/EIC、shadow register set
9. SmartMIPS 和更完整的 GPU 命令/调度模型
```

每个阶段都应该有可测量的验收条件：

- emulator instructions/s
- SDL frame latency 和 present 阻塞时间
- MMIO read/write 延迟
- IRQ 到 guest handler 的 cycle 数
- GPU command ring 的吞吐量
- Linux driver reset/fault 后能否恢复
- deterministic mode 下是否可以重放同一输入得到相同 trace

核心原则是：GPU 线程可以独立，设备事件可以消息化，但 RAM 和高频同步 MMIO 必须保持低延迟；Vortex/Nyuzi 的第一版应是一个可验证的片上加速设备，而不是一次性复刻完整商业 GPU 软件栈。

## 参考项目

- Vortex GPGPU：<https://github.com/vortexgpgpu/vortex>
- Nyuzi Processor：<https://github.com/jbush001/NyuziProcessor>

这些链接用于核对上游接口和许可证；实现时应固定具体 commit，并为本项目的 command ring、内存顺序和 IRQ 行为维护独立的兼容性测试。
