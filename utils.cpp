//
// Created by Chengbo on 2026/6/24.
//

#include "utils.h"
#include <fstream>
#include <vector>

void print_elf_info(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        throw std::runtime_error("ELF: could not open file!");
    }

    // Load ELF header.
    Elf32_Ehdr ehdr;
    file.read(reinterpret_cast<char*>(&ehdr), sizeof(Elf32_Ehdr));

    // Check ELF magic number.
    if (ehdr.e_ident[0] != 0x7f || ehdr.e_ident[1] != 'E' ||
        ehdr.e_ident[2] != 'L' || ehdr.e_ident[3] != 'F') {
        throw std::runtime_error("ELF: not an ELF file!");
    }

    // Print ELF header.
    printf("======================= ELF header =======================\n");
    printf("magic: ");
    for (const unsigned char i : ehdr.e_ident) {
        printf("%02x ", i);
    }
    printf("\n");
    printf("type: %d, machine: %d, version: %d, entry: 0x%x, flags: 0x%x\n",
        ehdr.e_type, ehdr.e_machine, ehdr.e_version, ehdr.e_entry, ehdr.e_flags);

    // Print program segments.
    printf("\n==================== Program segments ====================\n");
    for (int i = 0; i < ehdr.e_phnum; i++) {
        Elf32_Phdr phdr;
        file.seekg(ehdr.e_phoff + i * ehdr.e_phentsize, std::ios::beg);
        file.read(reinterpret_cast<char*>(&phdr), sizeof(Elf32_Phdr));
        printf("[%d] type: 0x%-9x, offset: 0x%-6x, vaddr: 0x%-9x, paddr: 0x%-8x, filesz: %-8d, memsz: %d\n",
            i, phdr.p_type, phdr.p_offset, phdr.p_vaddr, phdr.p_paddr, phdr.p_filesz, phdr.p_memsz);
    }

    // Print sections.
    std::vector<Elf32_Shdr> shdrs(ehdr.e_shnum);
    file.seekg(ehdr.e_shoff);
    file.read(reinterpret_cast<char*>(shdrs.data()), ehdr.e_shnum * ehdr.e_shentsize);

    std::vector<char> strtab(shdrs[ehdr.e_shstrndx].sh_size);
    file.seekg(shdrs[ehdr.e_shstrndx].sh_offset);
    file.read(strtab.data(), strtab.size());

    printf("\n======================== Sections ========================\n");
    for (int i=0; i<ehdr.e_shnum; i++) {
        std::string name = &strtab[shdrs[i].sh_name];
        if (name.empty()) name = "(none)";
        printf("[%2d] %-20s, type: 0x%-9x, flags: 0x%-3x, addr: 0x%-9x, offset: 0x%-8x, size: %d\n",
            i, name.c_str(), shdrs[i].sh_type, shdrs[i].sh_flags, shdrs[i].sh_addr, shdrs[i].sh_offset, shdrs[i].sh_size);
    }

}
