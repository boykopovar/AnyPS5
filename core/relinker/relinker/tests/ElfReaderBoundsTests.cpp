#include <relinker/parsing/ElfReader.hpp>
#include <cstdint>
#include <cstring>
#include <functional>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace {

using Bytes = std::vector<std::uint8_t>;

template<typename TValue>
void Write(Bytes& bytes, const std::size_t offset, const TValue value) {
    if (offset > bytes.size() || sizeof(value) > bytes.size() - offset) throw std::runtime_error("Invalid fixture offset");
    std::memcpy(bytes.data() + offset, &value, sizeof(value));
}

Bytes Fixture() {
    Bytes bytes(0x300);
    const std::uint8_t magic[] = {0x7f, 'E', 'L', 'F', 2, 1, 1};
    std::memcpy(bytes.data(), magic, sizeof(magic));
    Write<std::uint16_t>(bytes, 16, 3);
    Write<std::uint16_t>(bytes, 18, 62);
    Write<std::uint32_t>(bytes, 20, 1);
    Write<std::uint64_t>(bytes, 32, 64);
    Write<std::uint64_t>(bytes, 40, 128);
    Write<std::uint16_t>(bytes, 52, 64);
    Write<std::uint16_t>(bytes, 54, 56);
    Write<std::uint16_t>(bytes, 56, 1);
    Write<std::uint16_t>(bytes, 58, 64);
    Write<std::uint16_t>(bytes, 60, 3);
    Write<std::uint16_t>(bytes, 62, 2);
    Write<std::uint32_t>(bytes, 64, 1);
    Write<std::uint64_t>(bytes, 72, 0x240);
    Write<std::uint64_t>(bytes, 80, 0x1000);
    Write<std::uint64_t>(bytes, 96, 32);
    Write<std::uint64_t>(bytes, 104, 64);
    Write<std::uint32_t>(bytes, 192, 1);
    Write<std::uint32_t>(bytes, 196, 1);
    Write<std::uint64_t>(bytes, 216, 0x240);
    Write<std::uint64_t>(bytes, 224, 32);
    Write<std::uint64_t>(bytes, 240, 8);
    Write<std::uint64_t>(bytes, 248, 24);
    Write<std::uint32_t>(bytes, 256, 7);
    Write<std::uint32_t>(bytes, 260, 3);
    Write<std::uint64_t>(bytes, 280, 0x200);
    const char strings[] = "\0.text\0.shstrtab";
    Write<std::uint64_t>(bytes, 288, sizeof(strings));
    std::memcpy(bytes.data() + 0x200, strings, sizeof(strings));
    return bytes;
}

Bytes TlsFixture(const std::uint64_t address, const std::uint64_t fileSize, const std::uint64_t memorySize, const std::uint64_t alignment) {
    auto bytes = Fixture();
    std::memcpy(bytes.data() + 0x260, bytes.data() + 64, 56);
    Write<std::uint64_t>(bytes, 32, 0x260);
    Write<std::uint16_t>(bytes, 56, 2);
    Write<std::uint32_t>(bytes, 0x298, 7);
    Write<std::uint32_t>(bytes, 0x29c, 4);
    Write<std::uint64_t>(bytes, 0x2a0, 0x248);
    Write(bytes, 0x2a8, address);
    Write(bytes, 0x2b0, address);
    Write(bytes, 0x2b8, fileSize);
    Write(bytes, 0x2c0, memorySize);
    Write(bytes, 0x2c8, alignment);
    return bytes;
}

void Require(const bool condition) {
    if (!condition) throw std::runtime_error("Unexpected parsed value");
}

void RejectSections(Bytes bytes) {
    try {
        Relinker::ElfReader(std::move(bytes)).ReadSectionHeaders();
    } catch (const Relinker::RelinkerException&) {
        return;
    }
    throw std::runtime_error("Invalid section metadata was accepted");
}

void RejectAddress(Bytes bytes, const std::uint64_t address) {
    try {
        Relinker::ElfReader(std::move(bytes)).TranslateVirtualAddress(address);
    } catch (const Relinker::RelinkerException&) {
        return;
    }
    throw std::runtime_error("Invalid file mapping was accepted");
}

void RejectProgramHeaders(Bytes bytes) {
    try {
        Relinker::ElfReader(std::move(bytes)).ReadProgramHeaders();
    } catch (const Relinker::RelinkerException&) {
        return;
    }
    throw std::runtime_error("Invalid program header was accepted");
}

}

int main() {
    const std::vector<std::pair<std::string, std::function<void()>>> cases = {
        {"load file range past end of file", [] {
            auto bytes = Fixture();
            Write<std::uint64_t>(bytes, 96, 0xd0);
            Write<std::uint64_t>(bytes, 104, 0xd0);
            RejectProgramHeaders(std::move(bytes));
        }},
        {"load and uncovered SCE RELRO ending at end of file", [] {
            for (const std::uint32_t type : {1u, 0x61000010u}) {
                auto bytes = Fixture();
                Write<std::uint32_t>(bytes, 64, type);
                Write<std::uint64_t>(bytes, 96, 0xc0);
                Write<std::uint64_t>(bytes, 104, 0xc0);
                const auto headers = Relinker::ElfReader(std::move(bytes)).ReadProgramHeaders();
                Require(headers.size() == 1 && headers[0].Offset + headers[0].FileSize == 0x300);
            }
        }},
        {"uncovered SCE RELRO file size exceeds memory size", [] {
            auto bytes = Fixture();
            Write<std::uint32_t>(bytes, 64, 0x61000010);
            Write<std::uint64_t>(bytes, 96, 64);
            Write<std::uint64_t>(bytes, 104, 32);
            RejectProgramHeaders(std::move(bytes));
        }},
        {"uncovered SCE RELRO file range past end of file", [] {
            auto bytes = Fixture();
            Write<std::uint32_t>(bytes, 64, 0x61000010);
            Write<std::uint64_t>(bytes, 96, 0xd0);
            Write<std::uint64_t>(bytes, 104, 0xd0);
            RejectProgramHeaders(std::move(bytes));
        }},
        {"TLS image inside load memory and empty TLS placeholders", [] {
            for (const auto& [address, fileSize, memorySize, alignment] : {std::tuple<std::uint64_t, std::uint64_t, std::uint64_t, std::uint64_t>{0x1008, 8, 16, 8}, {0x1030, 16, 16, 16}, {0x9000, 0, 0, 0}, {0x9000, 0, 16, 16}}) {
                const auto headers = Relinker::ElfReader(TlsFixture(address, fileSize, memorySize, alignment)).ReadProgramHeaders();
                Require(headers.size() == 2 && headers[1].Type == 7 && headers[1].MappedAddress == address);
            }
        }},
        {"TLS image inside uncovered SCE RELRO", [] {
            auto bytes = TlsFixture(0x1008, 8, 16, 8);
            Write<std::uint32_t>(bytes, 0x260, 0x61000010);
            const auto headers = Relinker::ElfReader(std::move(bytes)).ReadProgramHeaders();
            Require(headers.size() == 2 && headers[0].Type == 1 && headers[1].Type == 7 && headers[1].MappedAddress == 0x1008);
        }},
        {"TLS image ending at end of file", [] {
            auto bytes = TlsFixture(0x1008, 8, 16, 8);
            Write<std::uint64_t>(bytes, 0x2a0, 0x2f8);
            const auto headers = Relinker::ElfReader(std::move(bytes)).ReadProgramHeaders();
            Require(headers.size() == 2 && headers[1].Offset + headers[1].FileSize == 0x300);
        }},
        {"empty TLS image beyond end of file", [] {
            constexpr std::uint64_t offsets[] = {0x300, 0x301, std::numeric_limits<std::uint64_t>::max()};
            for (const std::uint64_t offset : offsets) {
                auto bytes = TlsFixture(0x9000, 0, 16, 16);
                Write(bytes, 0x2a0, offset);
                const auto headers = Relinker::ElfReader(std::move(bytes)).ReadProgramHeaders();
                Require(headers.size() == 2 && headers[1].Offset == offset && headers[1].FileSize == 0);
            }
        }},
        {"TLS file range past end of file", [] {
            constexpr std::uint64_t offsets[] = {0x2f9, 0x300, 0x301, std::numeric_limits<std::uint64_t>::max() - 3, std::numeric_limits<std::uint64_t>::max()};
            for (const std::uint64_t offset : offsets) {
                auto bytes = TlsFixture(0x1008, 8, 16, 1);
                Write(bytes, 0x2a0, offset);
                RejectProgramHeaders(std::move(bytes));
            }
        }},
        {"TLS image outside load memory", [] {
            constexpr std::uint64_t addresses[] = {0x0ff8, 0x103c, 0x2000, std::numeric_limits<std::uint64_t>::max() - 3};
            for (const std::uint64_t address : addresses) {
                RejectProgramHeaders(TlsFixture(address, 8, 8, 1));
            }
        }},
        {"TLS file size exceeds memory size", [] {
            RejectProgramHeaders(TlsFixture(0x1008, 16, 8, 8));
        }},
        {"TLS alignment is not a power of two", [] {
            RejectProgramHeaders(TlsFixture(0x1008, 8, 8, 3));
        }},
        {"second TLS segment", [] {
            auto bytes = TlsFixture(0x1008, 8, 16, 8);
            bytes.resize(0x308);
            std::memcpy(bytes.data() + 0x2d0, bytes.data() + 0x298, 56);
            Write<std::uint16_t>(bytes, 56, 3);
            RejectProgramHeaders(std::move(bytes));
        }},
        {"section entry size field", [] {
            const auto sections = Relinker::ElfReader(Fixture()).ReadSectionHeaders();
            Require(sections.size() == 3 && sections[1].Name == ".text" && sections[1].EntrySize == 24);
        }},
        {"section header stride", [] {
            for (const std::uint16_t stride : {0, 56, 65}) {
                auto bytes = Fixture();
                Write(bytes, 58, stride);
                RejectSections(std::move(bytes));
            }
        }},
        {"section name table index", [] {
            auto bytes = Fixture();
            Write<std::uint16_t>(bytes, 62, 3);
            RejectSections(std::move(bytes));
        }},
        {"section name table type", [] {
            auto bytes = Fixture();
            Write<std::uint32_t>(bytes, 260, 1);
            RejectSections(std::move(bytes));
        }},
        {"section name table range", [] {
            auto bytes = Fixture();
            Write<std::uint64_t>(bytes, 288, 0x200);
            RejectSections(std::move(bytes));
        }},
        {"section name offset", [] {
            auto bytes = Fixture();
            Write<std::uint32_t>(bytes, 192, 64);
            bytes[0x240] = 'X';
            RejectSections(std::move(bytes));
        }},
        {"section name terminator", [] {
            auto bytes = Fixture();
            Write<std::uint64_t>(bytes, 288, 5);
            RejectSections(std::move(bytes));
        }},
        {"section header table truncation", [] {
            auto bytes = Fixture();
            Write<std::uint16_t>(bytes, 62, 0);
            bytes.resize(128 + 3 * 64 - 1);
            RejectSections(std::move(bytes));
        }},
        {"section file range past end of file", [] {
            auto bytes = Fixture();
            Write<std::uint64_t>(bytes, 216, 0x2f0);
            RejectSections(std::move(bytes));
        }},
        {"section file range offset overflow", [] {
            auto bytes = Fixture();
            Write<std::uint64_t>(bytes, 216, std::numeric_limits<std::uint64_t>::max() - 15);
            RejectSections(std::move(bytes));
        }},
        {"section ending at end of file", [] {
            auto bytes = Fixture();
            Write<std::uint64_t>(bytes, 216, 0x2e0);
            Write<std::uint64_t>(bytes, 224, 32);
            const auto sections = Relinker::ElfReader(std::move(bytes)).ReadSectionHeaders();
            Require(sections.size() == 3 && sections[1].Offset + sections[1].SectionSize == 0x300);
        }},
        {"unbacked SHT_NOBITS section with range past end of file", [] {
            auto bytes = Fixture();
            Write<std::uint32_t>(bytes, 196, 8);
            Write<std::uint64_t>(bytes, 216, 0x2f0);
            Write<std::uint64_t>(bytes, 224, 0x1000);
            const auto sections = Relinker::ElfReader(std::move(bytes)).ReadSectionHeaders();
            Require(sections.size() == 3 && sections[1].Type == 8);
        }},
        {"empty section beyond end of file", [] {
            constexpr std::uint64_t offsets[] = {0x300, 0x301, std::numeric_limits<std::uint64_t>::max()};
            for (const std::uint64_t offset : offsets) {
                auto bytes = Fixture();
                Write<std::uint64_t>(bytes, 216, offset);
                Write<std::uint64_t>(bytes, 224, 0);
                const auto sections = Relinker::ElfReader(std::move(bytes)).ReadSectionHeaders();
                Require(sections.size() == 3 && sections[1].Offset == offset && sections[1].SectionSize == 0);
            }
        }},
        {"translation program header stride", [] {
            for (const std::uint16_t stride : {0, 48, 64}) {
                auto bytes = Fixture();
                Write(bytes, 54, stride);
                RejectAddress(std::move(bytes), 0x1008);
            }
        }},
        {"translation virtual boundary", [] {
            auto bytes = Fixture();
            const auto start = std::numeric_limits<std::uint64_t>::max() - 31;
            Write(bytes, 80, start);
            Require(Relinker::ElfReader(std::move(bytes)).TranslateVirtualAddress(start + 16) == 0x250);
        }},
        {"translation overflowing file offset", [] {
            auto bytes = Fixture();
            Write<std::uint64_t>(bytes, 72, std::numeric_limits<std::uint64_t>::max() - 3);
            RejectAddress(std::move(bytes), 0x1008);
        }},
        {"translation file mapping range", [] {
            auto bytes = Fixture();
            Write<std::uint64_t>(bytes, 72, 0x2f0);
            RejectAddress(std::move(bytes), 0x1008);
        }},
        {"empty and absent name tables", [] {
            auto bytes = Fixture();
            Write<std::uint16_t>(bytes, 62, 0);
            const auto unnamed = Relinker::ElfReader(bytes).ReadSectionHeaders();
            Require(unnamed.size() == 3 && unnamed[1].Name.empty());
            Write<std::uint16_t>(bytes, 62, 2);
            Write<std::uint64_t>(bytes, 288, 0);
            Write<std::uint32_t>(bytes, 192, 0);
            Write<std::uint32_t>(bytes, 256, 0);
            const auto empty = Relinker::ElfReader(std::move(bytes)).ReadSectionHeaders();
            Require(empty.size() == 3 && empty[1].Name.empty());
        }},
        {"section name at table boundary", [] {
            auto bytes = Fixture();
            Write<std::uint32_t>(bytes, 192, 17);
            RejectSections(std::move(bytes));
        }},
        {"section name table offset overflow", [] {
            auto bytes = Fixture();
            Write<std::uint64_t>(bytes, 280, std::numeric_limits<std::uint64_t>::max() - 3);
            RejectSections(std::move(bytes));
        }},
        {"section table offset overflow", [] {
            auto bytes = Fixture();
            Write<std::uint16_t>(bytes, 62, 0);
            Write<std::uint64_t>(bytes, 40, std::numeric_limits<std::uint64_t>::max() - 31);
            RejectSections(std::move(bytes));
        }},
        {"valid string table suffix and final null", [] {
            auto bytes = Fixture();
            Write<std::uint32_t>(bytes, 192, 2);
            Write<std::uint32_t>(bytes, 256, 16);
            const auto sections = Relinker::ElfReader(std::move(bytes)).ReadSectionHeaders();
            Require(sections[1].Name == "text" && sections[2].Name.empty());
        }},
        {"ELF without section or program tables", [] {
            auto bytes = Fixture();
            Write<std::uint64_t>(bytes, 32, 0);
            Write<std::uint64_t>(bytes, 40, 0);
            Write<std::uint16_t>(bytes, 54, 0);
            Write<std::uint16_t>(bytes, 56, 0);
            Write<std::uint16_t>(bytes, 58, 0);
            Write<std::uint16_t>(bytes, 60, 0);
            Write<std::uint16_t>(bytes, 62, 0);
            const Relinker::ElfReader reader(bytes);
            Require(reader.ReadProgramHeaders().empty() && reader.ReadSectionHeaders().empty());
            RejectAddress(std::move(bytes), 0);
        }},
        {"translation truncated program header table", [] {
            auto bytes = Fixture();
            std::memcpy(bytes.data() + 0x2c8, bytes.data() + 64, 56);
            Write<std::uint64_t>(bytes, 32, 0x2c8);
            Write<std::uint16_t>(bytes, 56, 2);
            RejectAddress(std::move(bytes), 0x1008);
        }},
        {"translation below segment and at virtual maximum", [] {
            RejectAddress(Fixture(), 0x0fff);
            auto bytes = Fixture();
            const auto maximum = std::numeric_limits<std::uint64_t>::max();
            Write(bytes, 80, maximum - 31);
            Require(Relinker::ElfReader(std::move(bytes)).TranslateVirtualAddress(maximum) == 0x25f);
        }},
        {"ordinary translations and zero fill", [] {
            const Relinker::ElfReader reader(Fixture());
            Require(reader.TranslateVirtualAddress(0x1000) == 0x240);
            Require(reader.TranslateVirtualAddress(0x101f) == 0x25f);
            RejectAddress(Fixture(), 0x1020);
        }},
    };
    std::size_t failures = 0;
    for (const auto& [name, test] : cases) {
        try {
            test();
            std::cout << "PASS: " << name << '\n';
        } catch (const std::exception& error) {
            ++failures;
            std::cerr << "FAIL: " << name << ": " << error.what() << '\n';
        }
    }
    return failures == 0 ? 0 : 1;
}
