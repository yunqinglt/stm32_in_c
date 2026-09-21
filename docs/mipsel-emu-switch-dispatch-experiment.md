# MIPS 指令分派：switch-case 实验

最初的 `experiment/switch-dispatch` 分支将原来的八张函数指针 LUT 替换为枚举 opcode
和嵌套 `switch`；该实现现已合入 Windows port。`op.c` 被包含进 `emu.c`，所有指令和子解码函数使用
`__STATIC_FORCEINLINE`；因此编译器可将每个 case 的操作体直接展开，消除 LUT 读取和
分派调用边界。

这是一个空间优先的实现选择。`op.h` 和 `op.c` 是内部实现，不能作为外部
instruction-handler API 使用；公共执行入口仍是 `execute_instr()`、
`mipsel_emu_step()` 和 `mipsel_emu_run_steps()`。

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
包含 `mipsel_emu_step()` 的取指、KSEG0 地址翻译、执行、分支提交和 Count 更新。它是
完整 step 热路径微基准，不是只测 `switch` 本身：

```sh
cmake -S user/src/mipsel-emu -B build/dispatch-switch-host \
  -DCMAKE_BUILD_TYPE=Release \
  -DMIPSEL_EMU_BUILD_DISPATCH_BENCHMARK=ON \
  -DMIPSEL_EMU_BUILD_HOST=OFF
cmake --build build/dispatch-switch-host --target mipsel_emu_dispatch_benchmark -j2
build/dispatch-switch-host/mipsel_emu_dispatch_benchmark 100000000 5 1000000
```

Windows/MSVC 是 multi-config 构建，需显式选择 Release：

```powershell
cmake -S user/src/mipsel-emu -B build/dispatch-switch-msvc `
  -G "Visual Studio 18 2026" -A x64 `
  -DMIPSEL_EMU_BUILD_DISPATCH_BENCHMARK=ON `
  -DMIPSEL_EMU_BUILD_HOST=OFF
cmake --build build/dispatch-switch-msvc --config Release `
  --target mipsel_emu_dispatch_benchmark --parallel 4
& .\build\dispatch-switch-msvc\Release\mipsel_emu_dispatch_benchmark.exe `
  100000000 5 1000000
```

程序在 Windows 使用 QPC，在其他 C11 主机使用 `timespec_get`；它先执行预热，再从
相同 guest 状态运行每个样本，输出全部样本和中位数。吞吐结果只有在 PC、延迟槽、
寄存器、Count 和异常状态自检通过后才会打印。本 workload 中一个 step 恰好退休一条
指令，但该等价关系不适用于一般 guest 的异常/中断路径。

## 2026-09-17 原始实验对比

以下数值来自合并前的 Linux/RV32 实验环境，用于保存设计依据，不代表当前 Windows
机器或当前编译器的结果。

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
MIPS32EL/MMU/TLB/CP0 语义。历史测量的代价是该 host 热路径约慢 5–10%，并且
unity-style 编译降低了逐指令函数的独立调试和插桩便利性。合入后仍应在目标编译器和
机器上用同一 benchmark 持续记录这一空间/吞吐取舍。
