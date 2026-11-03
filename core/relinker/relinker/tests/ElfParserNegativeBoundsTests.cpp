#include <relinker/parsing/ElfReader.hpp>
#include <elfpatcher/general/ElfConstants.hpp>
#include <cstdint>
#include <cstring>
#include <functional>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using Bytes = std::vector<std::uint8_t>;

template<typename TValue>
void Write(Bytes& bytes, const std::size_t offset, const TValue value) {
    if (offset > bytes.size() || sizeof(value) > bytes.size() - offset) {
        throw std::runtime_error("Fixture offset exceeds bounds");
    }
    std::memcpy(bytes.data() + offset, &value, sizeof(value));
}

void Require(const bool condition) {
    if (!condition) {
        throw std::runtime_error("Negative boundary condition failed");
    }
}

Bytes ValidElfFixture() {
    Bytes bytes(0x400, 0);
    const std::uint8_t magic[] = {0x7f, 'E', 'L', 'F', 2, 1, 1, 0, 0};
    std::memcpy(bytes.data(), magic, sizeof(magic));
    Write<std::uint16_t>(bytes, 0x10, 3);
    Write<std::uint16_t>(bytes, 0x12, 62);
    Write<std::uint32_t>(bytes, 0x14, 1);
    Write<std::uint64_t>(bytes, 0x18, 0x1000);
    Write<std::uint64_t>(bytes, 0x20, 64);
    Write<std::uint64_t>(bytes, 0x28, 256);
    Write<std::uint16_t>(bytes, 0x34, 64);
    Write<std::uint16_t>(bytes, 0x36, 56);
    Write<std::uint16_t>(bytes, 0x38, 2);
    Write<std::uint16_t>(bytes, 0x3a, 64);
    Write<std::uint16_t>(bytes, 0x3c, 3);
    Write<std::uint16_t>(bytes, 0x3e, 2);

    Write<std::uint32_t>(bytes, 64, 1);
    Write<std::uint32_t>(bytes, 68, 7);
    Write<std::uint64_t>(bytes, 72, 0x200);
    Write<std::uint64_t>(bytes, 80, 0x1000);
    Write<std::uint64_t>(bytes, 88, 0x1000);
    Write<std::uint64_t>(bytes, 96, 0x100);
    Write<std::uint64_t>(bytes, 104, 0x200);
    Write<std::uint64_t>(bytes, 112, 0x1000);

    Write<std::uint32_t>(bytes, 120, 2);
    Write<std::uint32_t>(bytes, 124, 6);
    Write<std::uint64_t>(bytes, 128, 0x300);
    Write<std::uint64_t>(bytes, 136, 0x2000);
    Write<std::uint64_t>(bytes, 144, 0x2000);
    Write<std::uint64_t>(bytes, 152, 32);
    Write<std::uint64_t>(bytes, 160, 32);
    Write<std::uint64_t>(bytes, 168, 8);

    Write<std::uint64_t>(bytes, 0x300, 0);
    Write<std::uint64_t>(bytes, 0x308, 0);

    const std::size_t shstrtabOffset = 0x340;
    const char strtab[] = "\0.text\0.shstrtab";
    std::memcpy(bytes.data() + shstrtabOffset, strtab, sizeof(strtab));

    Write<std::uint32_t>(bytes, 256 + 64, 1);
    Write<std::uint32_t>(bytes, 256 + 68, 1);
    Write<std::uint64_t>(bytes, 256 + 72, 6);
    Write<std::uint64_t>(bytes, 256 + 80, 0x1000);
    Write<std::uint64_t>(bytes, 256 + 88, 0x200);
    Write<std::uint64_t>(bytes, 256 + 96, 0x40);
    Write<std::uint64_t>(bytes, 256 + 120, 16);

    Write<std::uint32_t>(bytes, 256 + 128, 7);
    Write<std::uint32_t>(bytes, 256 + 132, 3);
    Write<std::uint64_t>(bytes, 256 + 152, shstrtabOffset);
    Write<std::uint64_t>(bytes, 256 + 160, sizeof(strtab));
    Write<std::uint64_t>(bytes, 256 + 184, 1);

    return bytes;
}

void AssertThrowsMatching(const std::function<void()>& action, const std::string& expectedSubstring) {
    try {
        action();
    } catch (const Relinker::RelinkerException& error) {
        if (!expectedSubstring.empty() && std::string(error.what()).find(expectedSubstring) == std::string::npos) {
            throw std::runtime_error("Exception message mismatch: expected '" + expectedSubstring + "' but got '" + error.what() + "'");
        }
        return;
    } catch (const std::exception& error) {
        throw std::runtime_error("Unexpected exception type: " + std::string(error.what()));
    }
    throw std::runtime_error("Expected exception was not thrown");
}

}

int main() {
    const std::vector<std::pair<std::string, std::function<void()>>> testCases = {
        {"magic validation on empty buffer", [] {
            const Relinker::ElfReader reader(Bytes{});
            AssertThrowsMatching([&] { reader.ValidateMagic(); }, "File too small for ELF header");
        }},
        {"magic validation on truncated buffer under 64 bytes", [] {
            Bytes shortBytes(63, 0);
            const Relinker::ElfReader reader(shortBytes);
            AssertThrowsMatching([&] { reader.ValidateMagic(); }, "File too small for ELF header");
        }},
        {"magic validation rejects SELF container magic 0x1d3d154f", [] {
            auto bytes = ValidElfFixture();
            Write<std::uint32_t>(bytes, 0, 0x1d3d154f);
            const Relinker::ElfReader reader(std::move(bytes));
            AssertThrowsMatching([&] { reader.ValidateMagic(); }, "The input is a SELF container, not an ELF");
        }},
        {"magic validation rejects alternate SELF container magic 0xeef51454", [] {
            auto bytes = ValidElfFixture();
            Write<std::uint32_t>(bytes, 0, 0xeef51454);
            const Relinker::ElfReader reader(std::move(bytes));
            AssertThrowsMatching([&] { reader.ValidateMagic(); }, "The input is a SELF container, not an ELF");
        }},
        {"magic validation rejects non-ELF identifier", [] {
            auto bytes = ValidElfFixture();
            bytes[0] = 'M';
            bytes[1] = 'Z';
            const Relinker::ElfReader reader(std::move(bytes));
            AssertThrowsMatching([&] { reader.ValidateMagic(); }, "Invalid ELF magic number");
        }},
        {"header rejects 32-bit ELF class", [] {
            auto bytes = ValidElfFixture();
            bytes[4] = 1;
            const Relinker::ElfReader reader(std::move(bytes));
            AssertThrowsMatching([&] { reader.ReadHeader(); }, "Expected little-endian ELF64 version 1");
        }},
        {"header rejects big-endian data encoding", [] {
            auto bytes = ValidElfFixture();
            bytes[5] = 2;
            const Relinker::ElfReader reader(std::move(bytes));
            AssertThrowsMatching([&] { reader.ReadHeader(); }, "Expected little-endian ELF64 version 1");
        }},
        {"header rejects non-v1 ELF identification", [] {
            auto bytes = ValidElfFixture();
            bytes[6] = 2;
            const Relinker::ElfReader reader(std::move(bytes));
            AssertThrowsMatching([&] { reader.ReadHeader(); }, "Expected little-endian ELF64 version 1");
        }},
        {"header rejects non-x86-64 machine architectures", [] {
            for (const std::uint16_t machine : {std::uint16_t{3}, std::uint16_t{40}, std::uint16_t{183}, std::uint16_t{243}}) {
                auto bytes = ValidElfFixture();
                Write<std::uint16_t>(bytes, 0x12, machine);
                const Relinker::ElfReader reader(std::move(bytes));
                AssertThrowsMatching([&] { reader.ReadHeader(); }, "Expected an x86-64 ELF");
            }
        }},
        {"header rejects unsupported ELF object version", [] {
            for (const std::uint32_t version : {0u, 2u, 100u}) {
                auto bytes = ValidElfFixture();
                Write<std::uint32_t>(bytes, 0x14, version);
                const Relinker::ElfReader reader(std::move(bytes));
                AssertThrowsMatching([&] { reader.ReadHeader(); }, "Unsupported ELF object version");
            }
        }},
        {"header rejects invalid ELF header size", [] {
            for (const std::uint16_t headerSize : {std::uint16_t{0}, std::uint16_t{56}, std::uint16_t{72}}) {
                auto bytes = ValidElfFixture();
                Write<std::uint16_t>(bytes, 0x34, headerSize);
                const Relinker::ElfReader reader(std::move(bytes));
                AssertThrowsMatching([&] { reader.ReadHeader(); }, "Invalid ELF header size");
            }
        }},
        {"program header entry size mismatch", [] {
            for (const std::uint16_t entrySize : {std::uint16_t{0}, std::uint16_t{48}, std::uint16_t{64}}) {
                auto bytes = ValidElfFixture();
                Write<std::uint16_t>(bytes, 0x36, entrySize);
                const Relinker::ElfReader reader(std::move(bytes));
                AssertThrowsMatching([&] { reader.ReadProgramHeaders(); }, "Invalid ELF program header entry size");
            }
        }},
        {"program header table arithmetic offset overflow", [] {
            auto bytes = ValidElfFixture();
            Write<std::uint64_t>(bytes, 0x20, std::numeric_limits<std::uint64_t>::max() - 16);
            const Relinker::ElfReader reader(std::move(bytes));
            AssertThrowsMatching([&] { reader.ReadProgramHeaders(); }, "FileByteOffset out of bounds");
        }},
        {"program header table multiplication bound overflow", [] {
            auto bytes = ValidElfFixture();
            Write<std::uint16_t>(bytes, 0x38, 0xFFFF);
            const Relinker::ElfReader reader(std::move(bytes));
            AssertThrowsMatching([&] { reader.ReadProgramHeaders(); }, "FileByteOffset out of bounds");
        }},
        {"PT_LOAD file size exceeds memory size", [] {
            auto bytes = ValidElfFixture();
            Write<std::uint64_t>(bytes, 96, 0x300);
            Write<std::uint64_t>(bytes, 104, 0x200);
            const Relinker::ElfReader reader(std::move(bytes));
            AssertThrowsMatching([&] { reader.ReadProgramHeaders(); }, "PT_LOAD file size exceeds memory size");
        }},
        {"PT_LOAD segment offset out of file bounds", [] {
            auto bytes = ValidElfFixture();
            Write<std::uint64_t>(bytes, 72, 0x380);
            Write<std::uint64_t>(bytes, 96, 0x100);
            const Relinker::ElfReader reader(std::move(bytes));
            AssertThrowsMatching([&] { reader.ReadProgramHeaders(); }, "Segment offset out of bounds");
        }},
        {"PT_LOAD segment offset arithmetic overflow", [] {
            auto bytes = ValidElfFixture();
            Write<std::uint64_t>(bytes, 72, std::numeric_limits<std::uint64_t>::max() - 32);
            Write<std::uint64_t>(bytes, 96, 64);
            const Relinker::ElfReader reader(std::move(bytes));
            AssertThrowsMatching([&] { reader.ReadProgramHeaders(); }, "Segment offset out of bounds");
        }},
        {"PT_SCE_RELRO partial overlap with PT_LOAD", [] {
            auto bytes = ValidElfFixture();
            Write<std::uint32_t>(bytes, 120, 0x61000010);
            Write<std::uint64_t>(bytes, 136, 0x1080);
            Write<std::uint64_t>(bytes, 144, 0x1080);
            Write<std::uint64_t>(bytes, 152, 0x200);
            Write<std::uint64_t>(bytes, 160, 0x200);
            const Relinker::ElfReader reader(std::move(bytes));
            AssertThrowsMatching([&] { reader.ReadProgramHeaders(); }, "PT_SCE_RELRO partially overlaps a PT_LOAD segment");
        }},
        {"uncovered PT_SCE_RELRO file size exceeds memory size", [] {
            auto bytes = ValidElfFixture();
            Write<std::uint32_t>(bytes, 120, 0x61000010);
            Write<std::uint64_t>(bytes, 136, 0x5000);
            Write<std::uint64_t>(bytes, 144, 0x5000);
            Write<std::uint64_t>(bytes, 152, 64);
            Write<std::uint64_t>(bytes, 160, 32);
            const Relinker::ElfReader reader(std::move(bytes));
            AssertThrowsMatching([&] { reader.ReadProgramHeaders(); }, "PT_SCE_RELRO file size exceeds memory size");
        }},
        {"uncovered PT_SCE_RELRO segment offset out of file bounds", [] {
            auto bytes = ValidElfFixture();
            Write<std::uint32_t>(bytes, 120, 0x61000010);
            Write<std::uint64_t>(bytes, 128, 0x3f0);
            Write<std::uint64_t>(bytes, 136, 0x5000);
            Write<std::uint64_t>(bytes, 144, 0x5000);
            Write<std::uint64_t>(bytes, 152, 32);
            Write<std::uint64_t>(bytes, 160, 32);
            const Relinker::ElfReader reader(std::move(bytes));
            AssertThrowsMatching([&] { reader.ReadProgramHeaders(); }, "Segment offset out of bounds");
        }},
        {"multiple PT_TLS segments rejected", [] {
            auto bytes = ValidElfFixture();
            bytes.resize(0x500, 0);
            Write<std::uint16_t>(bytes, 0x38, 3);
            Write<std::uint32_t>(bytes, 120, 7);
            Write<std::uint64_t>(bytes, 128, 0x200);
            Write<std::uint64_t>(bytes, 136, 0x1000);
            Write<std::uint64_t>(bytes, 152, 16);
            Write<std::uint64_t>(bytes, 160, 16);
            Write<std::uint64_t>(bytes, 168, 16);

            Write<std::uint32_t>(bytes, 176, 7);
            Write<std::uint64_t>(bytes, 184, 0x210);
            Write<std::uint64_t>(bytes, 192, 0x1010);
            Write<std::uint64_t>(bytes, 208, 16);
            Write<std::uint64_t>(bytes, 216, 16);
            Write<std::uint64_t>(bytes, 224, 16);

            const Relinker::ElfReader reader(std::move(bytes));
            AssertThrowsMatching([&] { reader.ReadProgramHeaders(); }, "Multiple ELF TLS segments");
        }},
        {"PT_TLS file size exceeds memory size", [] {
            auto bytes = ValidElfFixture();
            Write<std::uint32_t>(bytes, 120, 7);
            Write<std::uint64_t>(bytes, 128, 0x200);
            Write<std::uint64_t>(bytes, 136, 0x1000);
            Write<std::uint64_t>(bytes, 152, 64);
            Write<std::uint64_t>(bytes, 160, 32);
            Write<std::uint64_t>(bytes, 168, 8);
            const Relinker::ElfReader reader(std::move(bytes));
            AssertThrowsMatching([&] { reader.ReadProgramHeaders(); }, "Invalid or unsupported ELF TLS layout");
        }},
        {"PT_TLS non-power-of-two alignment", [] {
            for (const std::uint64_t align : {3ULL, 5ULL, 6ULL, 7ULL, 12ULL, 24ULL}) {
                auto bytes = ValidElfFixture();
                Write<std::uint32_t>(bytes, 120, 7);
                Write<std::uint64_t>(bytes, 128, 0x200);
                Write<std::uint64_t>(bytes, 136, 0x1000);
                Write<std::uint64_t>(bytes, 152, 32);
                Write<std::uint64_t>(bytes, 160, 32);
                Write<std::uint64_t>(bytes, 168, align);
                const Relinker::ElfReader reader(std::move(bytes));
                AssertThrowsMatching([&] { reader.ReadProgramHeaders(); }, "Invalid or unsupported ELF TLS layout");
            }
        }},
        {"PT_TLS image outside file bounds", [] {
            auto bytes = ValidElfFixture();
            Write<std::uint32_t>(bytes, 120, 7);
            Write<std::uint64_t>(bytes, 128, 0x3f0);
            Write<std::uint64_t>(bytes, 136, 0x1000);
            Write<std::uint64_t>(bytes, 152, 32);
            Write<std::uint64_t>(bytes, 160, 32);
            Write<std::uint64_t>(bytes, 168, 8);
            const Relinker::ElfReader reader(std::move(bytes));
            AssertThrowsMatching([&] { reader.ReadProgramHeaders(); }, "PT_TLS initialization image is outside the file");
        }},
        {"PT_TLS image outside PT_LOAD memory", [] {
            auto bytes = ValidElfFixture();
            Write<std::uint32_t>(bytes, 120, 7);
            Write<std::uint64_t>(bytes, 128, 0x200);
            Write<std::uint64_t>(bytes, 136, 0x9000);
            Write<std::uint64_t>(bytes, 152, 32);
            Write<std::uint64_t>(bytes, 160, 32);
            Write<std::uint64_t>(bytes, 168, 8);
            const Relinker::ElfReader reader(std::move(bytes));
            AssertThrowsMatching([&] { reader.ReadProgramHeaders(); }, "PT_TLS initialization image is outside PT_LOAD memory");
        }},
        {"section header entry size mismatch", [] {
            for (const std::uint16_t entrySize : {std::uint16_t{0}, std::uint16_t{56}, std::uint16_t{72}}) {
                auto bytes = ValidElfFixture();
                Write<std::uint16_t>(bytes, 0x3a, entrySize);
                const Relinker::ElfReader reader(std::move(bytes));
                AssertThrowsMatching([&] { reader.ReadSectionHeaders(); }, "Invalid ELF section header entry size");
            }
        }},
        {"section header table offset arithmetic overflow", [] {
            auto bytes = ValidElfFixture();
            Write<std::uint64_t>(bytes, 0x28, std::numeric_limits<std::uint64_t>::max() - 32);
            const Relinker::ElfReader reader(std::move(bytes));
            AssertThrowsMatching([&] { reader.ReadSectionHeaders(); }, "Section header table out of bounds");
        }},
        {"section header string table index exceeds section count", [] {
            auto bytes = ValidElfFixture();
            Write<std::uint16_t>(bytes, 0x3e, 10);
            const Relinker::ElfReader reader(std::move(bytes));
            AssertThrowsMatching([&] { reader.ReadSectionHeaders(); }, "Section header string table index out of bounds");
        }},
        {"SHT_SYMTAB section invalid entry size", [] {
            for (const std::uint64_t entrySize : {0ULL, 16ULL, 20ULL, 28ULL, 32ULL}) {
                auto bytes = ValidElfFixture();
                Write<std::uint32_t>(bytes, 256 + 68, 2);
                Write<std::uint64_t>(bytes, 256 + 120, entrySize);
                const Relinker::ElfReader reader(std::move(bytes));
                AssertThrowsMatching([&] { reader.ReadSectionHeaders(); }, "Invalid symbol table entry size");
            }
        }},
        {"section offset extending past file buffer bounds", [] {
            auto bytes = ValidElfFixture();
            Write<std::uint64_t>(bytes, 256 + 88, 0x3f0);
            Write<std::uint64_t>(bytes, 256 + 96, 64);
            const Relinker::ElfReader reader(std::move(bytes));
            AssertThrowsMatching([&] { reader.ReadSectionHeaders(); }, "Section offset out of bounds");
        }},
        {"section offset arithmetic overflow", [] {
            auto bytes = ValidElfFixture();
            Write<std::uint64_t>(bytes, 256 + 88, std::numeric_limits<std::uint64_t>::max() - 16);
            Write<std::uint64_t>(bytes, 256 + 96, 64);
            const Relinker::ElfReader reader(std::move(bytes));
            AssertThrowsMatching([&] { reader.ReadSectionHeaders(); }, "Section offset out of bounds");
        }},
        {"section name string table wrong section type", [] {
            auto bytes = ValidElfFixture();
            Write<std::uint32_t>(bytes, 256 + 132, 1);
            const Relinker::ElfReader reader(std::move(bytes));
            AssertThrowsMatching([&] { reader.ReadSectionHeaders(); }, "Section names do not reference a string table");
        }},
        {"section name string table out of bounds", [] {
            auto bytes = ValidElfFixture();
            Write<std::uint64_t>(bytes, 256 + 152, 0x3f0);
            Write<std::uint64_t>(bytes, 256 + 160, 64);
            const Relinker::ElfReader reader(std::move(bytes));
            AssertThrowsMatching([&] { reader.ReadSectionHeaders(); }, "Section name string table out of bounds");
        }},
        {"section name offset exceeds string table size", [] {
            auto bytes = ValidElfFixture();
            Write<std::uint32_t>(bytes, 256 + 64, 100);
            const Relinker::ElfReader reader(std::move(bytes));
            AssertThrowsMatching([&] { reader.ReadSectionHeaders(); }, "Section name offset out of bounds");
        }},
        {"unterminated section name string", [] {
            auto bytes = ValidElfFixture();
            for (std::size_t i = 0x340; i < 0x340 + 32; ++i) bytes[i] = 'A';
            const Relinker::ElfReader reader(std::move(bytes));
            AssertThrowsMatching([&] { reader.ReadSectionHeaders(); }, "Unterminated section name");
        }},
        {"dynamic segment out of bounds", [] {
            const auto bytes = ValidElfFixture();
            const Relinker::ElfReader reader(bytes);
            Domain::ProgramHeader dynamicHeader{};
            dynamicHeader.Type = 2;
            dynamicHeader.Offset = 0x3f0;
            dynamicHeader.FileSize = 32;
            AssertThrowsMatching([&] { reader.ReadDynamicTags(dynamicHeader); }, "Dynamic segment out of bounds");
        }},
        {"unterminated dynamic segment without DT_NULL", [] {
            auto bytes = ValidElfFixture();
            Write<std::uint64_t>(bytes, 0x300, 1);
            Write<std::uint64_t>(bytes, 0x308, 10);
            Write<std::uint64_t>(bytes, 0x310, 2);
            Write<std::uint64_t>(bytes, 0x318, 20);
            const Relinker::ElfReader reader(bytes);
            Domain::ProgramHeader dynamicHeader{};
            dynamicHeader.Type = 2;
            dynamicHeader.Offset = 0x300;
            dynamicHeader.FileSize = 32;
            AssertThrowsMatching([&] { reader.ReadDynamicTags(dynamicHeader); }, "Unterminated dynamic segment");
        }},
        {"address translation rejects unmapped virtual addresses", [] {
            const Relinker::ElfReader reader(ValidElfFixture());
            AssertThrowsMatching([&] { reader.TranslateVirtualAddress(0x0fff); }, "Virtual address not mapped by any PT_LOAD segment");
            AssertThrowsMatching([&] { reader.TranslateVirtualAddress(0x1200); }, "Virtual address not mapped by any PT_LOAD segment");
            AssertThrowsMatching([&] { reader.TranslateVirtualAddress(0x3000); }, "Virtual address not mapped by any PT_LOAD segment");
        }},
        {"segment reading rejects out of bounds header offset", [] {
            const Relinker::ElfReader reader(ValidElfFixture());
            Domain::ProgramHeader invalidSegment{};
            invalidSegment.Offset = 0x3f0;
            invalidSegment.FileSize = 64;
            AssertThrowsMatching([&] { reader.ReadSegment(invalidSegment); }, "Segment offset out of bounds");
        }}
    };

    std::size_t passed = 0;
    for (const auto& [name, test] : testCases) {
        try {
            test();
            ++passed;
        } catch (const std::exception& error) {
            std::cerr << "FAIL: " << name << ": " << error.what() << '\n';
            return 1;
        }
    }

    std::cout << "All " << passed << " ELF parser negative boundary tests passed.\n";
    return 0;
}
