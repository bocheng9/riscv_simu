//
// dev.h — MMIO 设备：UART 16550 简化模型 与 CLINT 定时器
//

#ifndef RVSIMU_V3_DEV_H
#define RVSIMU_V3_DEV_H

#include "bus.h"

/*============================================================================
 * Uart — 16550 兼容的最小实现
 *   偏移 0x00 THR/RBR：发送写入字符，接收恒为 0
 *   偏移 0x05 LSR    ：读恒为 0x20（THR 空，可继续写）
 *   其他偏移读 0，写忽略
 *============================================================================*/
class Uart: public Mmio {
public:
    uint32_t read(uint32_t addr, uint32_t n_bytes) override;
    void     write(uint32_t addr, uint32_t n_bytes, uint32_t data) override;
};

/*============================================================================
 * Clint — 核心本地中断器
 *   0x4000 mtimecmp  (64 位，低/高字)
 *   0x4008 mtimecmp_h
 *   0xBFF8 mtime     (64 位，低/高字)
 *   0xBFFC mtime_h
 *
 *   mtime 按真实时间推进（10MHz，1 tick = 100ns），
 *   每步由外部调用 tick() 把当前 mtime 推给 CPU 的 time CSR。
 *============================================================================*/
class Clint: public Mmio {
    uint64_t mtime_base   = 0;   // 软件写入的基准值
    uint64_t host_start_ns = 0;  // 起始时刻
    uint64_t mtimecmp     = ~0ull;

public:
    Clint();

    /* 当前 mtime（10MHz 计数） */
    [[nodiscard]] uint64_t mtime() const;

    /* 定时器是否已到期（mtime >= mtimecmp） */
    [[nodiscard]] bool pending() const { return mtime() >= mtimecmp; }

    /* 清除到期状态（写 mtimecmp 由软件完成） */
    void clear_pending() { mtimecmp = ~0ull; }

    uint32_t read(uint32_t addr, uint32_t n_bytes) override;
    void     write(uint32_t addr, uint32_t n_bytes, uint32_t data) override;
};

#endif //RVSIMU_V3_DEV_H
