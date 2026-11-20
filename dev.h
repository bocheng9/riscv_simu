//
// dev.h — MMIO devices: UART 16550 and CLINT timer
//

#ifndef RVSIMU_V3_DEV_H
#define RVSIMU_V3_DEV_H

#include "bus.h"

/*============================================================================
 * Uart — 16550A compatible implementation
 *
 * Register map (offset):
 *   0x00  RBR / THR / DLL   (DLAB=0: read RBR / write THR; DLAB=1: DLL)
 *   0x01  IER / DLM         (DLAB=0: interrupt enable; DLAB=1: DLM)
 *   0x02  IIR (read) / FCR (write)
 *   0x03  LCR               (line control, bit7 = DLAB)
 *   0x04  MCR               (modem control)
 *   0x05  LSR               (line status)
 *   0x06  MSR               (modem status)
 *   0x07  SCR               (scratch register)
 *
 * To let the Linux 8250 driver probe the port and transmit normally,
 * LSR/MSR must report sensible values and IIR must indicate
 * "no interrupt pending".
 *============================================================================*/
class Uart: public Mmio {
    uint8_t ier = 0;      // interrupt enable
    uint8_t lcr = 0;      // line control (bit7 = DLAB)
    uint8_t mcr = 0;      // modem control
    uint8_t scr = 0;      // scratch
    uint8_t fcr = 0;      // FIFO control
    uint8_t dll = 0;      // divisor latch low byte
    uint8_t dlm = 0;      // divisor latch high byte

    /* RX buffer (no incoming data is produced currently) */
    bool rx_ready = false;
    uint8_t rx_byte = 0;

    /* TX FIFO occupancy. Characters are transmitted immediately, so the
     * occupancy returns to zero as soon as THR is written; what matters is
     * the latched interrupt condition below. */
    unsigned tx_pending = 0;

    /* Latched THR-empty interrupt. Set when a character is written while the
     * THR-empty interrupt is enabled, cleared when the driver reads IIR.
     * This mirrors the 16550 behaviour and avoids an interrupt storm. */
    bool tx_irq_latch = false;

    /* MSR delta flags: reported once after reset, then cleared */
    bool msr_delta = true;

public:
    static constexpr unsigned TX_FIFO_DEPTH = 256;

    uint32_t read(uint32_t addr, uint32_t n_bytes) override;
    void     write(uint32_t addr, uint32_t n_bytes, uint32_t data) override;

    /* Whether unread data is available (always false for now) */
    [[nodiscard]] bool has_rx() const { return rx_ready; }

    /* Interrupt request line: asserted while the THR-empty condition is
     * latched and the driver has enabled it in IER. */
    [[nodiscard]] bool irq() const {
        return (ier & 0x02) != 0 && tx_irq_latch;
    }

    /* Called when the CPU acknowledges the interrupt by reading IIR */
    void ack_tx() { tx_irq_latch = false; tx_pending = 0; }
};

/*============================================================================
 * Clint — core local interruptor
 *   0x4000 mtimecmp  (64-bit, low/high words)
 *   0x4004 mtimecmp_h
 *   0xBFF8 mtime     (64-bit, low/high words)
 *   0xBFFC mtime_h
 *
 * mtime advances with the simulated instruction count rather than wall-clock
 * time. Tying it to host time makes the clock advance by a few million ticks
 * per simulated second while the model retires hundreds of millions of
 * instructions, so timer interrupts either never arrive or arrive in bursts.
 * One tick per MTIME_DIV instructions keeps the ratio fixed and the clock
 * monotonic.
 *============================================================================*/
class Clint: public Mmio {
public:
    /* Instructions per mtime tick (CPU 100MHz / timer 10MHz) */
    static constexpr uint64_t MTIME_DIV = 10;

private:
    uint64_t mtime_cnt = 0;    // ticks since reset
    uint64_t mtimecmp  = ~0ull; // disarmed after reset, as on real hardware

public:
    Clint() = default;

    [[nodiscard]] uint64_t mtime() const { return mtime_cnt; }

    /* Advance the clock by one tick (called every MTIME_DIV instructions) */
    void tick() { mtime_cnt++; }

    [[nodiscard]] bool pending() const { return mtime_cnt >= mtimecmp; }

    uint32_t read(uint32_t addr, uint32_t n_bytes) override;
    void     write(uint32_t addr, uint32_t n_bytes, uint32_t data) override;
};

#endif //RVSIMU_V3_DEV_H
