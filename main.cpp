#include <iostream>
#include <string>
#include <cstring>
#include "utils.h"
#include "bus.h"
#include "cpu.h"

//============================================================================
// sim — 运行单个 ELF，可选 trace 与 ISA 判定
//   返回 0 = PASS，1 = FAIL，2 = 超时/异常
//============================================================================
static int sim(const std::string& path,
               const bool trace,
               const bool isa_test,
               const uint64_t max_cycles,
               const bool quiet) {

    // 载入程序到 DRAM
    const auto dram = std::make_shared<Dram>(0x80000000, 1024 * 1024);
    dram->load_elf(path);
    const uint32_t host_addr = get_tohost_addr(path);

    Bus bus;
    bus.map(0x80000000, 1024 * 1024, dram);

    Cpu cpu;
    cpu.connect(&bus);
    if (trace) cpu.trace_on();

    uint64_t cycles = 0;
    while (true) {
        cpu.run();
        cycles++;

        if (isa_test) {
            const uint32_t to_lo = bus.read(host_addr, 4);
            const uint32_t to_hi = bus.read(host_addr + 4, 4);
            // 与 V2 一致：低位==1 且高位==0 才算 PASS
            if (to_lo == 1 && to_hi == 0) {
                printf("PASS\n");
                return 0;
            }
            if (to_lo != 0 || to_hi != 0) {
                printf("FAIL testnum=%u hi=%u\n", to_lo >> 1, to_hi);
                return 1;
            }
        }

        if (max_cycles && cycles >= max_cycles) {
            printf("TIMEOUT cycles=%llu\n", (unsigned long long)cycles);
            return 2;
        }
    }
}

//============================================================================
// main — 用法：
//   v3sim <elf>                 运行（ISA 判定）
//   v3sim <elf> --trace         带 trace
//   v3sim <elf> --no-isa        只跑，不判定（用于 trace 对拍）
//   v3sim <elf> --max N         周期上限（默认 0 = 不限）
//   v3sim <elf> --quiet         只输出 PASS/FAIL/TIMEOUT
//============================================================================
int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr,
            "用法: %s <elf> [--trace] [--no-isa] [--max N] [--quiet]\n", argv[0]);
        return 3;
    }

    std::string path = argv[1];
    bool trace    = false;
    bool isa_test = true;
    bool quiet    = false;
    uint64_t max_cycles = 0;

    for (int i = 2; i < argc; i++) {
        if (!strcmp(argv[i], "--trace"))       trace = true;
        else if (!strcmp(argv[i], "--no-isa")) isa_test = false;
        else if (!strcmp(argv[i], "--quiet"))  quiet = true;
        else if (!strcmp(argv[i], "--max") && i + 1 < argc)
            max_cycles = strtoull(argv[++i], nullptr, 0);
        else {
            fprintf(stderr, "未知参数: %s\n", argv[i]);
            return 3;
        }
    }

    try {
        return sim(path, trace, isa_test, max_cycles, quiet);
    } catch (const std::exception& e) {
        fprintf(stderr, "ERROR: %s\n", e.what());
        return 2;
    }
}
