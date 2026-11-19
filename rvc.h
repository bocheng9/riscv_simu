//
// rvc.h — RVC（C 扩展）16 位压缩指令展开为 32 位指令
//
// 设计：纯组合函数，输入 16 位压缩指令，输出等价的 32 位指令字。
//      无法展开的编码返回 0，由上层判定为非法指令。
//
// 覆盖 RV32C（不含 C.FLD/FLDSP/FSD/FSDSP 等浮点压缩指令）。
//

#ifndef RVSIMU_V3_RVC_H
#define RVSIMU_V3_RVC_H

#include <cstdint>

/* 展开 16 位压缩指令；返回 0 表示非法编码 */
uint32_t rvc_expand(uint16_t c);

#endif //RVSIMU_V3_RVC_H
