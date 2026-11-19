# RVSIMU_V3 — RISC-V RV32IMA 模拟器（C++）

以 Python 版 RVSIMU_V2 为参考模型，通过 **RISC-V 官方 ISA 测试集**验证，
要求 **执行轨迹逐条一致**（PC + 指令码）。

## 当前状态

| 项 | 结果 |
|---|---|
| ISA 用例回归 | **66/66 PASS** |
| 与 V2 轨迹比对 | **66/66 完全一致** |

## 构建与运行

```bash
# 构建
g++ -std=c++17 -O2 -o v3sim main.cpp cpu.cpp bus.cpp utils.cpp

# 跑单个用例
./v3sim isa_test/rv32ui-p-add              # 判定 PASS/FAIL
./v3sim isa_test/rv32ui-p-add --trace      # 带指令轨迹
./v3sim isa_test/rv32ui-p-add --no-isa     # 只跑不判（用于轨迹对拍）
./v3sim isa_test/rv32ui-p-add --max 5000   # 周期上限
```

## 回归测试

```bash
# ISA 用例全量回归
./run_isa_v3.sh

# 与 Python V2 逐条轨迹比对（需 ../RVSIMU_V2）
python3 cmp_trace_v2.py
python3 cmp_trace_v2.py rv32ui-p-add      # 单用例
V2_DIR=/path/to/RVSIMU_V2 python3 cmp_trace_v2.py
```

## 目录

```
main.cpp            入口：ELF 载入、运行循环、tohost 判定
cpu.h / cpu.cpp     五级语义的指令执行、CSR、异常委托、MMU
bus.h / bus.cpp     地址映射与设备
utils.h / utils.cpp ELF 解析、tohost 符号查找
isa_test/           66 个 ISA 用例（ELF）
run_isa_v3.sh       批量回归脚本
cmp_trace_v2.py     与 V2 轨迹比对脚本
```

## 已实现特性

- **RV32IMA**：基础整数、乘除、原子操作（LR/SC、AMO）
- **M/S/U 三特权级**：`mret` / `sret` 的完整上下文切换
- **异常委托**：`medeleg` / `mideleg`，trap 按委托位决定进 M 还是 S 模式
- **Sv32 MMU**：两级页表、TLB、A/D 位软件更新（缺 A/D 触发 page fault）
- **CSR**：mstatus / mie / mtvec / mepc / mcause / mtval / medeleg / mideleg /
  sstatus / sie / stvec / sepc / scause / stval / satp / sscratch 等
- **取指地址对齐检查**：非 4 字节对齐触发 `cause=0`

## 与 V2 的已知差异（非 bug）

V2 的部分指令没有打印 trace（`csrrci` / `fence` / `fence.i` / `wfi` / `sfence.vma`），
已在 V2 侧补齐，两边格式现已统一。
