//

#include <cstdio>
#include "cpu.h"
#include "rvc.h"
#include <stdexcept>

Cpu::Cpu() {
    /* Initialize registers. */
    privilege = 3;
    pc = 0x80000000;
    reg.fill(0);

    /* Initialize CSRs. */
    mstatus.value = 0;
    medeleg = 0;
    mideleg = 0;
    mie = 0;
    mtvec = 0;
    mscratch = 0;
    mepc = 0;
    mcause = 0;
    mtval = 0;
    mip = 0;
    mcycle = 0;
    mcycleh = 0;

    stvec = 0;
    sscratch = 0;
    sepc = 0;
    satp = 0;

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
            val = 0x40141101;   // A+I+M+S+U
            break;
        case 0x302:  // medeleg
            val = medeleg;
            break;
        // Debug trigger CSRs (0x7A0-0x7BF)
        case 0x7a0: case 0x7a1: case 0x7a2: case 0x7a3:
        case 0x7a4: case 0x7a5: case 0x7a6: case 0x7a7:
        case 0x7a8: case 0x7a9: case 0x7aa: case 0x7ab:
        case 0x7ac: case 0x7ad: case 0x7ae: case 0x7af:
        case 0x7b0: case 0x7b1: case 0x7b2: case 0x7b3:
        case 0x7b4: case 0x7b5: case 0x7b6: case 0x7b7:
        case 0x7b8: case 0x7b9: case 0x7ba: case 0x7bb:
        case 0x7bc: case 0x7bd: case 0x7be: case 0x7bf:
            val = tdata[addr & 0x1f];
            break;
        // PMP configuration registers (0x3A0-0x3A3, only 4 on RV32)
        case 0x3a0: val = pmpcfg[0]; break;
        case 0x3a1: val = pmpcfg[1]; break;
        case 0x3a2: val = pmpcfg[2]; break;
        case 0x3a3: val = pmpcfg[3]; break;
        // PMP address registers (0x3B0-0x3BF, 16 in total)
        case 0x3b0: val = pmpaddr[0];  break;
        case 0x3b1: val = pmpaddr[1];  break;
        case 0x3b2: val = pmpaddr[2];  break;
        case 0x3b3: val = pmpaddr[3];  break;
        case 0x3b4: val = pmpaddr[4];  break;
        case 0x3b5: val = pmpaddr[5];  break;
        case 0x3b6: val = pmpaddr[6];  break;
        case 0x3b7: val = pmpaddr[7];  break;
        case 0x3b8: val = pmpaddr[8];  break;
        case 0x3b9: val = pmpaddr[9];  break;
        case 0x3ba: val = pmpaddr[10]; break;
        case 0x3bb: val = pmpaddr[11]; break;
        case 0x3bc: val = pmpaddr[12]; break;
        case 0x3bd: val = pmpaddr[13]; break;
        case 0x3be: val = pmpaddr[14]; break;
        case 0x3bf: val = pmpaddr[15]; break;
        case 0x303:  // mideleg
            val = mideleg;
            break;
        case 0x104:  // sie — delegated subset of mie
            val = mie & mideleg;
            break;
        case 0x144:  // sip — delegated subset of mip
            val = mip & mideleg;
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
        case 0x344:  // mip
            val = mip;
            break;
        // ---- Counters ----
        case 0xb00: val = mcycle;                 break;  // mcycle
        case 0xb80: val = mcycleh;                break;  // mcycleh
        case 0xb02: val = static_cast<uint32_t>(instret_cnt & 0xffffffff);        break;
        case 0xb82: val = static_cast<uint32_t>((instret_cnt >> 32) & 0xffffffff); break;
        case 0xc00: val = mcycle;                 break;  // cycle
        case 0xc80: val = mcycleh;                break;  // cycleh
        case 0xc01: val = static_cast<uint32_t>(mtime & 0xffffffff);         break;  // time
        case 0xc81: val = static_cast<uint32_t>((mtime >> 32) & 0xffffffff); break;  // timeh
        case 0xc02: val = static_cast<uint32_t>(instret_cnt & 0xffffffff);        break;  // instret
        case 0xc82: val = static_cast<uint32_t>((instret_cnt >> 32) & 0xffffffff); break;  // instreth
        case 0x100:  // sstatus
            val = mstatus.value & 0x800d6133;
            break;
        case 0x105:  // stvec
            val = stvec;
            break;
        case 0x140:  // sscratch
            val = sscratch;
            break;
        case 0x141:  // sepc
            val = sepc;
            break;
        case 0x142:  // scause
            val = scause;
            break;
        case 0x143:  // stval
            val = stval;
            break;
        case 0x180:  // satp
            val = satp;
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
        case 0x302:  // medeleg
            medeleg = val;
            break;
        // Debug trigger CSRs (0x7A0-0x7BF)
        case 0x7a0: case 0x7a1: case 0x7a2: case 0x7a3:
        case 0x7a4: case 0x7a5: case 0x7a6: case 0x7a7:
        case 0x7a8: case 0x7a9: case 0x7aa: case 0x7ab:
        case 0x7ac: case 0x7ad: case 0x7ae: case 0x7af:
        case 0x7b0: case 0x7b1: case 0x7b2: case 0x7b3:
        case 0x7b4: case 0x7b5: case 0x7b6: case 0x7b7:
        case 0x7b8: case 0x7b9: case 0x7ba: case 0x7bb:
        case 0x7bc: case 0x7bd: case 0x7be: case 0x7bf:
            tdata[addr & 0x1f] = val;
            break;
        // PMP configuration registers (keep only the low 8 valid bits)
        case 0x3a0: pmpcfg[0] = val & 0xFFu; break;
        case 0x3a1: pmpcfg[1] = val & 0xFFu; break;
        case 0x3a2: pmpcfg[2] = val & 0xFFu; break;
        case 0x3a3: pmpcfg[3] = val & 0xFFu; break;
        // PMP address registers
        case 0x3b0: pmpaddr[0]  = val; break;
        case 0x3b1: pmpaddr[1]  = val; break;
        case 0x3b2: pmpaddr[2]  = val; break;
        case 0x3b3: pmpaddr[3]  = val; break;
        case 0x3b4: pmpaddr[4]  = val; break;
        case 0x3b5: pmpaddr[5]  = val; break;
        case 0x3b6: pmpaddr[6]  = val; break;
        case 0x3b7: pmpaddr[7]  = val; break;
        case 0x3b8: pmpaddr[8]  = val; break;
        case 0x3b9: pmpaddr[9]  = val; break;
        case 0x3ba: pmpaddr[10] = val; break;
        case 0x3bb: pmpaddr[11] = val; break;
        case 0x3bc: pmpaddr[12] = val; break;
        case 0x3bd: pmpaddr[13] = val; break;
        case 0x3be: pmpaddr[14] = val; break;
        case 0x3bf: pmpaddr[15] = val; break;
        case 0x303:  // mideleg
            mideleg = val;
            break;
        case 0x104:  // sie — write only the delegated bits of mie
            mie = (mie & ~mideleg) | (val & mideleg);
            break;
        case 0x144:  // sip — write only the software-writable delegated bits
            mip = (mip & ~(mideleg & 0x222u)) | (val & mideleg & 0x222u);
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
        case 0x344:  // mip
            mip = val;
            break;
        // ---- Counters (writable; OpenSBI initialises them) ----
        case 0xb00: mcycle = val; break;
        case 0xb80: mcycleh = val; break;
        case 0xb02:
            instret_cnt = (instret_cnt & 0xffffffff00000000ull) | val;
            instret_written = true;
            break;
        case 0xb82:
            instret_cnt = (instret_cnt & 0xffffffffull) | (static_cast<uint64_t>(val) << 32);
            instret_written = true;
            break;
        case 0x100:  // sstatus
            mstatus.value &= ~0x800d6133;
            mstatus.value |= 0x800d6133 & val;
            break;
        case 0x105:  // stvec
            stvec = val;
            break;
        case 0x140:  // sscratch
            sscratch = val;
            break;
        case 0x141:  // sepc
            sepc = val;
            break;
        case 0x142:  // scause
            scause = val;
            break;
        case 0x180:  // satp
            satp = val;
            break;
        default:
            if (trace) printf("CSR: %x not implemented\n", addr);
            break;
    }
}


uint8_t PAGE_FAULT[4] = {12, 13, 15, 15};
uint8_t ACCESS_FAULT[4] = {1, 5, 7, 7};
uint32_t Cpu::mmu(const uint32_t v_addr, const AccessType access_type) const {
        // 1. Let a be satp.ppn * PAGESIZE, and let i = LEVELS − 1. (For Sv32, PAGESIZE=212 and LEVELS=2.)
        // 2. Let pte be the value of the PTE at address a+va.vpn[i] * PTESIZE. (For Sv32, PTESIZE=4.)
        // If accessing pte violates a PMA or PMP check, raise an access exception corresponding to
        // the original access type.
        // 3. If pte.v = 0, or if pte.r = 0 and pte.w = 1, stop and raise a page-fault exception corresponding
        // to the original access type.
        // 4. Otherwise, the PTE is valid. If pte.r = 1 or pte.x = 1, go to step 5. Otherwise, this PTE is a
        // pointer to the next level of the page table. Let i = i − 1. If i < 0, stop and raise a page-fault
        // exception corresponding to the original access type. Otherwise, let a = pte.ppn * PAGESIZE
        // and go to step 2.
        // 5. A leaf PTE has been found. Determine if the requested memory access is allowed by the
        // pte.r, pte.w, pte.x, and pte.u bits, given the current privilege mode and the value of the
        // SUM and MXR fields of the mstatus register. If not, stop and raise a page-fault exception
        // corresponding to the original access type.
        // 6. If i > 0 and pte.ppn[i − 1 : 0] ̸= 0, this is a misaligned superpage; stop and raise a page-fault
        // exception corresponding to the original access type.
        // 7. If pte.a = 0, or if the memory access is a store and pte.d = 0, either raise a page-fault
        // exception corresponding to the original access type.
        // 8. The translation is successful. The translated physical address is given as follows:
        // pa.pgoff = va.pgoff.
        // If i > 0, then this is a superpage translation and pa.ppn[i − 1 : 0] = va.vpn[i − 1 : 0].
        // pa.ppn[LEVELS − 1 : i] = pte.ppn[LEVELS − 1 : i].

        uint8_t pri_mode = privilege;
        if (mstatus.bits.mprv != 0 && (access_type == LOAD || access_type == STORE)) {
            // When MPRV=0, loads and stores behave as normal, using the translation and protection mechanisms
            // of the current privilege mode. When MPRV=1, load and store memory addresses are translated and
            // protected as though the current privilege mode were set to MPP.
            pri_mode = mstatus.bits.mpp;
        }
        if (satp >> 31 && pri_mode < 3) {
            // SV32
            // Get VPNs in the virtual address.
            uint32_t vpn[2];
            vpn[0] = v_addr >> 12 & 0x3ff;  // VPN[0]
            vpn[1] = v_addr >> 22 & 0x3ff;  // VPN[1]

            // Get root page table stored in satp.
            uint32_t ppn = satp & 0x3fffff;     // PPN in satp register.
            uint32_t pte_addr = ppn << 12;

            // Find page.
            bool page_found = false;
            uint32_t pte = 0;
            int i = 1;
            while (i >= 0) {
                try {
                    pte = bus->read(pte_addr + (vpn[i] << 2), 4);
                } catch ([[maybe_unused]] std::runtime_error& e) {
                    throw Trap(ACCESS_FAULT[access_type], v_addr);
                }

                const uint8_t v = pte & 1;
                const uint8_t r = pte >> 1 & 1;
                const uint8_t w = pte >> 2 & 1;
                const uint8_t x = pte >> 3 & 1;
                const uint8_t u = pte >> 4 & 1;
                const uint8_t a = pte >> 6 & 1;
                const uint8_t d = pte >> 7 & 1;

                // Valid page check.
                if (v == 0 || (r == 0 && w == 1)) throw Trap(PAGE_FAULT[access_type], v_addr);

                if (r == 1 || x == 1) {
                    page_found = true;

                    // Privilege Check.
                    if (pri_mode == 0 && u == 0 || pri_mode == 1 && mstatus.bits.sum == 0 && u == 1) {
                        throw Trap(PAGE_FAULT[access_type], v_addr);
                    }

                    // Permission Check
                    if (access_type == FETCH && x == 0
                            || access_type == LOAD && !(r == 1 || mstatus.bits.mxr == 1)
                            || (access_type == STORE || access_type == AMO) && w == 0) {
                        throw Trap(PAGE_FAULT[access_type], v_addr);
                    }

                    // A/D Bits Check
                    if (a == 0 || (d == 0 && (access_type == STORE || access_type == AMO))) {
                        throw Trap(PAGE_FAULT[access_type], v_addr);
                    }

                    // Superpage 4M alignment check.
                    if (i > 0 && pte >> 10 & 0x3ff) throw Trap(PAGE_FAULT[access_type], v_addr);

                    break;
                }

                ppn = pte >> 10 & 0x3fffff;
                pte_addr = ppn << 12;
                i -= 1;
            }
            if (page_found) {
                uint32_t offset = v_addr & 0xfff;

                if (i == 0) {
                    ppn = pte >> 10 & 0x3fffff;
                    return ppn << 12 | offset;
                }
                ppn = pte >> 20 & 0x3ff;
                return ppn << 22 | vpn[0] << 12 | offset;
            }
            throw Trap(PAGE_FAULT[access_type], v_addr);
        }
        return v_addr;
}

uint32_t Cpu::read_mem(const uint32_t addr, const uint32_t n_bytes, const AccessType access_type) const {
    uint32_t p_addr = mmu(addr, access_type);
    uint32_t data = 0;
    try {
        data = bus->read(p_addr, n_bytes);
    } catch ([[maybe_unused]] std::runtime_error& e) {
        uint32_t cause = 5;
        if (access_type == FETCH) cause = 1;
        else if (access_type == AMO) cause = 7;
        throw Trap(cause, addr);
    }
    return data;
}

void Cpu::write_mem(const uint32_t addr, const uint32_t n_bytes, const uint32_t data) const {
    uint32_t p_addr = mmu(addr, STORE);

    try {
        bus->write(p_addr, n_bytes, data);
    } catch ([[maybe_unused]] std::runtime_error& e) {
        throw Trap(7, addr);
    }
}


void Cpu::throw_illegal() {
    if (trace && !inst_traced) {
        printf("%08x: %08x \t illegal \t # %08x\n", pc, cur_inst, pc);
        inst_traced = true;
    }
    throw Trap(2, cur_inst);
}

void Cpu::execute() {
    /* STEP -1: hardware breakpoint check
     *   mcontrol (type=2): use the enable bits and access type in tdata1
     *   to decide whether the access at tdata2 triggers
     *   action=4 -> breakpoint exception (cause = 3)
     *   kind: 0=fetch, 1=load, 2=store
     */
    auto bp_hit = [&](const uint32_t vaddr, const int kind) -> bool {
        const uint32_t td1    = tdata[1];
        const uint32_t type   = (td1 >> 28) & 0xf;
        const uint32_t dmode  = (td1 >> 27) & 1;
        const uint32_t action = (td1 >> 12) & 0xf;   // 0=breakpoint, 1=exception
        const uint32_t match  = (td1 >> 7) & 0xf;
        const uint32_t chain  = (td1 >> 11) & 1;
        const bool m_bit = (td1 >> 6) & 1;
        const bool u_bit = (td1 >> 3) & 1;
        const bool exec_b  = (td1 >> 2) & 1;
        const bool store_b = (td1 >> 1) & 1;
        const bool load_b  = td1 & 1;

        if (dmode || chain || type != 2 || match != 0) return false;
        if (action != 0 && action != 4) return false;   // 0=breakpoint, 4=breakpoint (two encodings)
        if (!((privilege == 3 && m_bit) || (privilege == 0 && u_bit))) return false;

        const bool kind_ok =
            (kind == 0 && exec_b) || (kind == 1 && load_b) || (kind == 2 && store_b);
        return kind_ok && tdata[2] == vaddr;
    };

    if (bp_hit(pc, 0)) {
        if (trace) {
            printf("%08x: %08x \t exc \t # %08x\n", pc, 0u, 3u);
            inst_traced = true;
        }
        throw Trap(3, pc);
    }

    /* STEP0: interrupt check (before execution; mepc = current pc = next instruction) */
    {
        const uint32_t pending = mip & mie;
        if (pending != 0) {
            uint32_t cause = 0;
            for (uint32_t b = 0; b < 32; b++) {
                if (pending & (1u << b)) { cause = b; break; }
            }
            bool take = false;
            if (privilege <= 1 && mstatus.bits.sie && ((mideleg >> cause) & 1)) take = true;
            if (privilege == 3 && mstatus.bits.mie) take = true;
            if (take) throw Trap(0x80000000u | cause, 0);
        }
    }


    // Fetch alignment check: RVC requires 2-byte alignment (4 bytes only without C)
    if (pc & 0x1) {
        throw Trap(0, pc);   // instruction address misaligned
    }

    /* STEP1: fetch instruction. */
    uint32_t inst     = 0;   // expanded 32-bit instruction
    uint32_t inst_len = 4;   // original instruction length (2 or 4)
    const uint32_t raw = read_mem(pc & ~3u, 4, FETCH);
    const uint32_t half = (pc & 0x2) ? (raw >> 16) : (raw & 0xffff);

    // Detect a 16-bit compressed instruction (low 2 bits != 11)
    const bool is_rvc = (half & 0x3) != 0x3;
    if (is_rvc) {
        const uint32_t expanded = rvc_expand(static_cast<uint16_t>(half));
        if (expanded == 0) {
            if (trace) {
                printf("%08x: %04x     \t illegal \t # %08x\n", pc, half, pc);
                inst_traced = true;
            }
            cur_inst = half;
            throw Trap(2, half);
        }
        inst = expanded;
        inst_len = 2;
    } else {
        inst = (pc & 0x2) ? ((raw >> 16) | (read_mem((pc & ~3u) + 4, 2, FETCH) << 16)) : raw;
        inst_len = 4;
    }

    cur_inst = inst;       // record the current instruction
    inst_traced = false;   // this instruction has not been traced yet

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
    uint32_t next_pc = pc + inst_len;
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
                    } else throw_illegal();  // Illegal instruction.
                    break;
                case 1:
                    if (funct7 == 0) {  // SLL
                        if (rd != 0) reg[rd] = src1 << (src2 & 0x1f);
                        if (trace) op = "sll";
                    } else if (funct7 == 1) {  // MULH
                        if (rd != 0) reg[rd] =
                            signed_mul(static_cast<int32_t>(src1), static_cast<int32_t>(src2)) >> 32 & 0xffffffff;
                        if (trace) op = "mulh";
                    } else throw_illegal();  // Illegal instruction.
                    break;
                case 2:
                    if (funct7 == 0) {  // SLT
                        if (rd != 0) reg[rd] = static_cast<int32_t>(src1) < static_cast<int32_t>(src2) ? 1 : 0;
                        if (trace) op = "slt";
                    } else if (funct7 == 1) {  // MULHSU
                        if (rd != 0) reg[rd] = signed_mul(static_cast<int32_t>(src1), src2) >> 32 & 0xffffffff;
                        if (trace) op = "mulhsu";
                    } else throw_illegal();  // Illegal instruction.
                    break;
                case 3:
                    if (funct7 == 0) {  // SLTU
                        if (rd != 0) reg[rd] = src1 < src2 ? 1 : 0;
                        if (trace) op = "sltu";
                    } else if (funct7 == 1) {  // MULHU
                        if (rd != 0) reg[rd] =
                            signed_mul(src1, src2) >> 32 & 0xffffffff;
                        if (trace) op = "mulhu";
                    } else throw_illegal();  // Illegal instruction.
                    break;
                case 4:
                    if (funct7 == 0) {  // XOR
                        if (rd != 0) reg[rd] = src1 ^ src2;
                        if (trace) op = "xor";
                    } else if (funct7 == 1) {
                        if (rd != 0) reg[rd] = signed_div(static_cast<int32_t>(src1), static_cast<int32_t>(src2));
                        if (trace) op = "div";
                    } else throw_illegal();  // Illegal instruction.
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
                    } else throw_illegal();
                    break;
                case 6:
                    if (funct7 == 0) {  // OR
                        if (rd != 0) reg[rd] = src1 | src2;
                        if (trace) op = "or";
                    } else if (funct7 == 1) {  // REM
                        if (rd != 0) reg[rd] = signed_rem(static_cast<int32_t>(src1), static_cast<int32_t>(src2));
                        if (trace) op = "rem";
                    } else throw_illegal();  // Illegal instruction.
                    break;
                case 7:
                    if (funct7 == 0) {  // AND
                        if (rd != 0) reg[rd] = src1 & src2;
                        if (trace) op = "and";
                    } else if (funct7 == 1) {  // REMU
                        if (rd != 0) reg[rd] = signed_rem(src1, src2);
                        if (trace) op = "remu";
                    } else throw_illegal();  // Illegal instruction.
                    break;
                default:
                    throw_illegal();  // Illegal instruction.
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
                    } else throw_illegal();  // Illegal instruction.
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
                    } else throw_illegal();  // Illegal instruction.
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
                    throw_illegal();  // Illegal instruction.
            }
            if (trace) printf("%08x: %08x \t %s %s, %s, %d\n",
                pc, inst, op.c_str(), REG[rd], REG[rs1], static_cast<int32_t>(imm_i));
            break;
        case 0x3: { // I-type instructions (LOAD)
            if ((funct3 & 3) != 3) {
                const uint8_t n_bytes = 1 << (funct3 & 0x3);        // Get byte number.
                const uint32_t addr = src1 + imm_i;                 // Get address.
                if (bp_hit(addr, 1)) {
                    if (trace) { printf("%08x: %08x \t exc \t # %08x\n", pc, inst, 3u); inst_traced = true; }
                    throw Trap(3, addr);
                }
                const uint32_t data = read_mem(addr , n_bytes, LOAD);    // Load data.
                if (rd != 0) reg[rd] = funct3 & 0x4? data : signed_extend(data, n_bytes << 3);
                if (trace) {
                    op = n_bytes == 1? "lb" : n_bytes == 2? "lh" : "lw";
                    op += funct3 & 0x4? "u": "";
                    printf("%08x: %08x \t %s %s, %d(%s) # %08x: %08x\n",
                        pc, inst, op.c_str(), REG[rd], static_cast<int32_t>(imm_i), REG[rs1], addr, data);
                }
            } else throw_illegal();  // Illegal instruction.
            break;
        }
        case 0x23: {  // S-type instructions.
            if ((funct3 & 3) != 3) {
                const uint8_t n_bytes = 1 << (funct3 & 0x3);        // Get byte number.
                const uint32_t addr = src1 + imm_s;                 // Get address.
                if (bp_hit(addr, 2)) {
                    if (trace) { printf("%08x: %08x \t exc \t # %08x\n", pc, inst, 3u); inst_traced = true; }
                    throw Trap(3, addr);
                }
                write_mem(addr, n_bytes, src2);
                if (trace) {
                    op = n_bytes == 1? "sb" : n_bytes == 2? "sh" : "sw";
                    printf("%08x: %08x \t %s %s, %d(%s) # %08x <- %08x\n",
                        pc, inst, op.c_str(), REG[rs2], static_cast<int32_t>(imm_s), REG[rs1], addr, src2);
                }
            } else throw_illegal();  // Illegal instruction.
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
                    throw_illegal();  // Illegal instruction.
            }
            if (trace) {
                printf("%08x: %08x \t %s %s, %s, %d # %08x\n",
                    pc, inst, op.c_str(), REG[rs1], REG[rs2], static_cast<int32_t>(imm_b), next_pc);
                inst_traced = true;
            }
            break;
        }
        case 0x6f:  // JAL
            next_pc = pc + imm_j;
            if ((next_pc & 0x1) == 0) reg[rd] = pc + inst_len;   // return address uses the original length
            if (trace) {
                printf("%08x: %08x \t jal %s, %d # %08x\n",
                    pc, inst, REG[rd], static_cast<int32_t>(imm_j), next_pc);
                inst_traced = true;
            }
            break;
        case 0x67:  // JALR
            next_pc = (src1 + imm_i) & ~1u;
            if ((next_pc & 0x1) == 0) reg[rd] = pc + inst_len;   // return address uses the original length
            if (trace) {
                printf("%08x: %08x \t jal %s, %d(%s) # %08x\n",
                    pc, inst, REG[rd], static_cast<int32_t>(imm_i), REG[rs1], next_pc);
                inst_traced = true;   // already traced, do not emit an extra exc line
            }
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
                        if (trace) { printf("%08x: %08x \t ecall \t # %08x\n", pc, inst, pc); inst_traced = true; }
                        throw Trap(8 + privilege, pc);
                    case 0x00100073:  // EBREAK
                        if (trace) { printf("%08x: %08x \t ebreak \t # %08x\n", pc, inst, pc); inst_traced = true; }
                        throw Trap(3, pc);
                    case 0x30200073:  // MRET
                        privilege = mstatus.bits.mpp;           // Privilege mode is changed to MPP.
                        mstatus.bits.mie = mstatus.bits.mpie;   // MIE is set to MPIE.
                        mstatus.bits.mpie = 1;                  // MPIE is set to 1.
                        mstatus.bits.mpp = 0;                   // MPP is set to U-mode.
                        next_pc = mepc & ~1u;                    // PC is set to the value stored in mepc.
                        if (trace) op = "mret";
                        break;
                    case 0x10200073:  // SRET
                        // Same as the reference model: TSR=1 and in S-mode -> illegal
                        if (privilege == 1 && mstatus.bits.tsr) throw_illegal();
                        privilege        = mstatus.bits.spp;   // privilege <- SPP
                        mstatus.bits.sie  = mstatus.bits.spie; // SIE ← SPIE
                        mstatus.bits.spie = 1;                 // SPIE ← 1
                        mstatus.bits.spp  = 0;                 // SPP <- U-mode
                        next_pc = sepc & ~1u;                  // PC ← sepc
                        if (trace) op = "sret";
                        break;
                    case 0x10500073:  // WFI
                        if (trace) op = "wfi";
                        break;
                    default:
                        if (rd == 0 && funct7 == 9) {  // SFENCE.VMA
                            if (privilege == 1 && mstatus.bits.tvm) throw_illegal();
                            if (trace) op = "sfence.vma";
                        } else throw_illegal();
                }
                if (trace) printf("%08x: %08x \t %s\n", pc, inst, op.c_str());
            } else {
                uint16_t addr = imm_i & 0xfff;

                // Read-only CSRs (cycle/time/instret and their high halves): any write is illegal
                const bool csr_writes = (funct3 == 1 || funct3 == 4 || rs1 != 0);
                if (csr_writes &&
                    (addr == 0xc00 || addr == 0xc01 || addr == 0xc02 ||
                     addr == 0xc80 || addr == 0xc81 || addr == 0xc82)) {
                    throw_illegal();
                }

                // Privilege check: CSR address bits [9:8] above the current level -> illegal
                if (((addr >> 8) & 3) > privilege) {
                    throw_illegal();
                }

                // With TVM=1, S-mode reads/writes of satp -> illegal
                if (privilege == 1 && mstatus.bits.tvm && addr == 0x180) {
                    throw_illegal();
                }

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
                        throw_illegal();  // Illegal instruction.
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
                if (uint8_t funct5 = funct7 >> 2; (funct5 & 0x3) == 0) {
                    uint32_t old_val = read_mem(src1, 4, AMO);
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
                            throw_illegal();
                    }
                    write_mem(src1, 4, new_val);
                    if (rd != 0) reg[rd] = old_val;
                    if (trace) printf("%08x: %08x \t %s %s, %s, (%s)\n",
                        pc, inst, op.c_str(), REG[rd], REG[rs2], REG[rs1]);
                } else if (funct5 == 1) {  // AMOSWAP.W
                    uint32_t old_val = read_mem(src1, 4, AMO);
                    write_mem(src1, 4, src2);
                    if (rd != 0) reg[rd] = old_val;
                    if (trace) printf("%08x: %08x \t amoswap.w %s, %s, (%s)\n",
                        pc, inst, REG[rd], REG[rs2], REG[rs1]);
                } else if (funct5 == 2) {  // LR.W
                    uint32_t old_val = read_mem(src1, 4, AMO);
                    if (rd != 0) reg[rd] = old_val;
                    if (!reserve) reserve = true;
                    if (trace) printf("%08x: %08x \t lr.w %s, %s, (%s)\n",
                        pc, inst, REG[rd], REG[rs2], REG[rs1]);
                } else if (funct5 == 3) {  // SC.W
                    if (reserve) {
                        write_mem(src1, 4, src2);
                        reg[rd] = 0;
                        reserve = false;
                    } else {
                        reg[rd] = 1;
                    }
                    if (trace) printf("%08x: %08x \t sc.w %s, %s, (%s)\n",
                        pc, inst, REG[rd], REG[rs2], REG[rs1]);
                } else {
                    throw_illegal();
                }
            } else {
                throw_illegal();
            }
            break;
        case 0xf:
            if (funct3 == 0) {  // FENCE
                op = "fence";
            } else if (funct3 == 1) {  // FENCE.I
                op = "fencei";
            } else {
                throw_illegal();
            }
            if (trace) printf("%08x: %08x \t %s\n", pc, inst, op.c_str());
            break;
        default:
            // Illegal instructions must also emit a standard trace line, otherwise traces desync
            if (trace) {
                printf("%08x: %08x \t illegal \t # %08x\n", pc, inst, pc);
                inst_traced = true;
            }
            throw_illegal();  // Illegal instruction.
    }

    /* STEP4: target alignment check (RISC-V: misaligned target -> cause=0) */
    // With the C extension a jump target only needs 2-byte alignment
    if (next_pc & 0x1) {
        throw Trap(0, next_pc);   // Instruction address misaligned
    }

    /* STEP5: update PC. */
    pc = next_pc;
    reg[0] = 0; // Reset zero.

    /* STEP5.1: counter increment
     *   If this instruction wrote minstret / minstreth (0xB02/0xB82) the
     *   increment is suppressed: the spec requires that a csrw minstret is
     *   followed immediately by reading back the written value.
     */
    mcycle += 1;
    if (!instret_written) {
        instret_cnt += 1;
    }
    instret_written = false;

}

void Cpu::handle_exception(const Trap& trap) {
    if (trace) {
        // Only emit an extra exc line when the instruction itself was not traced
        //   (ecall/ebreak trace themselves; page faults are raised inside load/store)
        if (!inst_traced)
            printf("%08x: %08x \t exc \t # %08x\n", pc, cur_inst, trap.cause);
        printf("Trap %x: %x, %x\n", pc, trap.cause, trap.value);
    }

    const uint32_t cause = trap.cause;

    // Interrupt (cause bit31=1) -> check mideleg; synchronous exception -> medeleg
    bool delegated;
    if (cause & 0x80000000u) {
        delegated = (privilege <= 1) && ((mideleg >> (cause & 0x7fffffffu)) & 1);
    } else {
        delegated = (privilege <= 1) && (cause < 32) && ((medeleg >> cause) & 1);
    }

    if (delegated) {
        // ---- Delegated to S-mode ----
        mstatus.bits.spp  = privilege;          // SPP <- current mode
        privilege         = 1;                  // enter S-mode
        sepc              = pc;
        scause            = cause;
        stval             = trap.value;
        mstatus.bits.spie = mstatus.bits.sie;   // SPIE ← SIE
        mstatus.bits.sie  = 0;
        // stvec low 2 bits: 0 -> Direct, 1 -> Vectored
        // Vectored applies to interrupts only; synchronous exceptions are not offset
        pc = (stvec & ~3u) +
             (((stvec & 0x3) == 1 && (cause & 0x80000000u))
                ? ((cause & 0x7fffffffu) << 2) : 0);
    } else {
        // ---- Delegated to M-mode ----
        mstatus.bits.mpp  = privilege;
        privilege         = 3;
        mepc              = pc;
        mcause            = cause;
        mtval             = trap.value;
        mstatus.bits.mpie = mstatus.bits.mie;
        mstatus.bits.mie  = 0;
        pc = (mtvec & ~3u) +
             (((mtvec & 0x3) == 1 && (cause & 0x80000000u))
                ? ((cause & 0x7fffffffu) << 2) : 0);
    }
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
