# RVSIMU_V3

RISC-V RV32IMA 指令集模拟器（C++17），支持 Zicsr、Zifencei 扩展与 M/S/U 三特权级。

## 支持的指令集

| 扩展 | 说明 |
|---|---|
| RV32I | 基础整数指令 |
| RV32M | 乘除法 |
| RV32A | 原子操作（LR/SC、AMO） |
| Zicsr | CSR 读写指令 |
| Zifencei | 指令流同步 |

## 体系结构

- 五级语义顺序执行：取指、译码、执行、访存、写回
- 三特权级：M / S / U，支持 mret、sret 的完整上下文切换
- 异常委托：medeleg / mideleg，trap 按委托位决定进入 M 模式或 S 模式
- Sv32 分页：两级页表、TLB、A/D 位软件更新
- 中断：软件/定时器/外部中断，支持 Direct 与 Vectored 两种 tvec 模式
- 非对齐访问：load/store 跨字拆分为两段

## 已实现的 CSR

mstatus、misa、medeleg、mideleg、mie、mtvec、mcounteren、mscratch、mepc、
mcause、mtval、mip、mcycle、minstret、mvendorid、marchid、mimpid、mhartid、
sstatus、sie、stvec、sscratch、sepc、scause、stval、satp

## 目录结构

```
main.cpp              程序入口：ELF 载入、执行循环、结束判定
cpu.h / cpu.cpp       指令执行、CSR、异常与中断、地址翻译
bus.h / bus.cpp       内存总线与设备映射
utils.h / utils.cpp   ELF 解析、符号查找
CMakeLists.txt        CMake 构建配置
```

## 构建

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

或直接编译：

```bash
g++ -std=c++17 -O2 -o rvsimu main.cpp cpu.cpp bus.cpp utils.cpp
```

## 运行

```bash
./rvsimu <elf文件>
```

## 命令行选项

| 选项 | 说明 |
|---|---|
| `--trace` | 输出逐条指令执行轨迹 |
| `--no-isa` | 不进行 ISA 测试判定，仅执行 |
| `--max N` | 设置执行周期上限 |
| `--quiet` | 仅输出最终结果 |

## 内存模型

| 地址范围 | 说明 |
|---|---|
| 0x8000_0000 起 1MB | DRAM，程序载入与执行区 |

## 许可

本项目为个人学习与验证用途。RISC-V 是 RISC-V International 的注册商标。
