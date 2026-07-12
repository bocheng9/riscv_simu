#include <iostream>
#include "utils.h"
#include "bus.h"
#include "cpu.h"

void sim(const std::string& path, const bool trace=false, const bool isa_test=false) {

    // Load program to RAM.
    const auto dram = std::make_shared<Dram>(0x80000000, 1024 * 1024);
    dram->load_elf(path);
    const uint32_t host_addr = get_tohost_addr(path);

    // Init BUS.
    Bus bus;
    bus.map(0x80000000, 1024 * 1024, dram);

    // Unit CPU.
    Cpu cpu;
    cpu.connect(&bus);
    if (trace) cpu.trace_on();

    // Run emulator.
    while (true) {
        cpu.run();

        if (isa_test) {
            const uint32_t to_host = bus.read(host_addr, 4);
            if (to_host == 1) {
                printf("\033[32m PASS\n \033[0m");
                break;
            }
            if (to_host != 0) {
                printf("\033[31m FAIL\n \033[0m");
                break;
            }
        }
    }
}

#include <filesystem>
namespace fs = std::filesystem;
int main() {
    // sim(R"(./isa_test/rv32ua-p-lrsc)");
    for (const auto& entry : fs::directory_iterator(R"(./isa_test/)")) {
        std::cout << entry.path().filename() << "\n";
        sim(entry.path().string(), false, true);
    }
    return 0;
}
