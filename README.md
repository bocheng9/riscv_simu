# RVSIMU_V3

RISC-V RV32IMA 指令集模拟器（C++17），支持 Zicsr、Zifencei、C 扩展与 M/S/U 三特权级。

## 支持的指令集

| 扩展 | 说明 |
|---|---|
| RV32I | 基础整数指令 |
| RV32M | 乘除法 |
| RV32A | 原子操作（LR/SC、AMO） |
| Zicsr | CSR 读写指令 |
| Zifencei | 指令流同步 |
| **C（RVC）** | 16 位压缩指令，展开为等价 32 位指令执行 |

## 体系结构

| 项目 | 说明 |
|---|---|
| 特权级 | M / S / U 三级 |
| 异常委托 | `medeleg` / `mideleg`，支持 Vectored 模式（仅中断偏移） |
| 地址翻译 | Sv32（两级页表），4KB / 4MB 页 |
| 中断 | 机器模式定时器中断，`mip` / `mie` 驱动 |
| 非对齐访存 | 硬件拆分为多次访问 |
| 取指 | 支持 16 位压缩指令，PC 按实际指令长度推进 |
| 计数器 | `mcycle` / `minstret` / `time` 及高位 CSR |

## 已实现的 CSR

**机器级**：`mstatus` `misa` `medeleg` `mideleg` `mie` `mtvec` `mscratch` `mepc` `mcause` `mtval` `mip` `mcycle` `minstret` `mhartid`

**监管级**：`sstatus` `stvec` `sscratch` `sepc` `scause` `stval` `satp`

**只读计数**：`cycle` `time` `instret` 及各自高位

**物理内存保护**：`pmpcfg0-3` `pmpaddr0-15`（仅寄存器存储，不含访存拦截）

**调试触发器**：`tdata0-31`（0x7A0-0x7BF，仅寄存器存储）

## 目录结构

```
.
├── main.cpp        入口：ISA 测试模式 / Linux 启动模式
├── cpu.cpp/.h      处理器核心：取指、译码、执行、异常、CSR
├── rvc.cpp/.h      RVC 压缩指令展开
├── bus.cpp/.h      总线与内存
├── dev.cpp/.h      UART 16550、CLINT 定时器
└── utils.cpp/.h    ELF 加载与符号解析
```

## 构建

```bash
g++ -std=c++17 -O2 -o v3sim main.cpp cpu.cpp bus.cpp utils.cpp dev.cpp rvc.cpp
```

或使用 CMake：

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j4
```

## 运行

### ISA 测试模式

```bash
./v3sim <elf> [--trace] [--no-isa] [--max N]
```

| 参数 | 说明 |
|---|---|
| `<elf>` | RISC-V ELF 测试程序 |
| `--trace` | 逐条打印指令执行轨迹 |
| `--no-isa` | 不检查 `tohost`，按周期上限运行 |
| `--max N` | 周期上限 |

测例通过 `tohost` 写 `1` 表示成功。

### Linux 启动模式

```bash
./v3sim --linux --payload <固件镜像> --dtb <设备树> [--max N] [--trace]
```

| 参数 | 说明 |
|---|---|
| `--linux` | 进入 Linux 启动模式 |
| `--payload` | OpenSBI + 内核镜像，默认 `fw_payload_buildroot.bin` |
| `--dtb` | 设备树，默认 `virt.dtb` |

固件加载到 `0x80000000`，设备树加载到 `0x87E00000`，启动时
`a0 = hartid`、`a1 = 设备树物理地址`。

## 内存映射

| 区域 | 基址 | 大小 |
|---|---|---|
| DDR | `0x8000_0000` | 256 MB |
| CLINT | `0x0200_0000` | 64 KB |
| UART16550 | `0x1000_0000` | 256 B |

## 测试轨迹比对

`cmp_trace_v2.py` 用于与参考实现逐条比对执行轨迹：

```bash
python3 cmp_trace_v2.py [用例名]
```

## 许可

本项目仅供学习与研究使用。
