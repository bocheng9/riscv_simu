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
    std::array<uint32_t, 32> reg;

    /* Connection. */
    Bus *bus;

    /* Execute one instruction at a time. */
    void step();

public:
    Cpu();

    /* Connet to BUS. */
    void connect(Bus *b);

    /* Run CPU. */
    void run();
};


#endif //RVSIMU_V3_CPU_H
