# MIPS 指令分派：switch-case 实验

本实验分支 `experiment/switch-dispatch` 将原来的八张函数指针 LUT 替换为枚举 opcode
和嵌套 `switch`。`op.c` 被包含进 `emu.c`，所有指令和子解码函数使用
`__STATIC_FORCEINLINE`；因此编译器可将每个 case 的操作体直接展开，消除 LUT 读取和
分派调用边界。

这是一项空间优先的实验，不是默认主线实现。`op.h` 和 `op.c` 在该分支是内部实现，
不能作为外部 instruction-handler API 使用。

## 复现

RV32IMAC / CH32 profile：

```sh
cmake -S user/src/mipsel-emu -B build/dispatch-switch-ch32 \
  -DCMAKE_TOOLCHAIN_FILE="$PWD/user/src/mipsel-emu/cmake/riscv32-elf.cmake" \
  -DMIPSEL_EMU_USER_CONFIG_HEADER=board_mipsel_emu_config.h \
  -DMIPSEL_EMU_USER_CONFIG_INCLUDE_DIR="$PWD/user/src/mipsel-emu/boards/ch32v203f6p6" \
  -DCMAKE_BUILD_TYPE=MinSizeRel
cmake --build build/dispatch-switch-ch32 --target mipsel_emu_core -j2
riscv64-elf-objdump -d build/dispatch-switch-ch32/libmipsel_emu_core.a \
  | awk '/^[[:space:]]+[0-9a-f]+:/{n++} END {print n}'
```

主机吞吐基准使用一个 4 指令的 MIPS loop（`addiu`、`addu`、`beq`、delay-slot nop），
包含 `mipsel_emu_step()` 的取指、KSEG0 地址翻译、执行、分支提交和 Count 更新：

```sh
cmake -S user/src/mipsel-emu -B build/dispatch-switch-host \
  -DCMAKE_BUILD_TYPE=Release \
  -DMIPSEL_EMU_BUILD_DISPATCH_BENCHMARK=ON \
  -DMIPSEL_EMU_ENABLE_QT_GUI=OFF
cmake --build build/dispatch-switch-host --target mipsel_emu_dispatch_benchmark -j2
taskset -c 3 build/dispatch-switch-host/mipsel_emu_dispatch_benchmark 100000000
```

运行前应丢弃第一轮以避开 CPU frequency ramp；报告至少给出五轮预热后的中位数。

## 2026-09-17 对比

同一 GCC、`MinSizeRel` 和 RV32IMAC profile 下：

| 指标 | 函数指针 LUT | 强制内联 switch | 变化 |
| --- | ---: | ---: | ---: |
| RV32 archive 文件大小 | 70,768 B | 44,482 B | -26,286 B (-37.1%) |
| RV32 code/data/bss 求和 | 16,238 B | 14,418 B | -1,820 B (-11.2%) |
| RV32 反汇编指令数 | 5,118 | 4,658 | -460 (-9.0%) |
| RV32 LUT 数据 | 1,600 B | 0 B | -1,600 B |

在 12th Gen i7-1265U 的单个绑核上，连续执行 100M guest ticks 的五轮预热后中位数为：

| 指标 | LUT | 强制内联 switch | 变化 |
| --- | ---: | ---: | ---: |
| 吞吐 | 54.16 M ticks/s | 50.02 M ticks/s | -7.6% |
| `sizeof(Registers)` | 2,208 B | 2,208 B | 0 |
| dispatch LUT 常驻数据 | 3,200 B | 0 B | -3,200 B |
| host core archive | 145,428 B | 118,720 B | -18.4% |

主机值会受调频与系统负载影响；这里的吞吐差异应理解为量级结论，而非跨机器绝对值。
`Registers` 不因分派模型而改变，节省的是函数指针表及未再需要的独立 `op.c` 目标。

## 结论

对 32 KiB Flash 的 CH32 profile，这个方案有明显空间优势，且不改变 Linux 所需
MIPS32EL/MMU/TLB/CP0 语义。代价是 host 热路径约 5–10% 变慢，以及 unity-style
编译降低了逐指令函数的独立调试和插桩便利性。是否合入主线应由嵌入式 Flash 预算优先级
决定；若 PC 仿真吞吐优先，保留 LUT 更合适。
