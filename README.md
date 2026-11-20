# RVSIMU_V3

A RISC-V RV32IMA instruction set simulator written in C++17, supporting
Zicsr, Zifencei, the C extension, and the M/S/U privilege levels.

## Supported ISA

| Extension | Description |
|---|---|
| RV32I | Base integer instruction set |
| RV32M | Integer multiplication and division |
| RV32A | Atomic operations (LR/SC, AMO) |
| Zicsr | CSR access instructions |
| Zifencei | Instruction-fetch fence |
| C | Compressed 16-bit instructions (RV32C) |

Privilege levels: M-mode, S-mode, U-mode.
Virtual memory: Sv32 page-based translation.
Exceptions and interrupts: full delegation via `medeleg`/`mideleg`,
`mstatus.MPP`/`SPP`, vectored and direct trap modes.

## Architecture

- Single-cycle instruction execution with an explicit trap mechanism
  (`Trap(cause, tval)`), so exceptions unwind directly to the handler.
- Instruction fetch detects 16-bit compressed encodings by inspecting the
  low two bits, expands them through `rvc_expand()` and reuses the 32-bit
  decoder. `inst_len` (2 or 4) follows each instruction through the pipeline
  so that return addresses and `pc` advance correctly.
- Memory accesses go through a `Bus` that dispatches to RAM or to MMIO
  devices by physical address.
- Devices are modelled behind a `Mmio` interface: a 16550A UART and a
  CLINT timer.

## CSRs

| Group | Registers |
|---|---|
| Machine | `mstatus`, `misa`, `mie`, `mip`, `mtvec`, `mepc`, `mcause`, `mtval`, `mscratch`, `medeleg`, `mideleg`, `mhartid`, `mvendorid`, `marchid`, `mimpid` |
| Supervisor | `sstatus`, `sie`, `sip`, `stvec`, `sepc`, `scause`, `stval`, `sscratch`, `satp` |
| Counters | `cycle`, `time`, `instret` and their `h` counterparts (read-only), plus `mcycle`, `minstret` and their `h` counterparts (writable) |
| PMP | `pmpcfg0`-`pmpcfg3`, `pmpaddr0`-`pmpaddr15` (storage only) |
| Debug | `tselect`, `tdata1`-`tdata3`, `dcsr`, `dpc` (storage only) |

## Building

```sh
g++ -std=c++17 -O2 -o v3sim \
    main.cpp cpu.cpp bus.cpp utils.cpp dev.cpp rvc.cpp
```

`CMakeLists.txt` is provided as an alternative.

## Running

### ISA test mode

```sh
./v3sim <elf> [--trace] [--no-isa] [--max <cycles>]
```

An ISA test program signals completion through a store to `tohost`.
Exit status 0 means PASS.

### Linux boot mode

```sh
./v3sim --linux --payload <fw_payload.bin> --dtb <virt.dtb> [--trace] [--max <cycles>]
```

`--payload` is an OpenSBI firmware image that bundles the kernel and an
initramfs; `--dtb` is the device tree passed to the firmware.

The boot context is set up per the OpenSBI convention: `a0 = hartid`,
`a1 = physical address of the DTB`.

## Memory map

| Region | Base | Size | Notes |
|---|---|---|---|
| DRAM | `0x8000_0000` | 128 MB | program image, DTB, page tables |
| CLINT | `0x0200_0000` | 64 KB | `mtimecmp` at `0x4000`, `mtime` at `0xBFF8` |
| UART 16550A | `0x1000_0000` | 8 bytes | THR/RBR, IER, IIR/FCR, LCR, MCR, LSR, MSR, SCR |
| DTB | `0x87E0_0000` | — | loaded from `--dtb` |

The UART models the full 16550A register set. `IIR` reports "no interrupt
pending" and `MSR` reports CTS/DSR/DCD asserted, which is what the Linux
8250 driver expects before it will transmit user-space data.

## Directory layout

```
main.cpp    entry point, command-line handling, the two run modes
cpu.h/.cpp  CPU core: fetch/decode/execute, CSR file, traps, counters
rvc.h/.cpp  RV32C compressed instruction expansion
bus.h/.cpp  address decoder and RAM
dev.h/.cpp  MMIO devices: 16550A UART and CLINT
utils.h/.cpp helpers for loading ELF images
```

## Linux boot

The simulator boots OpenSBI v1.4, the Linux 6.5 kernel and a Buildroot
initramfs built for a pure `rv32ima` target with the `ilp32` ABI. Kernel
initialisation completes, the UART console comes up, and `/init` runs in
user mode.

The two images needed to reproduce this are in `images/`:

```
images/fw_payload.bin   OpenSBI + kernel + initramfs
images/virt.dtb         device tree
```

Boot it with:

```sh
./v3sim --linux --payload images/fw_payload.bin --dtb images/virt.dtb
```
