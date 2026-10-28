#include "OracleCodeObject.hpp"
#include <cstring>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

constexpr std::size_t ElfHeaderBytes = 64;
constexpr std::size_t SectionHeaderBytes = 64;
constexpr std::size_t SymbolBytes = 24;
constexpr std::size_t DescriptorBytes = 64;
constexpr std::uint16_t MachineAmdGpu = 224;
constexpr std::uint16_t TypeDyn = 3;
constexpr std::uint8_t SymbolObject = 1;
constexpr std::uint8_t SymbolFunc = 2;
constexpr std::uint32_t MaxGroupBytes = 65536;

struct Section {
    std::string name;
    std::uint64_t address;
    std::uint64_t offset;
    std::uint64_t size;
    std::uint32_t link;
};

struct Symbol {
    std::string name;
    std::uint8_t type;
    std::uint16_t section;
    std::uint64_t value;
    std::uint64_t size;
};

void Require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

class Reader {
public:
    explicit Reader(std::span<const std::byte> bytes) : bytes(bytes) {}

    template <typename T>
    T Read(std::uint64_t offset, const char* what) const {
        Check(offset, sizeof(T), what);
        T value;
        std::memcpy(&value, bytes.data() + offset, sizeof(T));
        return value;
    }

    std::span<const std::byte> Range(std::uint64_t offset, std::uint64_t size, const char* what) const {
        Check(offset, size, what);
        return bytes.subspan(static_cast<std::size_t>(offset), static_cast<std::size_t>(size));
    }

private:
    void Check(std::uint64_t offset, std::uint64_t size, const char* what) const {
        Require(offset <= bytes.size() && size <= bytes.size() - offset, std::string("code object is truncated: ") + what + " lies outside the file");
    }

    std::span<const std::byte> bytes;
};

std::string NameAt(std::span<const std::byte> table, std::uint64_t offset, const char* what) {
    Require(offset < table.size(), std::string("code object ") + what + " name offset is outside its string table");
    const auto begin = reinterpret_cast<const char*>(table.data()) + offset;
    const auto limit = static_cast<std::size_t>(table.size() - offset);
    std::size_t length = 0;
    while (length < limit && begin[length] != '\0') ++length;
    Require(length < limit, std::string("code object ") + what + " name is not terminated");
    return std::string(begin, length);
}

const Symbol& FindSymbol(const std::vector<Symbol>& symbols, std::string_view name) {
    for (const auto& symbol : symbols) {
        if (symbol.name == name) return symbol;
    }
    throw std::runtime_error("code object has no symbol " + std::string(name));
}

const Section& SectionOf(const std::vector<Section>& sections, const Symbol& symbol, std::string_view expected) {
    Require(symbol.section != 0 && symbol.section < sections.size(), "symbol " + symbol.name + " has an invalid section index");
    const auto& section = sections[symbol.section];
    Require(section.name == expected, "symbol " + symbol.name + " is in section " + section.name + ", not " + std::string(expected));
    Require(symbol.value >= section.address && symbol.value - section.address <= section.size && symbol.size <= section.size - (symbol.value - section.address),
            "symbol " + symbol.name + " lies outside section " + section.name);
    return section;
}

}

OracleKernel ParseOracleCodeObject(std::span<const std::byte> bytes) {
    const Reader reader(bytes);
    Require(bytes.size() >= ElfHeaderBytes, "code object is truncated: ELF header lies outside the file");
    Require(std::memcmp(bytes.data(), "\x7f" "ELF", 4) == 0, "code object ELF magic is wrong");
    Require(reader.Read<std::uint8_t>(4, "EI_CLASS") == 2, "code object EI_CLASS is not ELF64");
    Require(reader.Read<std::uint8_t>(5, "EI_DATA") == 1, "code object EI_DATA is not little-endian");
    Require(reader.Read<std::uint16_t>(16, "e_type") == TypeDyn, "code object e_type is not ET_DYN");
    Require(reader.Read<std::uint16_t>(18, "e_machine") == MachineAmdGpu, "code object e_machine is not EM_AMDGPU (224)");
    const auto sectionOffset = reader.Read<std::uint64_t>(40, "e_shoff");
    const auto sectionEntrySize = reader.Read<std::uint16_t>(58, "e_shentsize");
    const auto sectionCount = reader.Read<std::uint16_t>(60, "e_shnum");
    const auto nameSection = reader.Read<std::uint16_t>(62, "e_shstrndx");
    Require(sectionEntrySize == SectionHeaderBytes, "code object e_shentsize is not 64");
    Require(sectionCount != 0 && nameSection < sectionCount, "code object e_shnum or e_shstrndx is invalid");

    struct RawSection {
        std::uint32_t name;
        std::uint64_t address;
        std::uint64_t offset;
        std::uint64_t size;
        std::uint32_t link;
    };
    std::vector<RawSection> raw;
    for (std::uint16_t index = 0; index < sectionCount; ++index) {
        const auto base = sectionOffset + static_cast<std::uint64_t>(index) * SectionHeaderBytes;
        raw.push_back({reader.Read<std::uint32_t>(base, "section header"), reader.Read<std::uint64_t>(base + 16, "section header"), reader.Read<std::uint64_t>(base + 24, "section header"),
                       reader.Read<std::uint64_t>(base + 32, "section header"), reader.Read<std::uint32_t>(base + 40, "section header")});
    }
    const auto sectionNames = reader.Range(raw[nameSection].offset, raw[nameSection].size, ".shstrtab");
    std::vector<Section> sections;
    for (const auto& item : raw) {
        sections.push_back({NameAt(sectionNames, item.name, "section"), item.address, item.offset, item.size, item.link});
    }

    std::size_t symtab = sections.size();
    for (std::size_t index = 0; index < sections.size(); ++index) {
        if (sections[index].name == ".symtab") symtab = index;
    }
    Require(symtab != sections.size(), "code object has no .symtab section");
    const auto& symbolSection = sections[symtab];
    Require(symbolSection.link < sections.size() && sections[symbolSection.link].name == ".strtab", "code object .symtab does not link to .strtab");
    Require(symbolSection.size % SymbolBytes == 0, "code object .symtab size is not a multiple of 24");
    const auto strings = reader.Range(sections[symbolSection.link].offset, sections[symbolSection.link].size, ".strtab");
    std::vector<Symbol> symbols;
    for (std::uint64_t at = 0; at < symbolSection.size; at += SymbolBytes) {
        const auto base = symbolSection.offset + at;
        const auto nameOffset = reader.Read<std::uint32_t>(base, "symbol");
        const auto info = reader.Read<std::uint8_t>(base + 4, "symbol");
        symbols.push_back({NameAt(strings, nameOffset, "symbol"), static_cast<std::uint8_t>(info & 0xfu), reader.Read<std::uint16_t>(base + 6, "symbol"),
                           reader.Read<std::uint64_t>(base + 8, "symbol"), reader.Read<std::uint64_t>(base + 16, "symbol")});
    }

    const auto& probe = FindSymbol(symbols, "probe");
    const auto& descriptor = FindSymbol(symbols, "probe.kd");
    Require(probe.type == SymbolFunc, "symbol probe is not STT_FUNC");
    Require(descriptor.type == SymbolObject, "symbol probe.kd is not STT_OBJECT");
    Require(probe.size != 0, "symbol probe has size 0");
    Require(probe.size % 4 == 0, "symbol probe size is not a multiple of 4");
    Require(descriptor.size == DescriptorBytes, "symbol probe.kd size is not 64");
    const auto& textSection = SectionOf(sections, probe, ".text");
    const auto& rodataSection = SectionOf(sections, descriptor, ".rodata");
    const auto textBytes = reader.Range(textSection.offset + (probe.value - textSection.address), probe.size, "probe");
    const auto descriptorBytes = reader.Range(rodataSection.offset + (descriptor.value - rodataSection.address), descriptor.size, "probe.kd");

    const auto field = [&](std::size_t offset, std::size_t size) {
        std::uint64_t value = 0;
        std::memcpy(&value, descriptorBytes.data() + offset, size);
        return value;
    };
    const auto group = static_cast<std::uint32_t>(field(0, 4));
    const auto privateBytes = static_cast<std::uint32_t>(field(4, 4));
    const auto kernargBytes = static_cast<std::uint32_t>(field(8, 4));
    const auto entry = static_cast<std::int64_t>(field(16, 8));
    const auto rsrc1 = static_cast<std::uint32_t>(field(48, 4));
    const auto rsrc2 = static_cast<std::uint32_t>(field(52, 4));
    const auto properties = static_cast<std::uint32_t>(field(56, 2));
    Require(group <= MaxGroupBytes, "kernel descriptor group_segment_fixed_size exceeds 65536");
    Require(privateBytes == 0, "kernel descriptor private_segment_fixed_size is not 0");
    Require(kernargBytes == 16, "kernel descriptor kernarg_size is not 16");
    Require(entry == static_cast<std::int64_t>(probe.value) - static_cast<std::int64_t>(descriptor.value), "kernel descriptor kernel_code_entry_byte_offset does not point at probe");
    Require((rsrc2 & 1u) == 0, "kernel descriptor compute_pgm_rsrc2 ENABLE_PRIVATE_SEGMENT is set");
    Require(((rsrc2 >> 1) & 0x1fu) == 2, "kernel descriptor compute_pgm_rsrc2 USER_SGPR_COUNT is not 2");
    Require((properties & 0x8u) != 0, "kernel descriptor kernel_code_properties ENABLE_SGPR_KERNARG_SEGMENT_PTR is not set");
    Require((properties & 0x77u) == 0, "kernel descriptor kernel_code_properties enables a user SGPR other than the kernarg pointer");

    OracleKernel kernel{};
    kernel.text.resize(textBytes.size() / 4);
    std::memcpy(kernel.text.data(), textBytes.data(), textBytes.size());
    kernel.ldsBytes = group;
    kernel.waveSize = (properties & 0x400u) != 0 ? 32u : 64u;
    kernel.floatMode = {(rsrc1 >> 12) & 0xffu, ((rsrc1 >> 21) & 1u) != 0, ((rsrc1 >> 23) & 1u) != 0, ((rsrc1 >> 26) & 1u) != 0};
    kernel.groupIdEnable = {((rsrc2 >> 7) & 1u) != 0, ((rsrc2 >> 8) & 1u) != 0, ((rsrc2 >> 9) & 1u) != 0};
    kernel.tgSizeEnable = ((rsrc2 >> 10) & 1u) != 0;
    kernel.threadIdComponentCount = ((rsrc2 >> 11) & 3u) + 1u;
    return kernel;
}
