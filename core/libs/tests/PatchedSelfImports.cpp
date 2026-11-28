#include <elf.h>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

namespace Fs = std::filesystem;

class ElfFile {
public:
    explicit ElfFile(const Fs::path& path) : stream(path, std::ios::binary | std::ios::ate) {
        if (!stream) throw std::runtime_error("cannot open " + path.string());
        const auto end = stream.tellg();
        if (end < 0) throw std::runtime_error("cannot open " + path.string());
        size = static_cast<std::uint64_t>(end);
    }

    std::vector<char> Read(std::uint64_t offset, std::uint64_t length) {
        if (offset > size || size - offset < length) throw std::runtime_error("truncated ELF file");
        std::vector<char> bytes(static_cast<std::size_t>(length));
        stream.clear();
        stream.seekg(static_cast<std::streamoff>(offset));
        if (!stream) throw std::runtime_error("truncated ELF file");
        if (length != 0 && !stream.read(bytes.data(), static_cast<std::streamsize>(length)))
            throw std::runtime_error("truncated ELF file");
        return bytes;
    }

private:
    std::ifstream stream;
    std::uint64_t size = 0;
};

template <typename TValue>
TValue readAt(const std::vector<char>& bytes, std::size_t offset) {
    if (offset > bytes.size() || bytes.size() - offset < sizeof(TValue)) throw std::runtime_error("truncated ELF file");
    TValue value;
    std::memcpy(&value, bytes.data() + offset, sizeof(TValue));
    return value;
}

std::string readName(const std::vector<char>& strings, std::uint32_t offset) {
    if (offset >= strings.size()) throw std::runtime_error("symbol name outside its string table");
    const char* first = strings.data() + offset;
    return {first, strnlen(first, strings.size() - offset)};
}

std::vector<std::string> selfImports(const Fs::path& path) {
    ElfFile file(path);
    const auto headerBytes = file.Read(0, sizeof(Elf64_Ehdr));
    const auto header = readAt<Elf64_Ehdr>(headerBytes, 0);
    if (std::memcmp(header.e_ident, ELFMAG, SELFMAG) != 0 || header.e_ident[EI_CLASS] != ELFCLASS64)
        throw std::runtime_error(path.string() + " is not a 64-bit ELF file");
    const auto sections = file.Read(header.e_shoff, std::uint64_t{header.e_shnum} * header.e_shentsize);
    std::set<std::string> defined;
    std::set<std::string> imported;
    for (std::size_t index = 0; index < header.e_shnum; ++index) {
        const auto section = readAt<Elf64_Shdr>(sections, index * header.e_shentsize);
        if (section.sh_type != SHT_DYNSYM) continue;
        const auto stringsHeader = readAt<Elf64_Shdr>(sections, std::size_t{section.sh_link} * header.e_shentsize);
        const auto symbols = file.Read(section.sh_offset, section.sh_size);
        const auto strings = file.Read(stringsHeader.sh_offset, stringsHeader.sh_size);
        for (std::size_t offset = sizeof(Elf64_Sym); offset + sizeof(Elf64_Sym) <= symbols.size(); offset += sizeof(Elf64_Sym)) {
            const auto symbol = readAt<Elf64_Sym>(symbols, offset);
            const auto name = readName(strings, symbol.st_name);
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
