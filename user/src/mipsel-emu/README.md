# MIPS32EL emulator

这是一个 MIPS32 Release 2 小端模拟器。POSIX 前端可在 Linux PC 上提供 TUI、
trace 和文件 loader；不依赖 POSIX 的 `mipsel_emu_core` 则可作为静态库由
`arm-none-eabi-gcc` 编译，并嵌入 MCU 固件。本目录使用独立 CMake 工程，避免把
ncurses、`poll.h`、宿主 stdio 或固件 linker script 混入另一侧构建。

## 宿主构建

需要 CMake 3.22 以上和宿主 C11 编译器。TUI 默认启用，Linux 上还需要
`ncursesw` 开发包；Debian/Ubuntu 对应 `libncursesw5-dev`，Arch Linux 对应
`ncurses`。生成 guest DTB 另需 `dtc`（Debian/Ubuntu 包名
`device-tree-compiler`）。

从仓库根目录执行：

```sh
cmake -S user/src/mipsel-emu -B build/mipsel-emu \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build build/mipsel-emu
ctest --test-dir build/mipsel-emu --output-on-failure
```

若宿主没有 ncursesw，可构建无 TUI 版本：

```sh
cmake -S user/src/mipsel-emu -B build/mipsel-emu-headless \
  -DMIPSEL_EMU_ENABLE_TUI=OFF
cmake --build build/mipsel-emu-headless
```

## 嵌入式静态库

模块专用工具链默认生成 Cortex-M0 Thumb/soft-float 静态库，不构建 POSIX 前端或
宿主测试，也不指定 startup、linker script、nano/nosys specs：

```sh
cmake -S user/src/mipsel-emu -B build/mipsel-emu-arm-m0 \
  -DCMAKE_TOOLCHAIN_FILE="$PWD/user/src/mipsel-emu/cmake/arm-none-eabi.cmake" \
  -DCMAKE_BUILD_TYPE=MinSizeRel
cmake --build build/mipsel-emu-arm-m0 --target mipsel_emu_core
arm-none-eabi-size build/mipsel-emu-arm-m0/libmipsel_emu_core.a
```

可用 `-DMIPSEL_EMU_ARM_CPU=cortex-m4` 更换 CPU。若最终固件使用 Cortex-M4F
硬浮点，库、startup、USB 栈以及其他所有对象必须采用相同 ABI，例如同时传入：

```text
-DMIPSEL_EMU_ARM_CPU=cortex-m4
-DMIPSEL_EMU_ARM_FLOAT_ABI=hard
-DMIPSEL_EMU_ARM_FPU=fpv4-sp-d16
```

作为固件子目录集成时，链接目标为 `mipsel-emu::embedded`：

```cmake
add_subdirectory(user/src/mipsel-emu build/mipsel-emu-core)
target_link_libraries(board_firmware PRIVATE mipsel-emu::embedded)
```

公共入口是 `mipsel_emu.h`。`config.h` 中的默认值均可由 `-D` 覆盖，也可以让板级
工程提供一个配置头。配置会改变地址和公开结构大小，因此必须同时作用于库及其
调用方：

```cmake
set(MIPSEL_EMU_USER_CONFIG_HEADER board_mipsel_emu_config.h
    CACHE STRING "mipsel-emu board config")
set(MIPSEL_EMU_USER_CONFIG_INCLUDE_DIR
    "${CMAKE_CURRENT_LIST_DIR}/board/include"
    CACHE PATH "mipsel-emu board include directory")
add_subdirectory(user/src/mipsel-emu build/mipsel-emu-core)
```

这两个变量必须在 `add_subdirectory()` 之前设置，使 archive 编译和安装配置探测使用
同一板头；不要在子目录返回后再单独修改 `mipsel_emu_core` 的相关 compile definitions。

执行 `cmake --install` 时会生成自包含的安装版 `config.h`：feature flags 与所有影响
公开结构/平台行为的数值配置都会展开并锁定为 archive 的实际构建值，不再依赖原板级
配置头。安装包同时提供可由 `find_package(mipsel_emu CONFIG REQUIRED)` 导入的
`mipsel-emu::core` target；消费者若用 `-D` 指定不匹配值，或再次设置
`MIPSEL_EMU_USER_CONFIG_HEADER`，会在编译期报错，避免 ABI 漂移。

典型板级配置可覆盖 `MIPSEL_EMU_RAM_SIZE`、UART 地址/IRQ/时钟、RX/TX FIFO 大小、
`MIPSEL_EMU_IMAGE_CHUNK_SIZE`、ELF 最大段数，以及 Monitor 的
`MIPSEL_EMU_CONSOLE_LINE_SIZE`/`MIPSEL_EMU_CONSOLE_OUTPUT_SIZE` 固定缓冲区大小。
`MIPSEL_EMU_ENABLE_UART16550`、`MIPSEL_EMU_ENABLE_ELF_LOADER`、
`MIPSEL_EMU_ENABLE_INITRAMFS`、`MIPSEL_EMU_ENABLE_CONSOLE` 对应同名 CMake 选项；
裁剪前应先确认固件使用的启动包和功能不再需要相关引擎。

### PSRAM 后端

`platform_memory_*` 是绕过 MMIO 的原始物理内存接口。MCU 应在加载镜像或运行 CPU
前安装同步完成的 bulk callbacks；CPU 总线会自行处理小端拆装和 UART MMIO：

```c
#include "mipsel_emu.h"

static bool psram_read(void *opaque, uint32_t pa, void *dst, size_t len)
{
    struct psram_cache *cache = opaque;
    return psram_cache_read(cache, pa, dst, len);
}

static bool psram_write(void *opaque, uint32_t pa,
                        const void *src, size_t len)
{
    struct psram_cache *cache = opaque;
    return psram_cache_write(cache, pa, src, len);
}

static bool psram_fill(void *opaque, uint32_t pa, uint8_t value, size_t len)
{
    struct psram_cache *cache = opaque;
    return psram_cache_fill(cache, pa, value, len);
}

static const platform_memory_ops_t memory_ops = {
    .read = psram_read,
    .write = psram_write,
    .fill = psram_fill,
};

static bool guest_memory_init(struct psram_cache *psram)
{
    if (!platform_memory_configure(&memory_ops, psram,
                                   MIPSEL_EMU_RAM_SIZE)) {
        return false;
    }
    platform_init(NULL, NULL);
    return true;
}
```

`read`/`write` 必须完整传输请求范围后才返回 `true`。`fill` 可留空，但默认 fallback
会拆成 32 字节写入；SPI PSRAM 应实现 bulk fill，并在 backend 内使用 cache、连续
传输和 DMA。若 DMA 与 CPU cache 同时访问 PSRAM，还必须由板级代码完成一致性处理。
直接可寻址的 PC RAM 可改用 `platform_memory_bind()`。

SPI Flash 中的 ELF/DTB/initramfs 通过 `mipsel_image_t.read` 提供随机读取。
`mipsel_elf_load()` 在写 RAM 前先完整校验所有 `PT_LOAD`，随后流式复制并清 BSS；
`mipsel_dtb_load()` 和 `mipsel_initramfs_load()` 使用同一物理内存 backend。加载完成后
设置 CPU 入口/UHI 参数，再按较小时间片调用 `mipsel_emu_run_steps()`，以便及时服务
USB、SPI 和 watchdog。若量产镜像已离线打包成固定 load segments，可关闭通用 ELF
loader 并由板级启动包直接调用 bulk memory API。

### 暂停目标 Monitor（TUI/CDC0 共用）

`console.c` 是不依赖 ncurses、stdio、malloc 或 POSIX 的命令执行层。它不拥有传输，
而是通过 `halted`、物理总线和输出 callback 连接具体平台；同一套 parser 因而可以由
桌面 TUI 和 USB CDC0 共用。每条非空命令在 dispatch 前都会再次检查 `halted`，调用方
仍必须保证 Monitor 与 CPU 执行串行，不能在另一个中断或 RTOS task 正在修改
`Registers` 时调用它。

数值接受十进制或带 `0x` 前缀的十六进制。当前命令如下：

| 命令 | 行为 |
| --- | --- |
| `tlb [index]` | 打印一个或全部 64 个 TLB 条目的原始 EntryHi/Lo0/Lo1/PageMask |
| `translate <va>` | 使用当前 Status、ASID 和 TLB 执行与 CPU 相同的地址翻译 |
| `reg [name [value]]` | 打印全部寄存器，或读取/修改一个寄存器 |
| `mrb`/`mrh`/`mrw <pa>` | 从物理总线读取 8/16/32 位值 |
| `mwb`/`mwh`/`mww <pa> <value>` | 向物理总线写入 8/16/32 位值 |
| `disasm [word]` | 在当前 PC 上反汇编给定机器码；省略机器码时翻译 PC 并读取当前指令 |

`reg` 接受 ABI 名（如 `$t0`/`t0`）、`r0`..`r31`、数字索引、`pc`、`next_pc`、
`hi`、`lo`、常用 CP0 名，以及 `cp0.<reg>.<sel>`。`$zero` 不可写；修改 `pc` 会同步
`next_pc=pc+4` 并清除未完成的分支、delay slot 和异常流水线状态。物理总线命令会
访问 RAM 或 MMIO；读取 RBR 等设备寄存器可能消费数据，写入也会产生真实设备副作用。
CP0 写入复用 guest MTC0 规则，例如 Cause 只修改可写位，写 Compare 会清 TI/IP7。

TUI 默认在第一条指令前暂停。CPU 精确处于 `PAUSED` 时按 F3 或 `:` 进入 Monitor，
Esc/F3 返回主界面，PgUp/PgDn 或 Up/Down 滚动输出；运行中请求进入只会响铃。Monitor
拥有独立输入和 scrollback，其中的 Space、`q`、`s` 等都是命令字符，不会触发全局
快捷键，也不会进入 guest UART。先退出 Monitor，再按 Space 可继续执行。
若终端缩到无法绘制窗口，Monitor 会保留 focus 并忽略盲输字符，恢复尺寸后可继续；若
target 被外部状态切换恢复运行，Monitor 会清除未提交输入并锁定到 Esc/F3 显式退出。
若当前处于 UART input focus，应先按 F2 或 Ctrl-] 离开，再进入 Monitor。

嵌入式 CDC0 可直接使用流式接口；下面的 USB queue 函数由板级工程提供：

```c
#include "mipsel_emu.h"

struct board_monitor {
    Registers *cpu;
    bool target_paused;
    mipsel_console_t console;
};

static bool console_halted(void *opaque)
{
    const struct board_monitor *monitor = opaque;
    return monitor->target_paused;
}

static bool console_bus_read(void *opaque, uint32_t pa,
                             unsigned width, uint32_t *value)
{
    (void)opaque;
    return platform_bus_read(pa, width, value);
}

static bool console_bus_write(void *opaque, uint32_t pa,
                              unsigned width, uint32_t value)
{
    (void)opaque;
    return platform_bus_write(pa, width, value);
}

static void console_output(void *opaque, const char *bytes, size_t length)
{
    (void)opaque;
    cdc0_tx_queue_copy(bytes, length); /* 返回前必须复制全部字节 */
}

static bool board_monitor_init(struct board_monitor *monitor, Registers *cpu)
{
    bool initialized;

    monitor->cpu = cpu;
    monitor->target_paused = false;
    initialized = mipsel_console_init(&monitor->console,
        &(mipsel_console_config_t) {
            .registers = cpu,
            .halted = console_halted,
            .bus_read = console_bus_read,
            .bus_write = console_bus_write,
            .target_opaque = monitor,
            .output = console_output,
            .output_opaque = monitor,
            .flags = MIPSEL_CONSOLE_FLAG_ECHO |
                     MIPSEL_CONSOLE_FLAG_PROMPT,
        });
    if (initialized)
        mipsel_console_prompt(&monitor->console);
    return initialized;
}

/* 与 CPU 状态切换位于同一 task；离开暂停态时丢弃未提交的半行。 */
static void board_monitor_set_paused(struct board_monitor *monitor,
                                     bool paused)
{
    monitor->target_paused = paused;
    if (!paused)
        mipsel_console_cancel_input(&monitor->console);
}

/* 与 CPU time slice 位于同一 task；USB ISR/DMA completion 只更新队列。 */
static void cdc0_monitor_service(struct board_monitor *monitor)
{
    uint8_t bytes[64];
    size_t length;

    while ((length = cdc0_rx_queue_pop(bytes, sizeof(bytes))) != 0) {
        if (monitor->target_paused)
            (void)mipsel_console_feed(&monitor->console, bytes, length);
    }
}
```

`mipsel_console_feed()` 处理拆包、多个命令同包、CR/LF/CRLF、Backspace/Delete、
Ctrl-U 和超长行恢复。它以及 output callback 都是同步接口：callback 返回前必须消费
或复制全部输出；特别是无参数 `tlb` 会连续输出 64 行，CDC0 TX queue 必须有足够空间
或实现可靠的同步背压。USB ISR/DMA completion 应只投递 RX/TX 事件，主循环在有界 CPU
时间片之间串行处理这些事件。CPU 运行时应丢弃或拒绝 CDC0 RX，并在离开暂停态时取消
尚未提交的半行，避免旧输入在下一次暂停后执行；即使板级门禁失效，`halted` callback
仍会阻止一条完整命令在运行态 dispatch。

### 双 CDC 与 guest UART

一组 USB D+/D- 可枚举为包含两个 CDC ACM function 的 composite device。CDC0 连接
上述暂停目标 Monitor；CDC1 专门桥接 guest 16550A。两路数据不能复用同一 RX/TX
queue。CDC1 使用 platform-owned UART span API，初始化时必须给 `platform_init()` 传
`NULL` TX callback，否则桌面兼容 callback 会同步抽干 TX FIFO。

CDC1 OUT（host 到 guest）可直接把 USB DMA 指向 `platform_uart_rx_reserve()` 返回的
连续空间；完成后在与模拟器执行串行化的位置调用
`platform_uart_rx_produce(actual_length)`。CDC1 IN（guest 到 host）先用
`platform_uart_tx_peek()` 取得连续只读 span，只有在 USB 已复制数据或 DMA 完成后才
调用 `platform_uart_tx_consume(actual_length)`。队列为空/满时 span 长度为零，这是
正常背压：TX 满会让 guest 看到 THRE 清零，不能静默丢弃字节。

```c
/* Called between bounded emulator time slices, not concurrently with MMIO. */
void guest_uart_service(void)
{
    uint8_t *rx;
    const uint8_t *tx;
    size_t length;

    if (usb_cdc1_out_idle()) {
        length = platform_uart_rx_reserve(&rx);
        if (length)
            usb_cdc1_start_out_dma(rx, length);
    }

    if (usb_cdc1_in_idle()) {
        length = platform_uart_tx_peek(&tx);
        if (length)
            usb_cdc1_start_in_dma(tx, length);
    }
}

void usb_cdc1_out_complete(size_t received)
{
    (void)platform_uart_rx_produce(received);
}

void usb_cdc1_in_complete(size_t transmitted)
{
    (void)platform_uart_tx_consume(transmitted);
}
```

这些 span 是单生产者/单消费者协议，库内不使用 atomics，也不替板级代码关中断。
USB completion 与 guest MMIO 必须在同一任务中处理，或由板级临界区/DMB 保证顺序。
`produce(0)`/`consume(0)` 可取消预约；调用 `platform_reset()` 前必须先停止 DMA，因为
reset 会使未完成 span 失效。若 USB 栈只能交付自己的接收 buffer，可逐字节调用
`platform_uart_receive()`，或复制到 RX span 后一次 publish。

## 设备树和 UHI 启动约定

设备树描述 16 MiB RAM，以及位于物理地址 `0x1f000900` 的 16550A UART。
UART 寄存器宽度为 32 位、间隔为 4 字节（`reg-io-width = 4`、
`reg-shift = 2`），输入时钟为 14.7456 MHz，连接到 CPU 硬件中断 4。

安装了 `dtc` 时，CMake 会提供 DTB 目标：

```sh
cmake --build build/mipsel-emu --target mipsel-emu-dtb
```

也可以直接生成：

```sh
dtc -I dts -O dtb \
  -o build/mipsel-emu/mipsel-emu.dtb \
  user/src/mipsel-emu/linux/mipsel-emu.dts
```

loader 将 DTB 复制到物理地址 `0x00010000`，并按 MIPS UHI 协议进入内核：

- `a0 = 0xfffffffe`（有符号值 `-2`，表示 UHI/FDT 启动）；
- `a1 = 0x80010000`（DTB 的 KSEG0 地址）；
- `a2 = a3 = 0`。

设备树的 `chosen` 节点使用 `earlycon` 和 `stdout-path`。这里特意使用不带显式
MMIO 参数的 `earlycon`，让内核从 DTB 获取 `reg-shift`、访问宽度和 UART 时钟。

## Linux 配置与构建

`linux/emu.config` 是针对现有 MIPS generic/UHI 配置的增量 fragment，打开
printk、TTY、8250 console、OF probing、外部 initramfs、静态 BusyBox ELF/script
执行及常用伪文件系统。以下命令保留已有的 `.config` 并合入 fragment：

```sh
repo_root=$PWD
linux_dir="$repo_root/linux-7.1.4"
cross_prefix="$repo_root/mips32el--musl--stable-2025.08-1/bin/mipsel-buildroot-linux-musl-"

"$linux_dir/scripts/kconfig/merge_config.sh" -m -O "$linux_dir" \
  "$linux_dir/.config" \
  "$repo_root/user/src/mipsel-emu/linux/emu.config"
make -C "$linux_dir" ARCH=mips CROSS_COMPILE="$cross_prefix" olddefconfig
make -C "$linux_dir" ARCH=mips CROSS_COMPILE="$cross_prefix" -j"$(nproc)" vmlinuz
```

全新配置可先执行 `make ... 32r2el_defconfig`，再合入 fragment。该 defconfig
启用的功能较多，最终应检查 ELF 的所有 `PT_LOAD` 段和 BSS 是否仍能放入
16 MiB guest RAM。仓库内的 musl 工具链使用 O32 hard-float ABI，而模拟器没有
实现 CP1，因此 `CONFIG_MIPS_FP_SUPPORT=y` 不能裁掉：Linux 会通过
`arch/mips/math-emu` 模拟用户态浮点指令；否则 BusyBox 会收到 `SIGILL`。

fragment 只保留 gzip initramfs 解压器；未压缩的 `newc` cpio 也可直接使用。仓库
根目录的 `busybox-1.38.0/` 可通过以下脚本构建为静态 BusyBox 和最小 initramfs：

```sh
user/src/mipsel-emu/linux/build-initramfs.sh
```

脚本在临时目录中构建，不会修改 BusyBox 原有 `.config`；它强制静态链接、检查 ELF
中没有 `PT_INTERP`，并通过 Linux 的 `gen_init_cpio` 无特权创建 `/dev/console`。
默认产物是 `build/mipsel-emu-rootfs/initramfs.cpio.gz`。可用 `BUSYBOX_DIR`、
`LINUX_DIR`、`KERNEL_CONFIG`、`CROSS_COMPILE`、`OUTPUT_DIR` 和 `JOBS` 覆盖对应路径或
并行度。脚本还会验证实际 kernel `.config` 已为 hard-float BusyBox 启用软件 FPU，
并检查 `/init` 依赖的 ash、mount、setsid 和 cttyhack 等 applet。

因为根文件系统本身就是 initramfs，`CONFIG_DEVTMPFS_MOUNT` 不会替 PID 1 自动挂载
devtmpfs；随附 `/init` 会挂载 devtmpfs、proc、sysfs 和 devpts，再由 `cttyhack` 在
`ttyS0` 上启动具有 controlling tty 和 job control 的交互式 shell；PID 1 会在 shell
退出后重新启动它。修改 fragment 后必须重新运行 `olddefconfig` 并重建内核。

## 运行

CLI 形式为：

```text
mipsel-emu [--kernel FILE] [--dtb FILE] [--tui] [--run]
            [--initramfs FILE] [--trace FILE] [--max-steps N]
```

`--kernel` 也可以写成第一个位置参数，默认值为当前目录下的 `./vmlinuz`。
`--tui` 默认暂停在第一条指令；同时给出 `--run` 可立即连续执行。

从仓库根目录启动新构建的内核、DTB 和 rootfs：

```sh
build/mipsel-emu/mipsel-emu \
  --kernel linux-7.1.4/vmlinux \
  --dtb build/mipsel-emu/mipsel-emu.dtb \
  --initramfs build/mipsel-emu-rootfs/initramfs.cpio.gz
```

启动完成后会出现 `mipsel-emu #`，此时 `/bin/sh` 是连接到 `/dev/ttyS0` 的 BusyBox
ash。需要寄存器和 debug windows 时，可在命令末尾添加 `--tui --run`，并用
`--trace build/mipsel-emu/kernel.trace` 把完整指令/异常 trace 写到独立文件。

`--initramfs` 必须与 `--dtb` 一起使用。loader 将 archive 放在 RAM 顶部附近、避开
ELF 和 DTB，并修改 `/chosen/linux,initrd-start` 与 `linux,initrd-end`；随仓库提供的
DTS 已预留这两个定宽属性。内核、BSS、DTB、initramfs 和运行时页分配仍共享
`MIPSEL_EMU_RAM_SIZE`，16 MiB 配置下应控制 BusyBox 和 archive 大小。

不加 `--tui` 时使用 headless console；可配合 `--max-steps` 做有界的回归运行。
UART 输出在 TUI 模式进入 console window，headless 模式写入宿主 stdout，指令
与异常 trace 不应与 UART 字节流共用 stdout。headless 模式也会把 stdin 送入
UART；连接终端时使用原始输入，`Ctrl-] q` 退出模拟器，连续按两次 `Ctrl-]`
可向 guest 发送一个字面 `Ctrl-]`。TUI 中的 `r` 会清空 RAM、重新加载 ELF/DTB
并复位 CPU 和 UART。TUI 与 `--trace -` 不能共用终端，请指定独立 trace 文件。

## 裸机链接注意事项

静态库本身不使用 POSIX、stdio、malloc 或系统调用；它只要求 C runtime 提供
`memcpy`/`memset`。在 Cortex-M0 上，MIPS 的乘除法和 GCC 生成的 switch 还会引用
`__aeabi_idivmod`、`__aeabi_uidivmod`、`__aeabi_lmul`、
`__gnu_thumb1_case_uqi`，这些符号由 GCC multilib 中的 `libgcc` 提供。

最终固件应由 `arm-none-eabi-gcc` driver 链接，并保留匹配 CPU/float ABI 的
`libgcc`。可以用 `-nostartfiles` 替换默认 CRT startup，但不要用链接脚本
`/DISCARD/` 丢弃 `libgcc.a`，也不要在未显式补回 libc/libgcc 时使用 `-nostdlib`。
模块专用 toolchain 只负责编译 archive；芯片 startup、链接脚本、`nano.specs`、
USB 栈和 `_read/_write/_sbrk` 等 syscall stub 都属于最终 board firmware target。

## 当前范围

- 8250/16550 的基本寄存器、收发 FIFO 和 IRQ 已可用；接收 IRQ 当前在任意字节
  到达时立即置位，尚未模拟 FCR 阈值和字符超时的精确时序。
- 外部 initramfs、ELF BusyBox 和 DTB range patch 已可用；块设备和持久文件系统
  尚未实现，当前持久存储仍需后续加入 virtio-blk 或其他 MMIO 设备。
- CPU ISA、TLB 和平台设备仍在完善中。建议先用 `--max-steps` 做有界启动，并用
  TUI exception window 或 `--trace FILE` 定位尚未实现的 guest 行为。
