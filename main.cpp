//
// main.cpp — entry point
//
// Two operating modes:
//   1) ISA test:  rvsimu <elf> [--trace] [--no-isa] [--max N]
//   2) Linux boot: rvsimu --linux [--payload <bin>] [--dtb <dtb>] [--trace]
//

#include <iostream>
#include <string>
#include <cstring>
#include "utils.h"
#include "bus.h"
#include "cpu.h"
#include "dev.h"
#include "plic.h"

static constexpr uint32_t DRAM_BASE  = 0x80000000;
static constexpr uint32_t DRAM_SIZE  = 256u * 1024 * 1024;
static constexpr uint32_t UART_BASE  = 0x10000000;
static constexpr uint32_t UART_SIZE  = 0x100;
static constexpr uint32_t CLINT_BASE = 0x02000000;
static constexpr uint32_t CLINT_SIZE = 0x10000;
static constexpr uint32_t PLIC_BASE  = 0x0c000000;
static constexpr uint32_t PLIC_SIZE  = 0x4000000;
static constexpr uint32_t UART_IRQ   = 10;        // QEMU virt: serial0 = IRQ 10
static constexpr uint32_t S_CTX      = 1;         // PLIC context 1 = hart0 S-mode
static constexpr uint32_t DTB_OFFSET = 0x07e00000;   // relative to DRAM_BASE

//============================================================================
// ISA test mode
//============================================================================
static int run_isa(const std::string& path, bool trace, bool isa_test,
                   uint64_t max_cycles) {
    const auto dram = std::make_shared<Dram>(DRAM_BASE, 1024 * 1024);
    dram->load_elf(path);
    const uint32_t host_addr = get_tohost_addr(path);

    Bus bus;
    bus.map(DRAM_BASE, 1024 * 1024, dram);

    Cpu cpu;
    cpu.connect(&bus);
    if (trace) cpu.trace_on();

    uint64_t cycles = 0;
    while (true) {
        cpu.run();
        cycles++;

        if (isa_test) {
            const uint32_t to_lo = bus.read(host_addr, 4);
            const uint32_t to_hi = bus.read(host_addr + 4, 4);
            if (to_lo == 1 && to_hi == 0) { printf("PASS\n"); return 0; }
            if (to_lo != 0 || to_hi != 0) {
                printf("FAIL testnum=%u hi=%u\n", to_lo >> 1, to_hi);
                return 1;
            }
        }
        if (max_cycles && cycles >= max_cycles) {
            printf("TIMEOUT cycles=%llu\n", (unsigned long long)cycles);
            return 2;
        }
    }
}

//============================================================================
// Linux boot mode
//============================================================================
static int run_linux(const std::string& payload, const std::string& dtb,
                     bool trace, uint64_t max_cycles) {
    const auto dram = std::make_shared<Dram>(DRAM_BASE, DRAM_SIZE);
    dram->load_bin(payload, 0);
    if (!dtb.empty()) {
        dram->load_bin(dtb, DTB_OFFSET);
    }

    const auto uart  = std::make_shared<Uart>();
    const auto clint = std::make_shared<Clint>();
    const auto plic  = std::make_shared<Plic>();

    Bus bus;
    bus.map(DRAM_BASE,  DRAM_SIZE,  dram);
    bus.map(UART_BASE,  UART_SIZE,  uart);
    bus.map(CLINT_BASE, CLINT_SIZE, clint);
    bus.map(PLIC_BASE,  PLIC_SIZE,  plic);

    Cpu cpu;
    cpu.connect(&bus);
    cpu.set_boot_context(0, DRAM_BASE + DTB_OFFSET);
    if (trace) cpu.trace_on();

    fprintf(stderr, "[sim] payload=%s  dtb=%s (@%08x)\n",
            payload.c_str(), dtb.c_str(), DRAM_BASE + DTB_OFFSET);

    uint64_t cycles = 0;
    while (true) {
        /* Advance mtime once every MTIME_DIV instructions so the simulated
         * clock keeps a fixed ratio to the instruction stream. */
        if (cycles % Clint::MTIME_DIV == 0) clint->tick();
        cpu.update_time(clint->mtime());

        /* CLINT drives the machine timer interrupt (MTIP) only. The M-mode
         * firmware (OpenSBI) traps on it and forwards the event to S-mode by
         * setting STIP and re-programming mtimecmp; driving STIP directly
         * from the CLINT would skip that handover. */
        const bool timer = clint->pending();
        cpu.set_timer_irq(timer);

        /* UART sources IRQ 10 into the PLIC; the PLIC raises SEIP when the
         * source is enabled and above the context threshold. */
        plic->set_irq(UART_IRQ, uart->irq());
        cpu.set_ext_irq(plic->pending_for(S_CTX));
        cpu.run();
        cycles++;
        if (max_cycles && cycles >= max_cycles) {
            fprintf(stderr, "\n[sim] TIMEOUT cycles=%llu pc=%08x\n",
                    (unsigned long long)cycles, cpu.get_pc());
            return 2;
        }
    }
}

//============================================================================
// Entry point
//============================================================================
int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr,
            "Usage:\n"
            "  %s <elf> [--trace] [--no-isa] [--max N]\n"
            "  %s --linux [--payload <bin>] [--dtb <dtb>] [--max N] [--trace]\n",
            argv[0], argv[0]);
        return 3;
    }

    bool linux_mode = false, trace = false, isa_test = true;
    uint64_t max_cycles = 0;
    std::string elf, payload = "fw_payload_buildroot.bin", dtb = "virt.dtb";

    for (int i = 1; i < argc; i++) {
        const std::string a = argv[i];
        if (a == "--linux")        linux_mode = true;
        else if (a == "--trace")   trace = true;
        else if (a == "--no-isa")  isa_test = false;
        else if (a == "--max" && i + 1 < argc) max_cycles = strtoull(argv[++i], nullptr, 0);
        else if (a == "--payload" && i + 1 < argc) payload = argv[++i];
        else if (a == "--dtb" && i + 1 < argc) dtb = argv[++i];
        else if (a.rfind("--", 0) != 0) elf = a;
        else { fprintf(stderr, "unknown argument: %s\n", a.c_str()); return 3; }
    }

    try {
        if (linux_mode) return run_linux(payload, dtb, trace, max_cycles);
        if (elf.empty()) { fprintf(stderr, "missing ELF path\n"); return 3; }
        return run_isa(elf, trace, isa_test, max_cycles);
    } catch (const std::exception& e) {
        fprintf(stderr, "ERROR: %s\n", e.what());
        return 2;
    }
}
