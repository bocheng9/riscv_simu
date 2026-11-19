//
// dev.cpp — UART 与 CLINT 实现
//

#include <cstdio>
#include <cstdlib>
#include <chrono>
#include "dev.h"

/*============================================================================
 * Uart
 *============================================================================*/
uint32_t Uart::read(const uint32_t addr, const uint32_t /*n_bytes*/) {
    if (addr == 5) {
        return 0x20;    // LSR: THR empty
    }
    return 0;
}

void Uart::write(const uint32_t addr, const uint32_t /*n_bytes*/, const uint32_t data) {
    if (addr == 0) {
        fputc(static_cast<int>(data & 0xff), stdout);
        fflush(stdout);
    }
}

/*============================================================================
 * Clint
 *============================================================================*/
Clint::Clint() {
    host_start_ns = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
}

uint64_t Clint::mtime() const {
    const uint64_t now = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
    const uint64_t ticks = (now - host_start_ns) / 100;   // 10MHz
    return mtime_base + ticks;
}

uint32_t Clint::read(const uint32_t addr, const uint32_t /*n_bytes*/) {
    if (addr == 0xbff8) {
        return static_cast<uint32_t>(mtime() & 0xffffffff);
    }
    if (addr == 0xbffc) {
        return static_cast<uint32_t>((mtime() >> 32) & 0xffffffff);
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
    const auto now_ns = [] {
        return static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count());
    };

    if (addr == 0xbff8) {
        mtime_base = (mtime_base & 0xffffffff00000000ull) | data;
        host_start_ns = now_ns();
    } else if (addr == 0xbffc) {
        mtime_base = (static_cast<uint64_t>(data) << 32) | (mtime_base & 0xffffffff);
        host_start_ns = now_ns();
    } else if (addr == 0x4000) {
        mtimecmp = (mtimecmp & 0xffffffff00000000ull) | data;
    } else if (addr == 0x4004) {
        mtimecmp = (static_cast<uint64_t>(data) << 32) | (mtimecmp & 0xffffffff);
    }
}
