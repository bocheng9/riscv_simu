//
// Created by Chengbo on 2026/6/26.
//

#ifndef RVSIMU_V3_BUS_H
#define RVSIMU_V3_BUS_H

#include <memory>
#include <vector>

/* MMIO device base class. */
class Mmio {
public:
    virtual ~Mmio() = default;

    /* Read N bytes from MMIO device. */
    virtual uint32_t read(uint32_t addr, uint32_t n_bytes) = 0;

    /* Write N bytes to MMIO device. */
    virtual void write(uint32_t addr, uint32_t n_bytes, uint32_t data) = 0;
};

class Dram: public Mmio {
protected:
    uint32_t base_addr;
    uint32_t size;

    /* Memory. */
    std::vector<uint8_t> mem;
public:
    explicit Dram(uint32_t base_addr, uint32_t size);

    /* Load ELF to memory. */
    void load_elf(const std::string& path);

    /* Read N bytes from DRAM. */
    uint32_t read(uint32_t addr, uint32_t n_bytes) override;

    /* Write N bytes to DRAM. */
    void write(uint32_t addr, uint32_t n_bytes, uint32_t data) override;
};

class Bus {
protected:
    /* MMIO devices. */
    struct DeviceMapping {
        uint32_t base_addr;
        uint32_t size;
        std::shared_ptr<Mmio> device;
    };

    std::vector<DeviceMapping> devices;
public:
    /* Map a MMIO device to BUS. */
    void map(uint32_t base_addr, uint32_t size, const std::shared_ptr<Mmio> &dev);

    /* Read memory. */
    uint32_t read(uint32_t addr, uint32_t n_bytes) const;

    /* Write memory. */
    void write(uint32_t addr, uint32_t n_bytes, uint32_t data) const;
};


#endif //RVSIMU_V3_BUS_H
