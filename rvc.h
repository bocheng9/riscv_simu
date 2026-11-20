//
// rvc.h — RVC (C extension) 16-bit compressed instruction expansion
//
// Design: a pure combinational function mapping a 16-bit compressed
// instruction to its equivalent 32-bit instruction word.
// Encodings that cannot be expanded return 0 and are treated as illegal.
//
// Covers RV32C (excluding floating-point forms such as
// C.FLD / C.FLDSP / C.FSD / C.FSDSP).
//

#ifndef RVSIMU_V3_RVC_H
#define RVSIMU_V3_RVC_H

#include <cstdint>

/* Expand a 16-bit compressed instruction; returns 0 if the encoding is illegal */
uint32_t rvc_expand(uint16_t c);

#endif //RVSIMU_V3_RVC_H
