//
// Created by Chengbo on 2026/6/26.
//

#include <stdexcept>
#include "bus.h"
#include "utils.h"

/* Construct and initialize DRAM. */
Dram::Dram(const uint32_t base_addr, const uint32_t size) {
    this->base_addr = base_addr;
    this->size = size;
    mem.resize(size);
}

/* Load ELF to memory. */
void Dram::load_elf(const std::string &path) {
    load_elf_to_mem(path, mem, base_addr);
}

/* Read N bytes from DRAM. */
uint32_t Dram::read(const uint32_t addr, const uint32_t n_bytes) {
    uint32_t data = 0;
    for (int i = 0; i < n_bytes; i++) {
        data |= mem[addr + i] << (i * 8);
    }
    return data;
}

/* Write N bytes to DRAM. */
void Dram::write(const uint32_t addr, const uint32_t n_bytes, const uint32_t data) {
    for (int i = 0; i < n_bytes; i++) {
        mem[addr + i] = data >> (i * 8) & 0xff;
    }
}

/* Map a MMIO device to BUS. */
void Bus::map(const uint32_t base_addr, const uint32_t size, const std::shared_ptr<Mmio> &dev) {
    if (!dev) {
        return;
    }
    devices.push_back({base_addr, size, dev});
}

/* Read N bytes via the BUS. */
uint32_t Bus::read(const uint32_t addr, const uint32_t n_bytes) const {
    for (const auto& [m_base, m_size, m_dev] : devices) {
        if (addr >= m_base && addr < m_base + m_size) {
            const uint32_t offset = addr - m_base;
            return m_dev->read(offset, n_bytes);
        }
    }
    throw std::runtime_error("BUS: invalid address!");
}

/* Write N bytes via the BUS. */
void Bus::write(const uint32_t addr, const uint32_t n_bytes, const uint32_t data) const {
    for (const auto& [m_base, m_size, m_dev] : devices) {
        if (addr >= m_base && addr < m_base + m_size) {
            const uint32_t offset = addr - m_base;
            m_dev->write(offset, n_bytes, data);
        }
    }
}
