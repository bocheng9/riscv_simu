//
// plic.cpp — Platform-Level Interrupt Controller implementation
//

#include "plic.h"

void Plic::set_irq(const uint32_t source, const bool level) {
    if (source == 0 || source >= NUM_SOURCES) return;
    const uint32_t word = source / 32;
    const uint32_t bit = 1u << (source % 32);
    if (level) {
        pending[word] |= bit;
    } else {
        pending[word] &= ~bit;
    }
}

uint32_t Plic::best_source(const uint32_t context) const {
    if (context >= NUM_CONTEXTS) return 0;

    uint32_t best = 0;
    uint32_t best_prio = 0;
    for (uint32_t w = 0; w < NUM_SOURCES / 32; w++) {
        uint32_t bits = pending[w] & enable[context][w];
        while (bits != 0) {
            const uint32_t b = static_cast<uint32_t>(__builtin_ctz(bits));
            const uint32_t src = w * 32 + b;
            bits &= bits - 1;
            if (src == 0) continue;
            const uint32_t prio = priority[src];
            if (prio > threshold[context] && prio > best_prio) {
                best = src;
                best_prio = prio;
            }
        }
    }
    return best;
}

uint32_t Plic::read(const uint32_t addr, const uint32_t /*n_bytes*/) {
    /* priority: 0x000000 - 0x000FFF */
    if (addr < 0x1000) {
        const uint32_t src = addr / 4;
        return (src < NUM_SOURCES) ? priority[src] : 0;
    }

    /* pending: 0x001000 - 0x00107F */
    if (addr >= 0x1000 && addr < 0x1080) {
        const uint32_t w = (addr - 0x1000) / 4;
        return (w < NUM_SOURCES / 32) ? pending[w] : 0;
    }

    /* enable: 0x002000 + 0x80 * context */
    if (addr >= 0x2000 && addr < 0x2000 + 0x80 * NUM_CONTEXTS) {
        const uint32_t off = addr - 0x2000;
        const uint32_t ctx = off / 0x80;
        const uint32_t w = (off % 0x80) / 4;
        if (ctx < NUM_CONTEXTS && w < NUM_SOURCES / 32) return enable[ctx][w];
        return 0;
    }

    /* threshold / claim: 0x200000 + 0x1000 * context */
    if (addr >= 0x200000 && addr < 0x200000 + 0x1000 * NUM_CONTEXTS) {
        const uint32_t off = addr - 0x200000;
        const uint32_t ctx = off / 0x1000;
        const uint32_t reg = off % 0x1000;
        if (ctx >= NUM_CONTEXTS) return 0;

        if (reg == 0x0) {
            return threshold[ctx];
        }
        if (reg == 0x4) {
            /* Claim: return the winning source and mark it in service */
            const uint32_t src = best_source(ctx);
            if (src != 0) {
                const uint32_t w = src / 32;
                const uint32_t bit = 1u << (src % 32);
                pending[w] &= ~bit;      // gateway lowers until completion
                claimed[ctx] = src;
            }
            return src;
        }
        return 0;
    }

    return 0;
}

void Plic::write(const uint32_t addr, const uint32_t /*n_bytes*/, const uint32_t data) {
    /* priority */
    if (addr < 0x1000) {
        const uint32_t src = addr / 4;
        if (src > 0 && src < NUM_SOURCES) {
            priority[src] = (data > MAX_PRIORITY) ? MAX_PRIORITY : data;
        }
        return;
    }

    /* pending is read-only */

    /* enable */
    if (addr >= 0x2000 && addr < 0x2000 + 0x80 * NUM_CONTEXTS) {
        const uint32_t off = addr - 0x2000;
        const uint32_t ctx = off / 0x80;
        const uint32_t w = (off % 0x80) / 4;
        if (ctx < NUM_CONTEXTS && w < NUM_SOURCES / 32) enable[ctx][w] = data;
        return;
    }

    /* threshold / claim / complete */
    if (addr >= 0x200000 && addr < 0x200000 + 0x1000 * NUM_CONTEXTS) {
        const uint32_t off = addr - 0x200000;
        const uint32_t ctx = off / 0x1000;
        const uint32_t reg = off % 0x1000;
        if (ctx >= NUM_CONTEXTS) return;

        if (reg == 0x0) {
            threshold[ctx] = (data > MAX_PRIORITY) ? MAX_PRIORITY : data;
        } else if (reg == 0x4) {
            /* Complete: the gateway may raise this source again */
            if (data != 0 && data < NUM_SOURCES && data == claimed[ctx]) {
                claimed[ctx] = 0;
            }
        }
        return;
    }
}
