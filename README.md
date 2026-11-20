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
    main.cpp cpu.cpp bus.cpp utils.cpp dev.cpp rvc.cpp plic.cpp
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
| PLIC | `0x0C00_0000` | 64 MB | platform interrupt controller, context 1 = hart0 S-mode |
| UART 16550A | `0x1000_0000` | 8 bytes | THR/RBR, IER, IIR/FCR, LCR, MCR, LSR, MSR, SCR |
| DTB | `0x87E0_0000` | — | loaded from `--dtb` |

The UART models the full 16550A register set. `IIR` reports "no interrupt
pending" and `MSR` reports CTS/DSR/DCD asserted, which is what the Linux
8250 driver expects before it will transmit user-space data.

Transmission is interrupt driven: the UART asserts its interrupt request
whenever the driver has enabled the THR-empty interrupt, and the PLIC
forwards that to the S-mode external interrupt of the CPU. Without the
interrupt the transmit path stalls once one FIFO of data has been written.

## Directory layout

```
main.cpp    entry point, command-line handling, the two run modes
cpu.h/.cpp  CPU core: fetch/decode/execute, CSR file, traps, counters
rvc.h/.cpp  RV32C compressed instruction expansion
bus.h/.cpp  address decoder and RAM
dev.h/.cpp  MMIO devices: 16550A UART and CLINT
plic.h/.cpp PLIC 1.0.0 platform interrupt controller
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

Type `root` at the `rvsimu login:` prompt to get a shell. The console uses
CR (0x0D) as the line terminator, as a serial terminal does.

## Rebuilding the root filesystem

The rootfs is produced by Buildroot. Both the configuration and the post
processing are in `rootfs/`:

```
rootfs/buildroot_defconfig       Buildroot configuration
rootfs/busybox-no-pie.config     busybox fragment (static, no PIE)
rootfs/mkpayload.py              packs the rootfs back into images/fw_payload.bin
```

### 1. Build the toolchain and rootfs

```sh
git clone https://github.com/buildroot/buildroot.git
cd buildroot
git checkout 2024.02.9

cp <this repo>/rootfs/buildroot_defconfig configs/rv32ima_defconfig
cp <this repo>/rootfs/busybox-no-pie.config package/busybox/no-pie.config

make rv32ima_defconfig
make            # the toolchain build dominates the time; expect a few hours
```

The resulting rootfs is `output/images/rootfs.cpio.gz`.

Two settings matter more than the rest and are easy to get wrong:

* The target must be `BR2_riscv_custom`, not the default General purpose (G)
  variant. The G variant force-selects the F and D extensions, which this
  simulator does not implement.
* busybox must be built static and without PIE (`BR2_PIC_PIE` off plus
  `CONFIG_STATIC=y`). The default build is position independent, which needs
  relocations at load time that the kernel on this target does not perform.

Buildroot installs no character devices under `dev/`; it expects devtmpfs to
populate the directory at boot. The kernel opens `/dev/console` before that
mount happens, so the packing step adds those nodes explicitly.

### 2. Pack it into the payload

```sh
python3 rootfs/mkpayload.py images/fw_payload.bin \
        output/images/rootfs.cpio.gz images/fw_payload.new.bin
```

`mkpayload.py` replaces the initramfs inside the existing payload, so the
OpenSBI and kernel images stay untouched. It performs the steps the boot
sequence depends on:

| Step | Why |
|---|---|
| Drop `lib/`, `lib32/`, `usr/lib/` | busybox is static, and the shared objects would not fit the slot |
| Add `dev/console`, `dev/null`, `dev/tty`, `dev/zero` | the kernel opens `/dev/console` before devtmpfs is mounted |
| Make `/init` a hard link of `bin/busybox` | buildroot installs it as a two-step symlink; a hard link shares the inode and saves the second copy |
| Recompress at level 9 with `mtime=0` | the stream must be plain gzip with no FEXTRA field and a stable length |

Two constraints come out of the image format and are worth repeating: the new
gzip stream may not be longer than the original one, and it must be plain
gzip. The in-kernel decompressor used here rejects the FEXTRA field, so any
padding has to go after the stream rather than inside it. The script checks
both and refuses to write a payload that would not boot.
