//
// Created by Chengbo on 2026/6/27.
//

#include "cpu.h"

Cpu::Cpu() {
    pc = 0x80000000;
    reg.fill(0);
    bus = nullptr;
}

void Cpu::connect(Bus *b) {
    bus = b;
}

void Cpu::step() {

    /* STEP1: fetch instruction. */
    const uint32_t inst = bus->read(pc, 4);
    // printf("%08x, %08x\n", pc, inst);

    /* STEP2: decode instruction. */
    // ----------------------------- Instruction format ------------------------------
    //  31            25 24     20 19     15 14    12 11        7 6          0
    // |     funct7     |   rs2   |   rs1   | funct3 |     rd    |   opcode   | R-type
    // |        imm[11:0]         |   rs1   | funct3 |     rd    |   opcode   | I-type
    // |    imm[11:5]   |   rs2   |   rs1   | funct3 |  imm[4:0] |   opcode   | S-type
    // |  imm[12|10:5]  |   rs2   |   rs1   | funct3 |imm[4:1|11]|   opcode   | B-type
    // |                imm[31:12]                   |     rd    |   opcode   | U-type
    // |          imm[20|10:1|11|19:12]              |     rd    |   opcode   | J-type
    // -------------------------------------------------------------------------------

    /* Get register address and value. */
    uint8_t rd = inst >> 7 & 0x1f;
    const uint8_t rs1 = inst >> 15 & 0x1f;
    const uint8_t rs2 = inst >> 20 & 0x1f;

    uint32_t src1 = reg[rs1];
    uint32_t src2 = reg[rs2];

    /* Instruction with masks. */
    uint32_t r_type_inst = inst & 0xfe00707f;
    uint32_t i_type_inst = inst & 0x0000707f;
    uint32_t u_type_inst = inst & 0x0000007f;

    // ------------------------------------------- Types of immediate ---------------------------------------
    //  31        30          20 19         12 11         10          5 4           1      0
    // |                   inst[31]                      | inst[30:25] | inst[24:21] | inst[20] | I-immediate
    // |                   inst[31]                      | inst[30:25] | inst[11:8]  | inst[7]  | S-immediate
    // |               inst[31]               | inst[7]  | inst[30:25] | inst[11:8]  |     0    | B-immediate
    // | inst[31] | inst[30:20] | inst[19:12] |                     0                           | U-immediate
    // |        inst[31]        | inst[19:12] | inst[20] | inst[30:25] | inst[24:21] |     0    | J-immediate
    // ------------------------------------------------------------------------------------------------------

    /* STEP3: excute instruction. */
    const uint32_t next_pc = pc + 4;

    /* STEP4: update PC. */
    pc = next_pc;
}

void Cpu::run() {
    step();
}
