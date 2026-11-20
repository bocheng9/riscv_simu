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

    /* TX FIFO watermark: bytes written since the last interrupt service.
     * The Linux 8250 driver enables the THR-empty interrupt and refills the
     * FIFO from its ISR. Without the interrupt the transmit path stalls once
     * the FIFO is full, which is what makes user-space output stop after
     * exactly one FIFO of data. */
    unsigned tx_pending = 0;

public:
    static constexpr unsigned TX_FIFO_DEPTH = 256;

    uint32_t read(uint32_t addr, uint32_t n_bytes) override;
    void     write(uint32_t addr, uint32_t n_bytes, uint32_t data) override;

    /* Whether unread data is available (always false for now) */
    [[nodiscard]] bool has_rx() const { return rx_ready; }

    /* Interrupt request line, asserted while the transmitter wants service */
    [[nodiscard]] bool irq() const {
        return (ier & 0x02) != 0 && tx_pending < TX_FIFO_DEPTH;
    }

    /* Called when the CPU acknowledges the interrupt by reading IIR */
    void ack_tx() { tx_pending = 0; }
};

/*============================================================================
 * Clint — core local interruptor
 *   0x4000 mtimecmp  (64-bit, low/high words)
 *   0x4004 mtimecmp_h
 *   0xBFF8 mtime     (64-bit, low/high words)
 *   0xBFFC mtime_h
 *============================================================================*/
class Clint: public Mmio {
    uint64_t mtime_base    = 0;
    uint64_t host_start_ns = 0;
    uint64_t mtimecmp      = ~0ull;

public:
    Clint();

    [[nodiscard]] uint64_t mtime() const;
    [[nodiscard]] bool pending() const { return mtime() >= mtimecmp; }
    void clear_pending() { mtimecmp = ~0ull; }

    uint32_t read(uint32_t addr, uint32_t n_bytes) override;
    void     write(uint32_t addr, uint32_t n_bytes, uint32_t data) override;
};

#endif //RVSIMU_V3_DEV_H
