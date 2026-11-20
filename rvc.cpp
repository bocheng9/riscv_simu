//
// rvc.cpp — RV32C compressed instruction expansion
//
// Reference: RISC-V Unprivileged ISA, Volume I, Chapter 16 "C Standard Extension"
// Everything is expanded to equivalent 32-bit instructions so the
// existing 32-bit decoder can be reused.
//

#include "rvc.h"

namespace {

/* Common encoding helpers */
inline uint32_t enc_i(uint32_t imm, uint32_t rs1, uint32_t f3, uint32_t rd, uint32_t op) {
    return ((imm & 0xfff) << 20) | ((rs1 & 0x1f) << 15) | ((f3 & 7) << 12)
         | ((rd & 0x1f) << 7) | (op & 0x7f);
}
inline uint32_t enc_r(uint32_t f7, uint32_t rs2, uint32_t rs1, uint32_t f3, uint32_t rd, uint32_t op) {
    return ((f7 & 0x7f) << 25) | ((rs2 & 0x1f) << 20) | ((rs1 & 0x1f) << 15)
         | ((f3 & 7) << 12) | ((rd & 0x1f) << 7) | (op & 0x7f);
}
inline uint32_t enc_s(uint32_t imm, uint32_t rs2, uint32_t rs1, uint32_t f3, uint32_t op) {
    const uint32_t i = imm & 0xfff;
    return ((i >> 5) << 25) | ((rs2 & 0x1f) << 20) | ((rs1 & 0x1f) << 15)
         | ((f3 & 7) << 12) | ((i & 0x1f) << 7) | (op & 0x7f);
}
inline uint32_t enc_b(uint32_t imm, uint32_t rs2, uint32_t rs1, uint32_t f3) {
    const uint32_t i = imm & 0x1fff;
    return (((i >> 12) & 1) << 31) | (((i >> 5) & 0x3f) << 25) | ((rs2 & 0x1f) << 20)
         | ((rs1 & 0x1f) << 15) | ((f3 & 7) << 12) | (((i >> 1) & 0xf) << 8)
         | (((i >> 11) & 1) << 7) | 0x63;
}
inline uint32_t enc_j(uint32_t imm, uint32_t rd) {
    const uint32_t i = imm & 0x1fffff;
    return (((i >> 20) & 1) << 31) | (((i >> 1) & 0x3ff) << 21) | (((i >> 11) & 1) << 20)
         | (((i >> 12) & 0xff) << 12) | ((rd & 0x1f) << 7) | 0x6f;
}
inline uint32_t enc_u(uint32_t imm20, uint32_t rd, uint32_t op) {
    return ((imm20 & 0xfffff) << 12) | ((rd & 0x1f) << 7) | (op & 0x7f);
}

/* Sign-extension helpers */
inline int32_t sext(uint32_t v, int bits) {
    const uint32_t sign = 1u << (bits - 1);
    return static_cast<int32_t>((v ^ sign) - sign);
}

}  // namespace

uint32_t rvc_expand(const uint16_t ci) {
    const uint32_t c    = ci;
    const uint32_t quad = c & 0x3;
    const uint32_t f3   = (c >> 13) & 0x7;

    /* Register numbers in compressed formats
     *   rd   : bit[11:7]  - C.ADDI / C.LI / C.LUI / C.ADDI16SP / C.SLLI / C.LWSP / C.JALR ...
     *   rs2  : bit[6:2]   - C.MV / C.ADD / C.SWSP ...
     *   rd'  : bit[4:2]+8 - CIW / CL / CS formats
     */
    const uint32_t rd_  = (c >> 7) & 0x1f;   // ★ bit[11:7]
    const uint32_t rs2_ = (c >> 2) & 0x1f;
    const uint32_t rd_p = 8 + ((c >> 2) & 0x7);   // rd' of CIW/CL formats (x8..x15)
    const uint32_t rs1_p= 8 + ((c >> 7) & 0x7);   // x8..x15
    const uint32_t rs2_p= 8 + ((c >> 2) & 0x7);

    switch (quad) {
    //========================================================================
    // Quadrant 0
    //========================================================================
    case 0:
        switch (f3) {
        case 0: {   // C.ADDI4SPN → addi rd', x2, nzuimm
            const uint32_t nz = (((c >> 7) & 0xf) << 6) | (((c >> 11) & 0x3) << 4)
                              | (((c >> 5) & 0x1) << 3) | (((c >> 6) & 0x1) << 2);
            if (nz == 0) return 0;      // all zero -> reserved (illegal)
            return enc_i(nz, 2, 0x0, rd_p, 0x13);
        }
        case 2: {   // C.LW → lw rd', offset(rs1')
            const uint32_t off = (((c >> 10) & 0x7) << 3) | (((c >> 6) & 0x1) << 2)
                               | (((c >> 5) & 0x1) << 6);
            return enc_i(off, rs1_p, 0x2, rd_p, 0x03);
        }
        case 6: {   // C.SW → sw rs2', offset(rs1')
            const uint32_t off = (((c >> 10) & 0x7) << 3) | (((c >> 6) & 0x1) << 2)
                               | (((c >> 5) & 0x1) << 6);
            return enc_s(off, rs2_p, rs1_p, 0x2, 0x23);
        }
        default:
            return 0;   // C.FLD / C.FLW(64) / reserved
        }

    //========================================================================
    // Quadrant 1
    //========================================================================
    case 1:
        switch (f3) {
        case 0: {   // C.NOP / C.ADDI → addi rd, rd, nzimm
            const int32_t imm = sext((((c >> 12) & 0x1) << 5) | ((c >> 2) & 0x1f), 6);
            if (rd_ == 0) {
                if (imm == 0) return 0x00000013;   // c.nop
                return 0;                          // non-zero nop -> reserved
            }
            return enc_i(static_cast<uint32_t>(imm), rd_, 0x0, rd_, 0x13);
        }
        case 1:     // C.JAL (RV32) → jal x1, offset
                    // C.ADDIW (RV64) -> this design is RV32, handled as C.JAL
        {
            const int32_t imm = sext((((c >> 12) & 0x1) << 11) | (((c >> 11) & 0x1) << 4)
                                   | (((c >> 9) & 0x3) << 8) | (((c >> 8) & 0x1) << 10)
                                   | (((c >> 7) & 0x1) << 6) | (((c >> 6) & 0x1) << 7)
                                   | (((c >> 3) & 0x7) << 1) | (((c >> 2) & 0x1) << 5), 12);
            return enc_j(static_cast<uint32_t>(imm), 1);
        }
        case 2: {   // C.LI → addi rd, x0, imm
            const int32_t imm = sext((((c >> 12) & 0x1) << 5) | ((c >> 2) & 0x1f), 6);
            if (rd_ == 0) return 0;   // reserved
            return enc_i(static_cast<uint32_t>(imm), 0, 0x0, rd_, 0x13);
        }
        case 3: {   // C.ADDI16SP (rd==2) / C.LUI
            const int32_t imm6 = sext((((c >> 12) & 0x1) << 5) | ((c >> 2) & 0x1f), 6);
            if (rd_ == 2) {
                // C.ADDI16SP → addi x2, x2, nzimm（imm != 0）
                if (imm6 == 0) return 0;
                const int32_t imm = sext((((c >> 12) & 0x1) << 9) | (((c >> 6) & 0x1) << 4)
                                       | (((c >> 5) & 0x1) << 6) | (((c >> 3) & 0x3) << 7)
                                       | (((c >> 2) & 0x1) << 5), 10);
                return enc_i(static_cast<uint32_t>(imm), 2, 0x0, 2, 0x13);
            }
            if (imm6 == 0) return 0;   // reserved
            // C.LUI → lui rd, nzimm[17:12]
            return enc_u(static_cast<uint32_t>(imm6) & 0xfffff, rd_, 0x37);
        }
        case 4: {   // several forms (C.SRLI/C.SRAI/C.ANDI/C.SUB/C.XOR/C.OR/C.AND)
            const uint32_t f2  = (c >> 10) & 0x3;
            // CB format: rd' is in bit[9:7], rs2' is in bit[4:2]
            const uint32_t rd1 = 8 + ((c >> 7) & 0x7);
            const uint32_t r2  = 8 + ((c >> 2) & 0x7);
            if (f2 == 0 || f2 == 1) {
                // C.SRLI / C.SRAI → srli/srai rd', rd', shamt
                const uint32_t shamt = (((c >> 12) & 0x1) << 5) | ((c >> 2) & 0x1f);
                const uint32_t f7    = (f2 == 1) ? 0x20 : 0x00;
                return enc_r(f7, shamt, rd1, 0x5, rd1, 0x13);
            }
            if (f2 == 2) {   // C.ANDI → andi rd', rd', imm
                const int32_t imm = sext((((c >> 12) & 0x1) << 5) | ((c >> 2) & 0x1f), 6);
                return enc_i(static_cast<uint32_t>(imm), rd1, 0x7, rd1, 0x13);
            }
            // f2 == 3：C.SUB / C.XOR / C.OR / C.AND
            //   Discriminator is bit[6:5] (2 bits); bit12 must be 0 on RV32
            //     00=SUB  01=XOR  10=OR  11=AND
            const uint32_t b12  = (c >> 12) & 1;
            const uint32_t sel  = (c >> 5) & 0x3;
            if (b12 != 0) return 0;      // reserved encoding on RV32
            switch (sel) {
                case 0: return enc_r(0x20, r2, rd1, 0x0, rd1, 0x33);  // C.SUB
                case 1: return enc_r(0x00, r2, rd1, 0x4, rd1, 0x33);  // C.XOR
                case 2: return enc_r(0x00, r2, rd1, 0x6, rd1, 0x33);  // C.OR
                case 3: return enc_r(0x00, r2, rd1, 0x7, rd1, 0x33);  // C.AND
            }
            return 0;
        }
        case 5: {   // C.J → jal x0, offset
            const int32_t imm = sext((((c >> 12) & 0x1) << 11) | (((c >> 11) & 0x1) << 4)
                                   | (((c >> 9) & 0x3) << 8) | (((c >> 8) & 0x1) << 10)
                                   | (((c >> 7) & 0x1) << 6) | (((c >> 6) & 0x1) << 7)
                                   | (((c >> 3) & 0x7) << 1) | (((c >> 2) & 0x1) << 5), 12);
            return enc_j(static_cast<uint32_t>(imm), 0);
        }
        case 6: {   // C.BEQZ → beq rs1', x0, offset
            const int32_t imm = sext((((c >> 12) & 0x1) << 8) | (((c >> 10) & 0x3) << 3)
                                   | (((c >> 5) & 0x3) << 6) | (((c >> 3) & 0x3) << 1)
                                   | (((c >> 2) & 0x1) << 5), 9);
            return enc_b(static_cast<uint32_t>(imm), 0, rs1_p, 0x0);
        }
        case 7: {   // C.BNEZ → bne rs1', x0, offset
            const int32_t imm = sext((((c >> 12) & 0x1) << 8) | (((c >> 10) & 0x3) << 3)
                                   | (((c >> 5) & 0x3) << 6) | (((c >> 3) & 0x3) << 1)
                                   | (((c >> 2) & 0x1) << 5), 9);
            return enc_b(static_cast<uint32_t>(imm), 0, rs1_p, 0x1);
        }
        }
        return 0;

    //========================================================================
    // Quadrant 2
    //========================================================================
    case 2:
        switch (f3) {
        case 0: {   // C.SLLI → slli rd, rd, shamt
            const uint32_t shamt = (((c >> 12) & 0x1) << 5) | ((c >> 2) & 0x1f);
            if (rd_ == 0) return 0;
            return enc_r(0x00, shamt, rd_, 0x1, rd_, 0x13);
        }
        case 2: {   // C.LWSP → lw rd, offset(x2)
            if (rd_ == 0) return 0;
            const uint32_t off = (((c >> 12) & 0x1) << 5) | (((c >> 4) & 0x7) << 2)
                               | (((c >> 2) & 0x3) << 6);
            return enc_i(off, 2, 0x2, rd_, 0x03);
        }
        case 4:
            if (((c >> 12) & 1) == 0) {
                if (rs2_ == 0) {   // C.JR → jalr x0, 0(rs1)
                    if (rd_ == 0) return 0;
                    return enc_i(0, rd_, 0x0, 0, 0x67);
                }
                // C.MV → add rd, x0, rs2
                return enc_r(0x00, rs2_, 0, 0x0, rd_, 0x33);
            } else {
                if (rs2_ == 0) {
                    if (rd_ == 0) return 0x00100073;   // C.EBREAK
                    // C.JALR → jalr x1, 0(rs1)
                    return enc_i(0, rd_, 0x0, 1, 0x67);
                }
                // C.ADD → add rd, rd, rs2
                return enc_r(0x00, rs2_, rd_, 0x0, rd_, 0x33);
            }
        case 6: {   // C.SWSP → sw rs2, offset(x2)
            const uint32_t off = (((c >> 9) & 0xf) << 2) | (((c >> 7) & 0x3) << 6);
            return enc_s(off, rs2_, 2, 0x2, 0x23);
        }
        default:
            return 0;   // C.FLDSP / C.FLWSP / reserved
        }

    default:
        return 0;
    }
}
