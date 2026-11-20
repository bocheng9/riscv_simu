//

#ifndef RVSIMU_V3_CPU_H
#define RVSIMU_V3_CPU_H

#include <array>
#include <map>
#include <string>

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
        uint32_t wpri3 : 2;     // [10:9] Previous Machine Privilege Mode (00: U, 01: S, 11: M)
        uint32_t mpp   : 2;     // [12:11] Vector Extension Status (00:Off, 01:Initial, 10:Clean, 11:Dirty)
        uint32_t fs    : 2;     // [14:13] Floating-Point Status (00:Off, 01:Initial, 10:Clean, 11:Dirty)
        uint32_t xs    : 2;     // [16:15] Extension State. Tracks the status of user-level extensions.
        uint32_t mprv  : 1;     // [17]   Modify Privilege (1: data loads/stores use MPP privilege level)
        uint32_t sum   : 1;     // [18]   Permit Supervisor User Memory Access (1: S-mode can access U-mode pages)
        uint32_t mxr   : 1;     // [19]   Make Executable Readable (1: loads can read from execute-only pages)
        uint32_t tvm   : 1;     // [20]   Trap Virtual Memory (1: intercepts SFENCE.VMA and satp writes in S-mode)
        uint32_t tw    : 1;     // [21]   Timeout Wait (1: intercepts WFI instruction execution in lower modes)
        uint32_t tsr   : 1;     // [22]   Trap SRET (1: intercepts SRET instruction execution in S-mode)
        uint32_t wpri4 : 9;    // [31:23] WPRI - Reserved
    } bits;
};

class Trap : public std::exception {
public:
    uint32_t cause;     // Cause Code.
    uint32_t value;     // Bad Address or instruction.
    Trap(const uint32_t cause, const uint32_t val): cause(cause), value(val) {}
};

enum AccessType {
    FETCH = 0,
    LOAD,
    STORE,
    AMO,
};

class Cpu {
protected:
    /* Registers. */
    uint8_t privilege;
    uint32_t pc;
    std::array<uint32_t, 32> reg{};

    /* CSRs */
    mstatus_t mstatus{};
    uint32_t medeleg;
    uint32_t mideleg;

    /* PMP (CSR storage only, no permission checks yet) */
    uint32_t pmpcfg[4]  = {0, 0, 0, 0};
    uint32_t pmpaddr[16] = {0};

    /* Debug triggers (0x7A0-0x7BF, 32 of them): CSR storage only */
    uint32_t tdata[32] = {0};
    uint32_t mie;
    uint32_t mtvec;
    uint32_t mscratch;
    uint32_t mepc;
    uint32_t mcause;
    uint32_t mtval;
    uint32_t mip;
    uint32_t mcycle;
    uint32_t mcycleh;

    /* Wall clock pushed in by CLINT every step, read by time/timeh CSRs */
    uint64_t mtime = 0;

    /* Retired instruction counter (backing store for instret/instreth) */
    uint64_t instret_cnt = 0;

    /* Set when this instruction wrote minstret, suppresses its own increment */
    bool instret_written = false;

    uint32_t stvec;
    uint32_t sscratch;
    uint32_t sepc;
    uint32_t scause;
    uint32_t stval;
    uint32_t satp;

    /* Connection. */
    bool reserve;
    Bus *bus;

    /* Execute one instruction at a time. */
    bool trace;
    uint32_t cur_inst = 0;    // current instruction (used to emit exception trace)
    bool inst_traced = false; // whether this instruction was already traced
    uint32_t last_exc_pc = 0xFFFFFFFF; // PC of the last exception
    void execute();
    void throw_illegal();


    void handle_exception(const Trap& trap);

public:
    Cpu();

    /* Called by CLINT every step: refresh the wall clock */
    void update_time(uint64_t t) { mtime = t; }

    /* External interrupt lines (UART on the virt platform).
     * drv_uart feeds the interrupt request into mip.SEIP/MEIP. */
    void set_ext_irq(bool active) {
        if (active) {
            mip |= (1u << 9) | (1u << 11);     // SEIP | MEIP
        } else {
            mip &= ~((1u << 9) | (1u << 11));
        }
    }

    /* Current PC (for debugging output) */
    [[nodiscard]] uint32_t get_pc() const { return pc; }

    /* Set boot context (OpenSBI convention: a0=hartid, a1=DTB physical address) */
    void set_boot_context(uint32_t hartid, uint32_t dtb_addr) {
        reg[10] = hartid;
        reg[11] = dtb_addr;
    }

    /* Connet to BUS. */
    void connect(Bus *b);

    /* Read and write CSR. */
    [[nodiscard]] uint32_t read_csr(uint16_t addr) const;
    void write_csr(uint16_t addr, uint32_t val);

    /* Read and write MEM. */
    [[nodiscard]] uint32_t mmu(uint32_t v_addr, AccessType access_type) const;
    [[nodiscard]] uint32_t read_mem(uint32_t addr, uint32_t n_bytes, AccessType access_type) const;
    void write_mem(uint32_t addr, uint32_t n_bytes, uint32_t data) const;

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


inline uint32_t signed_extend(const uint32_t x, const uint8_t width) {
    if (x & 1 << (width - 1) && width < 32) {
        return x | 0xffffffff << width;
    }
    return x;
}

inline uint64_t signed_mul(const int64_t x, const int64_t y) {
    return static_cast<uint64_t>(x * y);
}

inline uint32_t signed_div(const int64_t x, const int64_t y) {
    if (y == 0) return 0xffffffff;
    return static_cast<uint32_t>(x / y);
}

inline uint32_t signed_rem(const int64_t x, const int64_t y) {
    if (y == 0) return static_cast<uint32_t>(x);
    return static_cast<uint32_t>(x % y);
}

#endif //RVSIMU_V3_CPU_H
