//
// dev.cpp — UART 16550 and CLINT implementation
//

#include <cstdio>
#include <cstdlib>
#include <chrono>
#include "dev.h"

/*============================================================================
 * Uart — 16550A
 *============================================================================*/
uint32_t Uart::read(const uint32_t addr, const uint32_t /*n_bytes*/) {
    const bool dlab = (lcr & 0x80) != 0;
    uint32_t val = 0;

    switch (addr) {
    case 0:
        if (dlab) {
            val = dll;
        } else {
            /* RBR: return 0 when no data is pending, and clear rx_ready */
            val = rx_ready ? rx_byte : 0;
            rx_ready = false;
        }
        break;
    case 1:
        val = dlab ? dlm : ier;
        break;
    case 2:
        /* IIR: bit0=0 means an interrupt is pending.
         * Reading IIR clears the pending THR-empty condition, which is how
         * the driver learns it may refill the transmit FIFO.
         */
        if (irq()) {
            val = 0x02;          // THR empty interrupt pending
            ack_tx();
        } else {
            val = 0x01;          // no interrupt pending
        }
        break;
    case 3:
        val = lcr;
        break;
    case 4:
        val = mcr;
        break;
    case 5:
        /* LSR: bit5=THRE (transmit holding register empty)
         *      bit6=TEMT (transmitter empty)
         *      Transmission completes instantly here, so both stay 1.
         *      bit0=DR (data ready)
         */
        val = 0x60;
        if (rx_ready) val |= 0x01;
        break;
    case 6:
        /* MSR: bit4=CTS bit5=DSR bit6=RI bit7=DCD
         *      Report the peer as always ready so the driver
         *      completes its initialisation.
         */
        val = 0xb0;
        break;
    case 7:
        val = scr;
        break;
    default:
        val = 0;
        break;
    }

    return val;
}

void Uart::write(const uint32_t addr, const uint32_t /*n_bytes*/, const uint32_t data) {
    const bool dlab = (lcr & 0x80) != 0;
    const uint8_t b = static_cast<uint8_t>(data & 0xff);

    switch (addr) {
    case 0:
        if (dlab) {
            dll = b;
        } else {
            /* THR: emit to the terminal. Transmission is instantaneous, so
             * the FIFO drains at once and the THR-empty condition latches. */
            fputc(b, stdout);
            fflush(stdout);
            tx_pending = 0;
            if ((ier & 0x02) != 0) {
                tx_irq_latch = true;
            }
        }
        break;
    case 1:
        if (dlab) dlm = b;
        else {
            ier = b;
            /* Enabling the interrupt while the transmitter is idle should
             * raise a request so the driver can start feeding it. */
            if ((ier & 0x02) != 0 && tx_pending == 0) {
                tx_irq_latch = true;
            }
        }
        break;
    case 2:
        fcr = b;      // FIFO control
        break;
    case 3:
        lcr = b;      // line control (includes DLAB)
        break;
    case 4:
        mcr = b;
        break;
    case 5:
        break;        // LSR is read-only
    case 6:
        break;        // MSR is read-only
    case 7:
        scr = b;
        break;
    default:
        break;
    }
}

/*============================================================================
 * Clint
 *============================================================================*/
uint32_t Clint::read(const uint32_t addr, const uint32_t /*n_bytes*/) {
    if (addr == 0xbff8) {
        return static_cast<uint32_t>(mtime_cnt & 0xffffffff);
    }
    if (addr == 0xbffc) {
        return static_cast<uint32_t>((mtime_cnt >> 32) & 0xffffffff);
    }
    if (addr == 0x4000) {
        return static_cast<uint32_t>(mtimecmp & 0xffffffff);
    }
    if (addr == 0x4004) {
        return static_cast<uint32_t>((mtimecmp >> 32) & 0xffffffff);
    }
    return 0;
}

void Clint::write(const uint32_t addr, const uint32_t /*n_bytes*/, const uint32_t data) {
    if (addr == 0xbff8) {
        mtime_cnt = (mtime_cnt & 0xffffffff00000000ull) | data;
    } else if (addr == 0xbffc) {
        mtime_cnt = (static_cast<uint64_t>(data) << 32) | (mtime_cnt & 0xffffffff);
    } else if (addr == 0x4000) {
        mtimecmp = (mtimecmp & 0xffffffff00000000ull) | data;
    } else if (addr == 0x4004) {
        mtimecmp = (static_cast<uint64_t>(data) << 32) | (mtimecmp & 0xffffffff);
    }
}
