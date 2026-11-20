//
// plic.h — Platform-Level Interrupt Controller (SiFive PLIC 1.0.0)
//
// Implements the register layout the Linux RISC-V PLIC driver expects:
//   priority  : 0x000000 + 4 * source        (source 1..1023)
//   pending   : 0x001000 + 4 * (source / 32)
//   enable    : 0x002000 + 0x80 * context + 4 * (source / 32)
//   threshold : 0x200000 + 0x1000 * context
//   claim     : 0x200004 + 0x1000 * context   (read = claim, write = complete)
//
// Context numbering follows the QEMU virt platform: context 0 is M-mode and
// context 1 is S-mode of hart 0, so the S-mode external interrupt (SEIP) is
// raised from context 1.
//

#ifndef RVSIMU_V3_PLIC_H
#define RVSIMU_V3_PLIC_H

#include <cstdint>
#include "bus.h"

class Plic: public Mmio {
public:
    static constexpr uint32_t NUM_SOURCES = 1024;
    static constexpr uint32_t NUM_CONTEXTS = 2;
    static constexpr uint32_t MAX_PRIORITY = 7;

private:
    uint32_t priority[NUM_SOURCES] {};
    uint32_t pending[NUM_SOURCES / 32] {};
    uint32_t enable[NUM_CONTEXTS][NUM_SOURCES / 32] {};
    uint32_t threshold[NUM_CONTEXTS] {};
    uint32_t claimed[NUM_CONTEXTS] {};

    /* Lowest-numbered pending source that is enabled and above the context
     * threshold. Returns 0 when there is nothing to deliver. */
    [[nodiscard]] uint32_t best_source(uint32_t context) const;

public:
    /* Raise or clear an interrupt request for a source */
    void set_irq(uint32_t source, bool level);

    /* Whether this context currently has an interrupt to deliver */
    [[nodiscard]] bool pending_for(uint32_t context) const {
        return best_source(context) != 0;
    }

    uint32_t read(uint32_t addr, uint32_t n_bytes) override;
    void     write(uint32_t addr, uint32_t n_bytes, uint32_t data) override;
};

#endif //RVSIMU_V3_PLIC_H
