//
// Created by Chengbo on 2026/6/27.
//

#include <cstdio>
#include "cpu.h"

Cpu::Cpu() {
    pc = 0x80000000;
    reg.fill(0);
    trace = false;
    bus = nullptr;
}

void Cpu::connect(Bus *b) {
    bus = b;
}

void Cpu::step() {

    /* STEP1: fetch instruction. */
    const uint32_t inst = bus->read(pc, 4);

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

    /* Get operation code. */
    const uint8_t opcode7 = inst & 0x7f;
    const uint8_t opcode3 = inst >> 12 & 7;

    /* Get register address and value. */
    const uint8_t rd = inst >> 7 & 0x1f;
    const uint8_t rs1 = inst >> 15 & 0x1f;
    const uint8_t rs2 = inst >> 20 & 0x1f;

    const uint32_t src1 = reg[rs1];
    const uint32_t src2 = reg[rs2];

    // ------------------------------------------- Types of immediate ---------------------------------------
    //  31        30          20 19         12 11         10          5 4           1      0
    // |                   inst[31]                      | inst[30:25] | inst[24:21] | inst[20] | I-immediate
    // |                   inst[31]                      | inst[30:25] | inst[11:8]  | inst[7]  | S-immediate
    // |               inst[31]               | inst[7]  | inst[30:25] | inst[11:8]  |     0    | B-immediate
    // | inst[31] | inst[30:20] | inst[19:12] |                     0                           | U-immediate
    // |        inst[31]        | inst[19:12] | inst[20] | inst[30:25] | inst[24:21] |     0    | J-immediate
    // ------------------------------------------------------------------------------------------------------

    const uint32_t imm_i_type = sign_extend(inst >> 20 & 0xfff, 12);
    const uint32_t imm_s_type = sign_extend(inst >> 7 & 0x1f | (inst >> 25 & 0x7f) << 5, 12);
    const uint32_t imm_b_type = sign_extend((inst >> 8 & 0xf) << 1 | (inst >> 25 & 0x3f) << 5 |
        (inst >> 7 & 0x1) << 11 | (inst >> 31 & 0x1) << 12, 13);
    const uint32_t imm_u_type = inst & 0xfffff000;
    const uint32_t imm_j_type = sign_extend((inst >> 21 & 0x3ff) << 1 | (inst >> 20 & 0x1) << 11 |
        (inst >> 12 & 0xff) << 12 | (inst >> 31 & 0x1) << 20, 21);

    const uint16_t csr = inst & 0xfff;
    const uint8_t shamt = inst >> 20 & 0x1f;

    /* STEP3: excute instruction. */
    reg[0] = 0; // Reset zero.
    uint32_t next_pc = pc + 4;

    switch (opcode7) {
        case 0x13:
            switch (opcode3) {
                case 0x00:  // ADDI.
                    reg[rd] = src1 + imm_i_type;
                    if (trace) {
                        printf("%08x: %08x, addi %s, %s, %x\n", pc, inst, REG[rd], REG[rs1], imm_i_type);
                    }
                    break;
                default:
                    throw Trap(2, inst);    // Illegal instruction.
            }

        case 0x33:
            switch (opcode3) {
                case 0x00:  // ADD.
                    reg[rd] = src1 + src2;
                    break;
                default:
                    throw Trap(2, inst);    // Illegal instruction.
            }
            break;
        case 0x37:  // LUI.
            reg[rd] = imm_u_type;
            if (trace) {
                printf("%08x: %08x, lui %s, %x\n", pc, inst, REG[rd], imm_u_type >> 12);
            }
            break;
        case 0x6f:  // JAL.
            next_pc = pc + imm_j_type;
            reg[rd] = pc + 4;
            if (trace) {
                printf("%08x: %08x, jal %s, %d \t// %08x\n",
                    pc, inst, REG[rd], static_cast<int32_t>(imm_j_type), next_pc);
            }
            break;
        default:
            throw Trap(2, inst);    // Illegal instruction.
    }

    /* STEP4: update PC. */
    pc = next_pc;
}

void Cpu::run() {
    try {
        step();
    } catch (const Trap& trap) {
        printf("Trap %x, %x\n", trap.get_mcause(), trap.get_mtval());
    }

}

void Cpu::trace_on() {
    trace = true;
}
