//
// Created by Chengbo on 2026/6/27.
//

#include <cstdio>
#include "cpu.h"

Cpu::Cpu() {
    /* Initialize registers. */
    privilege = 3;
    pc = 0x80000000;
    reg.fill(0);

    /* Initialize CSRs. */
    mstatus.value = 0;
    mie = 0;
    mtvec = 0;
    mscratch = 0;
    mepc = 0;
    mcause = 0;
    mtval = 0;
    mip = 0;
    mcycle = 0;
    mcycleh = 0;

    /* Init BUS. */
    reserve = false;
    bus = nullptr;

    /* Init others. */
    trace = false;
}

void Cpu::connect(Bus *b) {
    bus = b;
}

uint32_t Cpu::read_csr(const uint16_t addr) const {
    uint32_t val = 0;
    switch (addr) {
        case 0xf11:  // mvendorid
        case 0xf12:  // marchid
        case 0xf13:  // mimpid
        case 0xf14:  // mhartid
            break;
        case 0x300:  // mstatus
            val = mstatus.value;
            break;
        case 0x301:  // misa
            val = 0x40041101;
            break;
        case 0x304:  // mie
            val = mie;
            break;
        case 0x305:  // mtvec
            val = mtvec;
            break;
        case 0x340:  // mscratch
            val = mscratch;
            break;
        case 0x341:  // mepc
            val = mepc;
            break;
        case 0x342:  // mcause
            val = mcause;
            break;
        case 0x343:  // mtval
            val = mtval;
            break;
        case 0x344:  // mtval
            val = mip;
            break;
        default:
            if (trace) printf("CSR: %x not implemented\n", addr);
            break;
    }
    return val;
}

void Cpu::write_csr(const uint16_t addr, const uint32_t val) {
    switch (addr) {
        case 0xf11:  // mvendorid
        case 0xf12:  // marchid
        case 0xf13:  // mimpid
        case 0xf14:  // mhartid
        case 0x301:  // misa
            break;
        case 0x300:  // mstatus
            mstatus.value = val;
            break;
        case 0x304:  // mie
            mie = val;
            break;
        case 0x305:  // mtvec
            mtvec = val;
            break;
        case 0x340:  // mscratch
            mscratch = val;
            break;
        case 0x341:  // mepc
            mepc = val;
            break;
        case 0x342:  // mcause
            mcause = val;
            break;
        case 0x343:  // mtval
            mtval = val;
            break;
        case 0x344:  // mtval
            mip = val;
            break;
        default:
            if (trace) printf("CSR: %x not implemented\n", addr);
            break;
    }
}

void Cpu::execute() {

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
    const uint8_t funct3 = inst >> 12 & 7;
    const uint8_t funct7 = inst >> 25 & 0x7f;

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

    const uint32_t imm_i = signed_extend(inst >> 20 & 0xfff, 12);
    const uint32_t imm_s = signed_extend(inst >> 7 & 0x1f | (inst >> 25 & 0x7f) << 5, 12);
    const uint32_t imm_b = signed_extend((inst >> 8 & 0xf) << 1 | (inst >> 25 & 0x3f) << 5 |
        (inst >> 7 & 0x1) << 11 | (inst >> 31 & 0x1) << 12, 13);
    const uint32_t imm_u = inst & 0xfffff000;
    const uint32_t imm_j = signed_extend((inst >> 21 & 0x3ff) << 1 | (inst >> 20 & 0x1) << 11 |
        (inst >> 12 & 0xff) << 12 | (inst >> 31 & 0x1) << 20, 21);

    /* STEP3: excute instruction. */
    uint32_t next_pc = pc + 4;
    std::string op;

    switch (opcode7) {
        case 0x33:  // R-type instructions.
            switch (funct3) {
                case 0:
                    if (funct7 == 0) {  // ADD
                        if (rd != 0) reg[rd] = src1 + src2;
                        if (trace) op = "add";
                    } else if (funct7 == 0x20) {  // SUB
                        if (rd != 0) reg[rd] = src1 - src2;
                        if (trace) op = "sub";
                    } else if (funct7 == 1) {  // MUL
                        if (rd != 0) reg[rd] =
                            signed_mul(static_cast<int32_t>(src1), static_cast<int32_t>(src2)) & 0xffffffff;
                        if (trace) op = "mul";
                    } else throw Trap(2, inst);  // Illegal instruction.
                    break;
                case 1:
                    if (funct7 == 0) {  // SLL
                        if (rd != 0) reg[rd] = src1 << (src2 & 0x1f);
                        if (trace) op = "sll";
                    } else if (funct7 == 1) {  // MULH
                        if (rd != 0) reg[rd] =
                            signed_mul(static_cast<int32_t>(src1), static_cast<int32_t>(src2)) >> 32 & 0xffffffff;
                        if (trace) op = "mulh";
                    } else throw Trap(2, inst);  // Illegal instruction.
                    break;
                case 2:
                    if (funct7 == 0) {  // SLT
                        if (rd != 0) reg[rd] = static_cast<int32_t>(src1) < static_cast<int32_t>(src2) ? 1 : 0;
                        if (trace) op = "slt";
                    } else if (funct7 == 1) {  // MULHSU
                        if (rd != 0) reg[rd] = signed_mul(static_cast<int32_t>(src1), src2) >> 32 & 0xffffffff;
                        if (trace) op = "mulhsu";
                    } else throw Trap(2, inst);  // Illegal instruction.
                    break;
                case 3:
                    if (funct7 == 0) {  // SLTU
                        if (rd != 0) reg[rd] = src1 < src2 ? 1 : 0;
                        if (trace) op = "sltu";
                    } else if (funct7 == 1) {  // MULHU
                        if (rd != 0) reg[rd] =
                            signed_mul(src1, src2) >> 32 & 0xffffffff;
                        if (trace) op = "mulhu";
                    } else throw Trap(2, inst);  // Illegal instruction.
                    break;
                case 4:
                    if (funct7 == 0) {  // XOR
                        if (rd != 0) reg[rd] = src1 ^ src2;
                        if (trace) op = "xor";
                    } else if (funct7 == 1) {
                        if (rd != 0) reg[rd] = signed_div(static_cast<int32_t>(src1), static_cast<int32_t>(src2));
                        if (trace) op = "div";
                    } else throw Trap(2, inst);  // Illegal instruction.
                    break;
                case 5:
                    if (funct7 == 0) {  // SRL
                        if (rd != 0) reg[rd] = src1 >> (src2 & 0x1f);
                        if (trace) op = "srl";
                    } else if (funct7 == 0x20) {  // SRA
                        if (rd != 0) reg[rd] = static_cast<int32_t>(src1) >> (src2 & 0x1f);
                        if (trace) op = "sra";
                    } else if (funct7 == 1) {  // DIVU
                        if (rd != 0) reg[rd] = signed_div(src1, src2);
                        if (trace) op = "divu";
                    } else throw Trap(2, inst);
                    break;
                case 6:
                    if (funct7 == 0) {  // OR
                        if (rd != 0) reg[rd] = src1 | src2;
                        if (trace) op = "or";
                    } else if (funct7 == 1) {  // REM
                        if (rd != 0) reg[rd] = signed_rem(static_cast<int32_t>(src1), static_cast<int32_t>(src2));
                        if (trace) op = "rem";
                    } else throw Trap(2, inst);  // Illegal instruction.
                    break;
                case 7:
                    if (funct7 == 0) {  // AND
                        if (rd != 0) reg[rd] = src1 & src2;
                        if (trace) op = "and";
                    } else if (funct7 == 1) {  // REMU
                        if (rd != 0) reg[rd] = signed_rem(src1, src2);
                        if (trace) op = "remu";
                    } else throw Trap(2, inst);  // Illegal instruction.
                    break;
                default:
                    throw Trap(2, inst);  // Illegal instruction.
            }
            if (trace) printf("%08x: %08x \t %s %s, %s, %s\n",
                pc, inst, op.c_str(), REG[rd], REG[rs1], REG[rs2]);
            break;
        case 0x13:  // I-type instructions.
            switch (funct3) {
                case 0:  // ADDI
                    if (rd != 0) reg[rd] = src1 + imm_i;
                    if (trace) op = "addi";
                    break;
                case 1: // SLLI
                    if (funct7 == 0) {
                        if (rd != 0) reg[rd] = src1 << (imm_i & 0x1f);
                        if (trace) op = "slli";
                    } else throw Trap(2, inst);  // Illegal instruction.
                    break;
                case 2:  // SLTI
                    if (rd != 0) reg[rd] = static_cast<int32_t>(src1) < static_cast<int32_t>(imm_i) ? 1 : 0;
                    if (trace) op = "slti";
                    break;
                case 3:  // SLTIU
                    if (rd != 0) reg[rd] = src1 < imm_i? 1 : 0;
                    if (trace) op = "sltiu";
                    break;
                case 4:  // XORI
                    if (rd != 0) reg[rd] = src1 ^ imm_i;
                    if (trace) op = "xori";
                    break;
                case 5:
                    if (funct7 == 0) {  // SRLI
                        if (rd != 0) reg[rd] = src1 >> (imm_i & 0x1f);
                        if (trace) op = "srli";
                    } else if (funct7 == 0x20) {  // SRAI
                        if (rd != 0) reg[rd] = static_cast<int32_t>(src1) >> (imm_i & 0x1f);
                        if (trace) op = "srai";
                    } else throw Trap(2, inst);  // Illegal instruction.
                    break;
                case 6:  // ORI
                    if (rd != 0) reg[rd] = src1 | imm_i;
                    if (trace) op = "ori";
                    break;
                case 7:  // ANDI
                    if (rd != 0) reg[rd] = src1 & imm_i;
                    if (trace) op = "andi";
                    break;
                default:
                    throw Trap(2, inst);  // Illegal instruction.
            }
            if (trace) printf("%08x: %08x \t %s %s, %s, %d\n",
                pc, inst, op.c_str(), REG[rd], REG[rs1], static_cast<int32_t>(imm_i));
            break;
        case 0x3: { // I-type instructions (LOAD)
            if ((funct3 & 3) != 3) {
                const uint8_t n_bytes = 1 << (funct3 & 0x3);        // Get byte number.
                const uint32_t addr = src1 + imm_i;                 // Get address.
                const uint32_t data = bus->read(addr , n_bytes);    // Load data.
                if (rd != 0) reg[rd] = funct3 & 0x4? data : signed_extend(data, n_bytes << 3);
                if (trace) {
                    op = n_bytes == 1? "lb" : n_bytes == 2? "lh" : "lw";
                    op += funct3 & 0x4? "u": "";
                    printf("%08x: %08x \t %s %s, %d(%s) # %08x: %08x\n",
                        pc, inst, op.c_str(), REG[rd], static_cast<int32_t>(imm_i), REG[rs1], addr, data);
                }
            } else throw Trap(2, inst);  // Illegal instruction.
            break;
        }
        case 0x23: {  // S-type instructions.
            if ((funct3 & 3) != 3) {
                const uint8_t n_bytes = 1 << (funct3 & 0x3);        // Get byte number.
                const uint32_t addr = src1 + imm_s;                 // Get address.
                bus->write(addr, n_bytes, src2);
                if (trace) {
                    op = n_bytes == 1? "sb" : n_bytes == 2? "sh" : "sw";
                    printf("%08x: %08x \t %s %s, %d(%s) # %08x <- %08x\n",
                        pc, inst, op.c_str(), REG[rs2], static_cast<int32_t>(imm_s), REG[rs1], addr, src2);
                }
            } else throw Trap(2, inst);  // Illegal instruction.
            break;
        }
        case 0x63: {  // B-type instructions.
            uint32_t addr = pc + imm_b;
            switch (funct3) {
                case 0:  // BEQ
                    if (src1 == src2) next_pc = addr;
                    if (trace) op = "beq";
                    break;
                case 1:  // BNE
                    if (src1 != src2) next_pc = addr;
                    if (trace) op = "bne";
                    break;
                case 4:  // BLT
                    if (static_cast<int32_t>(src1) < static_cast<int32_t>(src2)) next_pc = addr;
                    if (trace) op = "blt";
                    break;
                case 5:  // BGE
                    if (static_cast<int32_t>(src1) >= static_cast<int32_t>(src2)) next_pc = addr;
                    if (trace) op = "bge";
                    break;
                case 6:  // BLTU
                    if (src1 < src2) next_pc = addr;
                    if (trace) op = "bltu";
                    break;
                case 7:
                    if (src1 >= src2) next_pc = addr;
                    if (trace) op = "bgeu";
                    break;
                default:
                    throw Trap(2, inst);  // Illegal instruction.
            }
            if (trace) {
                printf("%08x: %08x \t %s %s, %s, %d # %08x\n",
                    pc, inst, op.c_str(), REG[rs1], REG[rs2], static_cast<int32_t>(imm_b), next_pc);
            }
            break;
        }
        case 0x6f:  // JAL
            next_pc = pc + imm_j;
            reg[rd] = pc + 4;
            if (trace) printf("%08x: %08x \t jal %s, %d # %08x\n",
                pc, inst, REG[rd], static_cast<int32_t>(imm_j), next_pc);
            break;
        case 0x67:  // JALR
            next_pc = src1 + imm_i & ~1;
            reg[rd] = pc + 4;
            if (trace) printf("%08x: %08x \t jal %s, %d(%s) # %08x\n",
                pc, inst, REG[rd], static_cast<int32_t>(imm_j), REG[rs1], next_pc);
            break;
        case 0x37:  // LUI
            reg[rd] = imm_u;
            if (trace) printf("%08x: %08x \t lui %s, %x\n", pc, inst, REG[rd], imm_u >> 12);
            break;
        case 0x17:  // AUIPC
            reg[rd] = pc + imm_u;
            if (trace) printf("%08x: %08x \t auipc %s, %x\n", pc, inst, REG[rd], imm_u >> 12);
            break;
        case 0x73: { // System instructions.
            if (funct3 == 0) {
                switch (inst) {
                    case 0x00000073:  // ECALL
                        if (trace) printf("%08x: %08x \t ecall\n", pc, inst);
                        throw Trap(8 + privilege, pc);
                    case 0x00100073:  // EBREAK
                        if (trace) printf("%08x: %08x \t ebreak\n", pc, inst);
                        throw Trap(3, pc);
                    case 0x30200073:  // MRET
                        privilege = mstatus.bits.mpp;           // Privilege mode is changed to MPP.
                        mstatus.bits.mie = mstatus.bits.mpie;   // MIE is set to MPIE.
                        mstatus.bits.mpie = 1;                  // MPIE is set to 1.
                        mstatus.bits.mpp = 0;                   // MPP is set to U-mode.
                        next_pc = mepc & ~3;                    // PC is set to the value stored in mepc.
                        if (trace) op = "mret";
                        break;
                    case 0x10200073:  // SRET
                        if (trace) op = "sret";
                        break;
                    case 0x10500073:  // WFI
                        if (trace) op = "wfi";
                        break;
                    default:
                        throw Trap(2, inst);
                }
                if (trace) printf("%08x: %08x \t %s\n", pc, inst, op.c_str());
            } else {
                uint16_t addr = imm_i & 0xfff;
                uint32_t old_val = read_csr(addr);
                uint32_t new_val;
                switch (funct3) {
                    case 1:  // CSRRW
                        new_val = src1;
                        if (trace) op = "csrrw";
                        break;
                    case 2:  // CSRRS
                        new_val = old_val | src1;
                        if (trace) op = "csrrs";
                        break;
                    case 3:  // CSRRC
                        new_val = old_val & ~src1;
                        if (trace) op = "csrrc";
                        break;
                    case 5:  // CSRRWI
                        new_val = rs1;
                        if (trace) op = "csrrwi";
                        break;
                    case 6:  // CSRRSI
                        new_val = old_val | rs1;
                        if (trace) op = "csrrw";
                        break;
                    case 7:  // CSRRCI
                        new_val = old_val & ~rs1;
                        if (trace) op = "csrrci";
                        break;
                    default:
                        throw Trap(2, inst);  // Illegal instruction.
                }
                write_csr(addr, new_val);
                if (rd != 0) reg[rd] = old_val;
                if (trace) {
                    if (funct3 < 4) {
                        printf("%08x: %08x \t %s %s, %s, %s # %08x\n",
                            pc, inst, op.c_str(), REG[rd], CSR[addr].c_str(), REG[rs1], new_val);
                    } else {
                        printf("%08x: %08x \t %s %s, %s, %x # %08x\n",
                            pc, inst, op.c_str(), REG[rd], CSR[addr].c_str(), rs1, new_val);
                    }
                }
            }
            break;
        }
        case 0x2f:  // Atomic instructions.
            if (funct3 == 2) {
                uint8_t funct5 = funct7 >> 2;
                if ((funct5 & 0x3) == 0) {
                    uint32_t old_val = bus->read(src1, 4);
                    uint32_t new_val;
                    switch (funct5 >> 2) {
                        case 0:  // AMOADD.W
                            new_val = old_val + src2;
                            if (trace) op = "amoadd.w";
                            break;
                        case 1: // AMOXOR.W
                            new_val = old_val ^ src2;
                            if (trace) op = "amoxor.w";
                            break;
                        case 2: // AMOOR.W
                            new_val = old_val | src2;
                            if (trace) op = "amoxor.w";
                            break;
                        case 3: // AMOADD.W
                            new_val = old_val & src2;
                            if (trace) op = "amoand.w";
                            break;
                        case 4: // AMOMIN.W
                            new_val = static_cast<int32_t>(old_val) < static_cast<int32_t>(src2)? old_val : src2;
                            if (trace) op = "amomin.w";
                            break;
                        case 5: // AMOMAX.W
                            new_val = static_cast<int32_t>(old_val) > static_cast<int32_t>(src2)? old_val : src2;
                            if (trace) op = "amomax.w";
                            break;
                        case 6: // AMOMINU.W
                            new_val = old_val < src2? old_val : src2;
                            if (trace) op = "amominu.w";
                            break;
                        case 7: // AMOMAXU.W
                            new_val = old_val > src2? old_val : src2;
                            if (trace) op = "amomaxu.w";
                            break;
                        default:
                            throw Trap(2, inst);
                    }
                    bus->write(src1, 4, new_val);
                    if (rd != 0) reg[rd] = old_val;
                    if (trace) printf("%08x: %08x \t %s %s, %s, (%s)\n",
                        pc, inst, op.c_str(), REG[rd], REG[rs2], REG[rs1]);
                } else if (funct5 == 1) {  // AMOSWAP.W
                    uint32_t old_val = bus->read(src1, 4);
                    bus->write(src1, 4, src2);
                    if (rd != 0) reg[rd] = old_val;
                    if (trace) printf("%08x: %08x \t amoswap.w %s, %s, (%s)\n",
                        pc, inst, REG[rd], REG[rs2], REG[rs1]);
                } else if (funct5 == 2) {  // LR.W
                    uint32_t old_val = bus->read(src1, 4);
                    if (rd != 0) reg[rd] = old_val;
                    if (!reserve) reserve = true;
                    if (trace) printf("%08x: %08x \t lr.w %s, %s, (%s)\n",
                        pc, inst, REG[rd], REG[rs2], REG[rs1]);
                } else if (funct5 == 3) {  // SC.W
                    if (reserve) {
                        bus->write(src1, 4, src2);
                        reg[rd] = 0;
                        reserve = false;
                    } else {
                        reg[rd] = 1;
                    }
                    if (trace) printf("%08x: %08x \t sc.w %s, %s, (%s)\n",
                        pc, inst, REG[rd], REG[rs2], REG[rs1]);
                } else {
                    throw Trap(2, inst);
                }
            } else {
                throw Trap(2, inst);
            }
            break;
        case 0xf:
            if (funct3 == 0) {
                op = "fence";
            } else if (funct3 == 1) {
                op = "fencei";
            } else {
                throw Trap(2, inst);
            }
            if (trace) printf("%08x: %08x \t %s\n", pc, inst, op.c_str());
            break;
        default:
            throw Trap(2, inst);  // Illegal instruction.
    }

    /* STEP4: update PC. */
    pc = next_pc;
    reg[0] = 0; // Reset zero.
}

void Cpu::handle_exception(const Trap& trap) {
    if (trace) printf("Trap %x: %x, %x\n", pc, trap.cause, trap.value);
    mstatus.bits.mpp = privilege;           // Set MPP to current mode.
    privilege = 3;                          // Set privilege to M-mode.
    mepc = pc;                              // Save current PC in mepc.
    mcause = trap.cause;                    // Set mcause to exception code.
    mtval = trap.value;                     // Set mtval to trap value.
    mstatus.bits.mpie = mstatus.bits.mie;   // MIE is stored to MPIE.
    mstatus.bits.mie = 0;                   // Clear up MIE.
    pc = mtvec & 0xfffffffc;
}

void Cpu::run() {
    try {
        execute();
    } catch (const Trap& trap) {
        handle_exception(trap);
    }
}

void Cpu::trace_on() {
    trace = true;
}
