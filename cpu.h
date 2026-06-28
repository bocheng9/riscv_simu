//
// Created by Chengbo on 2026/6/27.
//

#ifndef RVSIMU_V3_CPU_H
#define RVSIMU_V3_CPU_H

#include <array>
#include "Bus.h"


class Cpu {
protected:
    /* Registers. */
    uint32_t pc;
    std::array<uint32_t, 32> reg{};

    /* Connection. */
    Bus *bus;

    /* Execute one instruction at a time. */
    bool trace;
    void step();

public:
    Cpu();

    /* Connet to BUS. */
    void connect(Bus *b);

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

class Trap : public std::exception {
protected:
    uint32_t mcause;  // Cause Code.
    uint32_t mtval;   // Bad Address or instruction.

public:
    Trap(const uint32_t cause, const uint32_t tval): mcause(cause), mtval(tval) {}
    uint32_t get_mcause() const { return mcause; }
    uint32_t get_mtval()  const { return mtval; }
};


inline uint32_t sign_extend(const uint32_t x, const uint8_t width) {
    if (x & 1 << (width - 1)) {
        return x | 0xffffffff << width;
    } else {
        return x;
    }
}

#endif //RVSIMU_V3_CPU_H
