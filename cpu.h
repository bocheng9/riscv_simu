//
// Created by Chengbo on 2026/6/27.
//

#ifndef RVSIMU_V3_CPU_H
#define RVSIMU_V3_CPU_H

#include <array>
#include <map>

#include "Bus.h"

union mstatus_t {
    uint32_t value;             // 32-bit raw register value

    struct {
        uint32_t wpri0 : 1;     // [0]    WPRI - Reserved (Write Preserves, Read Ignore)
        uint32_t sie   : 1;     // [1]    Supervisor Interrupt Enable
        uint32_t wpri1 : 1;     // [2]    WPRI - Reserved
        uint32_t mie   : 1;     // [3]    Machine Interrupt Enable (Global M-mode interrupt control)
        uint32_t wpri2 : 1;     // [4]    WPRI - Reserved
        uint32_t spie  : 1;     // [5]    Previous Supervisor Interrupt Enable (Preserved on trap)
        uint32_t ube   : 1;     // [6]    User-mode Endianness (0: Little-Endian, 1: Big-Endian)
        uint32_t mpie  : 1;     // [7]    Previous Machine Interrupt Enable (Preserved on trap)
        uint32_t spp   : 1;     // [8]    Previous Supervisor Privilege Mode (0: U-mode, 1: S-mode)
        uint32_t mpp   : 2;     // [10:9] Previous Machine Privilege Mode (00: U, 01: S, 11: M)
        uint32_t vs    : 2;     // [12:11] Vector Extension Status (00:Off, 01:Initial, 10:Clean, 11:Dirty)
        uint32_t fs    : 2;     // [14:13] Floating-Point Status (00:Off, 01:Initial, 10:Clean, 11:Dirty)
        uint32_t mprv  : 1;     // [15]   Modify Privilege (1: data loads/stores use MPP privilege level)
        uint32_t sum   : 1;     // [16]   Permit Supervisor User Memory Access (1: S-mode can access U-mode pages)
        uint32_t mxr   : 1;     // [17]   Make Executable Readable (1: loads can read from execute-only pages)
        uint32_t tvm   : 1;     // [18]   Trap Virtual Memory (1: intercepts SFENCE.VMA and satp writes in S-mode)
        uint32_t tw    : 1;     // [19]   Timeout Wait (1: intercepts WFI instruction execution in lower modes)
        uint32_t tsr   : 1;     // [20]   Trap SRET (1: intercepts SRET instruction execution in S-mode)
        uint32_t wpri3 : 11;    // [31:21] WPRI - Reserved
    } bits;
};

class Trap : public std::exception {
public:
    uint32_t cause;     // Cause Code.
    uint32_t value;     // Bad Address or instruction.
    Trap(const uint32_t cause, const uint32_t val): cause(cause), value(val) {}
};

class Cpu {
protected:
    /* Registers. */
    uint8_t privilege;
    uint32_t pc;
    std::array<uint32_t, 32> reg{};

    /* CSRs */
    union mstatus_t mstatus{};
    uint32_t mie;
    uint32_t mtvec;
    uint32_t mscratch;
    uint32_t mepc;
    uint32_t mcause;
    uint32_t mtval;
    uint32_t mip;
    uint32_t mcycle;
    uint32_t mcycleh;

    /* Connection. */
    Bus *bus;

    /* Execute one instruction at a time. */
    bool trace;
    void execute();
    void handle_exception(const Trap& trap);

public:
    Cpu();

    /* Connet to BUS. */
    void connect(Bus *b);

    /* Read CSR. */
    [[nodiscard]] uint32_t read_csr(uint16_t addr) const;
    void write_csr(uint16_t addr, uint32_t val);

    /* Run CPU. */
    void run();
    void trace_on();
};

inline const char* REG[] = {
    "zero", "ra",   "sp",   "gp",   "tp",   "t0",   "t1",   "t2",
    "s0",   "s1",   "a0",   "a1",   "a2",   "a3",   "a4",   "a5",
    "a6",   "a7",   "s2",   "s3",   "s4",   "s5",   "s6",   "s7",
    "s8",   "s9",   "s10",  "s11",  "t3",   "t4",   "t5",   "t6"
};

inline std::map<uint16_t, std::string> CSR {
    // User Trap Setup
    {0x000, "ustatus"},
    {0x004, "uie"},
    {0x005, "utvec"},

    // User Timers and Counters
    {0xC00, "cycle"},
    {0xC01, "time"},
    {0xC02, "instret"},
    {0xC80, "cycleh"},
    {0xC81, "timeh"},
    {0xC82, "instreth"},

    // Supervisor Trap Setup
    {0x100, "sstatus"},
    {0x104, "sie"},
    {0x105, "stvec"},
    {0x106, "scounteren"},

    // Supervisor Trap Handling
    {0x140, "sscratch"},
    {0x141, "sepc"},
    {0x142, "scause"},
    {0x143, "stval"},
    {0x144, "sip"},

    // Supervisor Protection and Translation
    {0x180, "satp"},

    // Machine Information Registers
    {0xF11, "mvendorid"},
    {0xF12, "marchid"},     // Architecture ID discussed earlier
    {0xF13, "mimpid"},
    {0xF14, "mhartid"},

    // Machine Trap Setup
    {0x300, "mstatus"},     // Machine status register union
    {0x301, "misa"},
    {0x302, "medeleg"},
    {0x303, "mideleg"},
    {0x304, "mie"},
    {0x305, "mtvec"},
    {0x306, "mcounteren"},

    // Machine Trap Handling
    {0x340, "mscratch"},
    {0x341, "mepc"},
    {0x342, "mcause"},
    {0x343, "mtval"},
    {0x344, "mip"}
};


inline uint32_t sign_extend(const uint32_t x, const uint8_t width) {
    if (x & 1 << (width - 1) && width < 32) {
        return x | 0xffffffff << width;
    }
    return x;
}

#endif //RVSIMU_V3_CPU_H
