#include <elf.h>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

namespace Fs = std::filesystem;

std::vector<char> readFile(const Fs::path& path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) throw std::runtime_error("cannot open " + path.string());
    return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}

template <typename TValue>
TValue readAt(const std::vector<char>& bytes, std::size_t offset) {
    if (offset > bytes.size() || bytes.size() - offset < sizeof(TValue)) throw std::runtime_error("truncated ELF file");
    TValue value;
    std::memcpy(&value, bytes.data() + offset, sizeof(TValue));
    return value;
}

std::string readName(const std::vector<char>& bytes, const Elf64_Shdr& strings, std::uint32_t offset) {
    if (offset >= strings.sh_size || strings.sh_offset > bytes.size() || bytes.size() - strings.sh_offset < strings.sh_size)
        throw std::runtime_error("symbol name outside its string table");
    const char* first = bytes.data() + strings.sh_offset + offset;
    return {first, strnlen(first, strings.sh_size - offset)};
}

std::vector<std::string> selfImports(const Fs::path& path) {
    const auto bytes = readFile(path);
    const auto header = readAt<Elf64_Ehdr>(bytes, 0);
    if (std::memcmp(header.e_ident, ELFMAG, SELFMAG) != 0 || header.e_ident[EI_CLASS] != ELFCLASS64)
        throw std::runtime_error(path.string() + " is not a 64-bit ELF file");
    std::set<std::string> defined;
    std::set<std::string> imported;
    for (std::size_t index = 0; index < header.e_shnum; ++index) {
        const auto section = readAt<Elf64_Shdr>(bytes, header.e_shoff + index * header.e_shentsize);
        if (section.sh_type != SHT_DYNSYM) continue;
        const auto strings = readAt<Elf64_Shdr>(bytes, header.e_shoff + section.sh_link * header.e_shentsize);
        for (std::size_t offset = sizeof(Elf64_Sym); offset + sizeof(Elf64_Sym) <= section.sh_size; offset += sizeof(Elf64_Sym)) {
            const auto symbol = readAt<Elf64_Sym>(bytes, section.sh_offset + offset);
            const auto name = readName(bytes, strings, symbol.st_name);
            if (!name.empty()) (symbol.st_shndx == SHN_UNDEF ? imported : defined).insert(name);
        }
    }
    std::vector<std::string> both;
    std::set_intersection(imported.begin(), imported.end(), defined.begin(), defined.end(), std::back_inserter(both));
    return both;
}

}

int main(int argc, char** argv) {
    try {
        if (argc != 2) throw std::runtime_error("usage: patched_self_imports_tests <patched library directory>");
        std::size_t libraries = 0;
        std::vector<std::string> failures;
        for (const auto& entry : Fs::directory_iterator(argv[1])) {
            if (!entry.is_regular_file() || entry.path().extension() != ".prx") continue;
            ++libraries;
            for (const auto& name : selfImports(entry.path()))
                failures.push_back(entry.path().filename().string() + " imports " + name + ", a name it also exports, so the import can bind to its own export");
        }
        if (libraries == 0) throw std::runtime_error(std::string("no .prx libraries in ") + argv[1]);
        for (const auto& failure : failures) std::cerr << failure << "\n";
        if (!failures.empty()) return 1;
        std::cout << "No patched library imports a name it exports (" << libraries << " libraries)\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << "\n";
        return 1;
    }
}
