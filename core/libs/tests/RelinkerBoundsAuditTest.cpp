#include <relinker/parsing/ElfReader.hpp>
#include <relinker/analysis/UnusedNidFilter/EhFrameReader.hpp>
#include <relinker/domain/Types.hpp>
#include <elfpatcher/general/ElfConstants.hpp>
#include <io/BufferUtils.hpp>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using Bytes = std::vector<std::uint8_t>;

void Require(const bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error("Validation failure: " + message);
}

template<typename T>
void Write(Bytes& bytes, const std::size_t offset, const T value) {
    if (offset > bytes.size() || sizeof(value) > bytes.size() - offset) {
        throw std::runtime_error("Buffer offset exceeds fixture capacity");
    }
    std::memcpy(bytes.data() + offset, &value, sizeof(value));
}

Bytes CreateValidElfFixture() {
    Bytes bytes(0x400, 0);
    const std::uint8_t magic[] = {0x7f, 'E', 'L', 'F', 2, 1, 1, 0, 0};
    std::memcpy(bytes.data(), magic, sizeof(magic));
    Write<std::uint16_t>(bytes, 16, 3);
    Write<std::uint16_t>(bytes, 18, 62);
    Write<std::uint32_t>(bytes, 20, 1);
    Write<std::uint64_t>(bytes, 24, 0x1000);
    Write<std::uint64_t>(bytes, 32, 64);
    Write<std::uint64_t>(bytes, 40, 128);
    Write<std::uint16_t>(bytes, 52, 64);
    Write<std::uint16_t>(bytes, 54, 56);
    Write<std::uint16_t>(bytes, 56, 1);
    Write<std::uint16_t>(bytes, 58, 64);
    Write<std::uint16_t>(bytes, 60, 3);
    Write<std::uint16_t>(bytes, 62, 2);

    Write<std::uint32_t>(bytes, 64, 1);
    Write<std::uint32_t>(bytes, 68, 5);
    Write<std::uint64_t>(bytes, 72, 0x240);
    Write<std::uint64_t>(bytes, 80, 0x1000);
    Write<std::uint64_t>(bytes, 88, 0x1000);
    Write<std::uint64_t>(bytes, 96, 0x100);
    Write<std::uint64_t>(bytes, 104, 0x200);
    Write<std::uint64_t>(bytes, 112, 0x1000);

    Write<std::uint32_t>(bytes, 192, 1);
    Write<std::uint32_t>(bytes, 196, 1);
    Write<std::uint64_t>(bytes, 200, 6);
    Write<std::uint64_t>(bytes, 208, 0x1000);
    Write<std::uint64_t>(bytes, 216, 0x240);
    Write<std::uint64_t>(bytes, 224, 0x100);
    Write<std::uint32_t>(bytes, 232, 0);
    Write<std::uint32_t>(bytes, 236, 0);
    Write<std::uint64_t>(bytes, 240, 16);
    Write<std::uint64_t>(bytes, 248, 0);

    Write<std::uint32_t>(bytes, 256, 7);
    Write<std::uint32_t>(bytes, 260, 3);
    Write<std::uint64_t>(bytes, 264, 0);
    Write<std::uint64_t>(bytes, 272, 0);
    Write<std::uint64_t>(bytes, 280, 0x380);
    const char shstrtab[] = "\0.text\0.shstrtab";
    Write<std::uint64_t>(bytes, 288, sizeof(shstrtab));
    Write<std::uint32_t>(bytes, 296, 0);
    Write<std::uint32_t>(bytes, 300, 0);
    Write<std::uint64_t>(bytes, 304, 1);
    Write<std::uint64_t>(bytes, 312, 0);

    std::memcpy(bytes.data() + 0x380, shstrtab, sizeof(shstrtab));
    return bytes;
}

void TestElfHeaderBounds() {
    auto base = CreateValidElfFixture();

    {
        Bytes tooSmall(32, 0);
        bool rejected = false;
        try {
            Relinker::ElfReader reader(std::move(tooSmall));
            reader.ReadHeader();
        } catch (const Domain::RelinkerException& ex) {
            rejected = std::string(ex.what()).find("File too small for ELF header") != std::string::npos;
        }
        Require(rejected, "buffer smaller than 64 bytes must be rejected");
    }

    {
        auto selfBytes1 = base;
        Write<std::uint32_t>(selfBytes1, 0, 0x1d3d154f);
        bool rejected1 = false;
        try {
            Relinker::ElfReader reader(std::move(selfBytes1));
            reader.ValidateMagic();
        } catch (const Domain::RelinkerException& ex) {
            rejected1 = std::string(ex.what()).find("The input is a SELF container, not an ELF") != std::string::npos;
        }
        Require(rejected1, "SELF container magic 0x1d3d154f must be rejected");

        auto selfBytes2 = base;
        Write<std::uint32_t>(selfBytes2, 0, 0xeef51454);
        bool rejected2 = false;
        try {
            Relinker::ElfReader reader(std::move(selfBytes2));
            reader.ValidateMagic();
        } catch (const Domain::RelinkerException& ex) {
            rejected2 = std::string(ex.what()).find("The input is a SELF container, not an ELF") != std::string::npos;
        }
        Require(rejected2, "SELF container magic 0xeef51454 must be rejected");
    }

    {
        auto badMagic = base;
        badMagic[0] = 0x7E;
        bool rejected = false;
        try {
            Relinker::ElfReader reader(std::move(badMagic));
            reader.ValidateMagic();
        } catch (const Domain::RelinkerException& ex) {
            rejected = std::string(ex.what()).find("Invalid ELF magic number") != std::string::npos;
        }
        Require(rejected, "corrupt ELF magic must be rejected");
    }

    {
        auto badClass = base;
        badClass[4] = 1;
        bool rejected = false;
        try {
            Relinker::ElfReader reader(std::move(badClass));
            reader.ReadHeader();
        } catch (const Domain::RelinkerException& ex) {
            rejected = std::string(ex.what()).find("Expected little-endian ELF64 version 1") != std::string::npos;
        }
        Require(rejected, "32-bit ELF class must be rejected");
    }

    {
        auto badEndian = base;
        badEndian[5] = 2;
        bool rejected = false;
        try {
            Relinker::ElfReader reader(std::move(badEndian));
            reader.ReadHeader();
        } catch (const Domain::RelinkerException& ex) {
            rejected = std::string(ex.what()).find("Expected little-endian ELF64 version 1") != std::string::npos;
        }
        Require(rejected, "big-endian ELF data must be rejected");
    }

    {
        auto badMachine = base;
        Write<std::uint16_t>(badMachine, 18, 3);
        bool rejected = false;
        try {
            Relinker::ElfReader reader(std::move(badMachine));
            reader.ReadHeader();
        } catch (const Domain::RelinkerException& ex) {
            rejected = std::string(ex.what()).find("Expected an x86-64 ELF") != std::string::npos;
        }
        Require(rejected, "non-x86_64 machine must be rejected");
    }

    {
        auto badVersion = base;
        Write<std::uint32_t>(badVersion, 20, 2);
        bool rejected = false;
        try {
            Relinker::ElfReader reader(std::move(badVersion));
            reader.ReadHeader();
        } catch (const Domain::RelinkerException& ex) {
            rejected = std::string(ex.what()).find("Unsupported ELF object version") != std::string::npos;
        }
        Require(rejected, "unsupported ELF version must be rejected");
    }

    {
        auto badEhSize = base;
        Write<std::uint16_t>(badEhSize, 52, 32);
        bool rejected = false;
        try {
            Relinker::ElfReader reader(std::move(badEhSize));
            reader.ReadHeader();
        } catch (const Domain::RelinkerException& ex) {
            rejected = std::string(ex.what()).find("Invalid ELF header size: expected 64 bytes") != std::string::npos;
        }
        Require(rejected, "invalid ELF header size must be rejected");
    }

    {
        auto badPhEntSize = base;
        Write<std::uint16_t>(badPhEntSize, 54, 40);
        bool rejected = false;
        try {
            Relinker::ElfReader reader(std::move(badPhEntSize));
            reader.ReadProgramHeaders();
        } catch (const Domain::RelinkerException& ex) {
            rejected = std::string(ex.what()).find("Invalid ELF program header entry size: expected 56 bytes") != std::string::npos;
        }
        Require(rejected, "invalid program header entry size must be rejected");
    }

    {
        auto badShEntSize = base;
        Write<std::uint16_t>(badShEntSize, 58, 48);
        bool rejected = false;
        try {
            Relinker::ElfReader reader(std::move(badShEntSize));
            reader.ReadSectionHeaders();
        } catch (const Domain::RelinkerException& ex) {
            rejected = std::string(ex.what()).find("Invalid ELF section header entry size: expected 64 bytes") != std::string::npos;
        }
        Require(rejected, "invalid section header entry size must be rejected");
    }
}

void TestProgramAndSectionBounds() {
    auto base = CreateValidElfFixture();

    {
        auto phOffOob = base;
        Write<std::uint64_t>(phOffOob, 32, 0x2000);
        bool rejected = false;
        try {
            Relinker::ElfReader reader(std::move(phOffOob));
            reader.ReadProgramHeaders();
        } catch (const Domain::RelinkerException& ex) {
            rejected = std::string(ex.what()).find("FileByteOffset out of bounds") != std::string::npos;
        }
        Require(rejected, "program header offset out of bounds must be rejected");
    }

    {
        auto phOffOverflow = base;
        Write<std::uint64_t>(phOffOverflow, 32, std::numeric_limits<std::uint64_t>::max() - 10);
        bool rejected = false;
        try {
            Relinker::ElfReader reader(std::move(phOffOverflow));
            reader.ReadProgramHeaders();
        } catch (const Domain::RelinkerException& ex) {
            rejected = std::string(ex.what()).find("FileByteOffset out of bounds") != std::string::npos;
        }
        Require(rejected, "program header offset arithmetic overflow must be rejected");
    }

    {
        auto shOffOob = base;
        Write<std::uint64_t>(shOffOob, 40, 0x3000);
        bool rejected = false;
        try {
            Relinker::ElfReader reader(std::move(shOffOob));
            reader.ReadSectionHeaders();
        } catch (const Domain::RelinkerException& ex) {
            rejected = std::string(ex.what()).find("Section header table out of bounds") != std::string::npos;
        }
        Require(rejected, "section header offset out of bounds must be rejected");
    }

    {
        auto shOffOverflow = base;
        Write<std::uint64_t>(shOffOverflow, 40, std::numeric_limits<std::uint64_t>::max() - 20);
        bool rejected = false;
        try {
            Relinker::ElfReader reader(std::move(shOffOverflow));
            reader.ReadSectionHeaders();
        } catch (const Domain::RelinkerException& ex) {
            rejected = std::string(ex.what()).find("Section header table out of bounds") != std::string::npos;
        }
        Require(rejected, "section header offset arithmetic overflow must be rejected");
    }

    {
        auto ptLoadMemOob = base;
        Write<std::uint64_t>(ptLoadMemOob, 96, 0x400);
        Write<std::uint64_t>(ptLoadMemOob, 104, 0x100);
        bool rejected = false;
        try {
            Relinker::ElfReader reader(std::move(ptLoadMemOob));
            reader.ReadProgramHeaders();
        } catch (const Domain::RelinkerException& ex) {
            rejected = std::string(ex.what()).find("PT_LOAD file size exceeds memory size") != std::string::npos;
        }
        Require(rejected, "PT_LOAD file size exceeding memory size must be rejected");
    }

    {
        auto ptLoadOffsetOob = base;
        Write<std::uint64_t>(ptLoadOffsetOob, 72, 0x2000);
        bool rejected = false;
        try {
            Relinker::ElfReader reader(std::move(ptLoadOffsetOob));
            reader.ReadProgramHeaders();
        } catch (const Domain::RelinkerException& ex) {
            rejected = std::string(ex.what()).find("Segment offset out of bounds") != std::string::npos;
        }
        Require(rejected, "PT_LOAD segment offset out of bounds must be rejected");
    }

    {
        auto shOffsetOob = base;
        Write<std::uint64_t>(shOffsetOob, 216, 0x2000);
        bool rejected = false;
        try {
            Relinker::ElfReader reader(std::move(shOffsetOob));
            reader.ReadSectionHeaders();
        } catch (const Domain::RelinkerException& ex) {
            rejected = std::string(ex.what()).find("Section offset out of bounds") != std::string::npos;
        }
        Require(rejected, "section offset out of bounds must be rejected");
    }

    {
        auto shAlignNonPowerTwo = base;
        Write<std::uint64_t>(shAlignNonPowerTwo, 240, 7);
        bool rejected = false;
        try {
            Relinker::ElfReader reader(std::move(shAlignNonPowerTwo));
            reader.ReadSectionHeaders();
        } catch (const Domain::RelinkerException& ex) {
            rejected = std::string(ex.what()).find("Section header alignment is not a power of two") != std::string::npos;
        }
        Require(rejected, "non-power-of-two section alignment must be rejected");
    }

    {
        auto shLinkOob = base;
        Write<std::uint32_t>(shLinkOob, 232, 99);
        bool rejected = false;
        try {
            Relinker::ElfReader reader(std::move(shLinkOob));
            reader.ReadSectionHeaders();
        } catch (const Domain::RelinkerException& ex) {
            rejected = std::string(ex.what()).find("Section link index out of bounds") != std::string::npos;
        }
        Require(rejected, "section link index out of bounds must be rejected");
    }
}

void TestDynamicRelocationTableBounds() {
    auto base = CreateValidElfFixture();

    {
        Domain::ProgramHeader dynamicHeader{};
        dynamicHeader.Type = Elfpatcher::PT_DYNAMIC;
        dynamicHeader.Offset = 0x2000;
        dynamicHeader.FileSize = 0x100;
        bool rejected = false;
        try {
            Relinker::ElfReader reader(base);
            reader.ReadDynamicTags(dynamicHeader);
        } catch (const Domain::RelinkerException& ex) {
            rejected = std::string(ex.what()).find("Dynamic segment out of bounds") != std::string::npos;
        }
        Require(rejected, "dynamic segment out of bounds must be rejected");
    }

    {
        auto unterminated = base;
        for (std::size_t offset = 0x240; offset < 0x240 + 32; offset += 16) {
            Write<std::int64_t>(unterminated, offset, Elfpatcher::DT_NEEDED);
            Write<std::uint64_t>(unterminated, offset + 8, 1);
        }
        Domain::ProgramHeader dynamicHeader{};
        dynamicHeader.Type = Elfpatcher::PT_DYNAMIC;
        dynamicHeader.Offset = 0x240;
        dynamicHeader.FileSize = 32;
        bool rejected = false;
        try {
            Relinker::ElfReader reader(std::move(unterminated));
            reader.ReadDynamicTags(dynamicHeader);
        } catch (const Domain::RelinkerException& ex) {
            rejected = std::string(ex.what()).find("Unterminated dynamic segment") != std::string::npos;
        }
        Require(rejected, "unterminated dynamic segment must be rejected");
    }

    {
        auto validDyn = base;
        Write<std::int64_t>(validDyn, 0x240, Elfpatcher::DT_RELA);
        Write<std::uint64_t>(validDyn, 0x248, 0x300);
        Write<std::int64_t>(validDyn, 0x250, Elfpatcher::DT_RELASZ);
        Write<std::uint64_t>(validDyn, 0x258, 24);
        Write<std::int64_t>(validDyn, 0x260, Elfpatcher::DT_RELAENT);
        Write<std::uint64_t>(validDyn, 0x268, 24);
        Write<std::int64_t>(validDyn, 0x270, Elfpatcher::DT_NULL);
        Write<std::uint64_t>(validDyn, 0x278, 0);

        Domain::ProgramHeader dynamicHeader{};
        dynamicHeader.Type = Elfpatcher::PT_DYNAMIC;
        dynamicHeader.Offset = 0x240;
        dynamicHeader.FileSize = 64;
        Relinker::ElfReader reader(std::move(validDyn));
        const auto tags = reader.ReadDynamicTags(dynamicHeader);
        Require(tags.size() == 3, "expected 3 tags before DT_NULL");
        Require(tags[0].Tag == Elfpatcher::DT_RELA && tags[0].Value == 0x300, "DT_RELA must match");
        Require(tags[1].Tag == Elfpatcher::DT_RELASZ && tags[1].Value == 24, "DT_RELASZ must match");
        Require(tags[2].Tag == Elfpatcher::DT_RELAENT && tags[2].Value == 24, "DT_RELAENT must match");
    }

    auto auditRelocationTableBounds = [](const std::size_t fileSize,
                                         const std::uint64_t relaOffset,
                                         const std::uint64_t relaSize,
                                         const std::uint64_t jmpRelOffset,
                                         const std::uint64_t jmpRelSize,
                                         const std::uint64_t relaEntSize,
                                         const bool hasPltRelocations,
                                         const std::int64_t pltRelType) {
        constexpr std::size_t kExpectedRelaEntSize = 24;
        if (relaEntSize != kExpectedRelaEntSize) {
            throw Domain::RelinkerException("Unsupported DT_RELAENT value: " + std::to_string(relaEntSize) + "; expected 24 bytes");
        }
        if (relaSize % kExpectedRelaEntSize != 0) {
            throw Domain::RelinkerException("Invalid DT_RELASZ value: " + std::to_string(relaSize) + " bytes; expected a multiple of 24 bytes");
        }
        if (hasPltRelocations) {
            if (pltRelType != Elfpatcher::DT_RELA) {
                throw Domain::RelinkerException("Unsupported DT_PLTREL type: " + std::to_string(pltRelType) + "; expected DT_RELA (7)");
            }
            if (jmpRelSize % kExpectedRelaEntSize != 0) {
                throw Domain::RelinkerException("Invalid DT_PLTRELSZ value: " + std::to_string(jmpRelSize) + " bytes; expected a multiple of 24 bytes");
            }
            if (jmpRelOffset > fileSize || jmpRelSize > fileSize - jmpRelOffset || jmpRelOffset + jmpRelSize < jmpRelOffset) {
                throw Domain::RelinkerException("Jump relocation table is out of bounds", jmpRelOffset);
            }
        }
        if (relaOffset > fileSize || relaSize > fileSize - relaOffset || relaOffset + relaSize < relaOffset) {
            throw Domain::RelinkerException("Relocation table is out of bounds", relaOffset);
        }
        if (relaSize != 0 && jmpRelSize != 0 &&
            relaOffset < jmpRelOffset + jmpRelSize && jmpRelOffset < relaOffset + relaSize &&
            relaOffset % kExpectedRelaEntSize != jmpRelOffset % kExpectedRelaEntSize) {
            throw Domain::RelinkerException("Unaligned overlapping relocation tables", jmpRelOffset);
        }
    };

    {
        bool threw = false;
        try {
            auditRelocationTableBounds(0x8000, 0xFFFFFFFFFFFFFFF8ULL, 24, 0, 0, 24, false, Elfpatcher::DT_RELA);
        } catch (const Domain::RelinkerException& ex) {
            threw = std::string(ex.what()).find("Relocation table is out of bounds") != std::string::npos;
        }
        Require(threw, "near UINT64_MAX rela offset must throw out of bounds");
    }

    {
        bool threw = false;
        try {
            auditRelocationTableBounds(0x8000, 0x8000 - 16, 48, 0, 0, 24, false, Elfpatcher::DT_RELA);
        } catch (const Domain::RelinkerException& ex) {
            threw = std::string(ex.what()).find("Relocation table is out of bounds") != std::string::npos;
        }
        Require(threw, "rela table extending past EOF must throw out of bounds");
    }

    {
        bool threw = false;
        try {
            auditRelocationTableBounds(0x8000, 0x1000, 25, 0, 0, 24, false, Elfpatcher::DT_RELA);
        } catch (const Domain::RelinkerException& ex) {
            threw = std::string(ex.what()).find("Invalid DT_RELASZ value") != std::string::npos;
        }
        Require(threw, "misaligned rela size not divisible by 24 must throw");
    }

    {
        bool threw = false;
        try {
            auditRelocationTableBounds(0x8000, 0x1000, 24, 0, 0, 16, false, Elfpatcher::DT_RELA);
        } catch (const Domain::RelinkerException& ex) {
            threw = std::string(ex.what()).find("Unsupported DT_RELAENT value") != std::string::npos;
        }
        Require(threw, "unsupported entry size != 24 must throw");
    }

    {
        bool threw = false;
        try {
            auditRelocationTableBounds(0x8000, 0x1000, 24, 0x8000 - 8, 24, 24, true, Elfpatcher::DT_RELA);
        } catch (const Domain::RelinkerException& ex) {
            threw = std::string(ex.what()).find("Jump relocation table is out of bounds") != std::string::npos;
        }
        Require(threw, "jump relocation table past EOF must throw");
    }

    {
        bool threw = false;
        try {
            auditRelocationTableBounds(0x8000, 0x1000, 24, 0x2000, 25, 24, true, Elfpatcher::DT_RELA);
        } catch (const Domain::RelinkerException& ex) {
            threw = std::string(ex.what()).find("Invalid DT_PLTRELSZ value") != std::string::npos;
        }
        Require(threw, "misaligned jump relocation size not divisible by 24 must throw");
    }

    {
        bool threw = false;
        try {
            auditRelocationTableBounds(0x8000, 0x1000, 24, 0x2000, 24, 24, true, Elfpatcher::DT_REL);
        } catch (const Domain::RelinkerException& ex) {
            threw = std::string(ex.what()).find("Unsupported DT_PLTREL type") != std::string::npos;
        }
        Require(threw, "unsupported DT_PLTREL type != DT_RELA must throw");
    }

    {
        bool threw = false;
        try {
            auditRelocationTableBounds(0x8000, 0x1000, 48, 0x1008, 48, 24, true, Elfpatcher::DT_RELA);
        } catch (const Domain::RelinkerException& ex) {
            threw = std::string(ex.what()).find("Unaligned overlapping relocation tables") != std::string::npos;
        }
        Require(threw, "overlapping relocation tables with misaligned stride must throw");
    }

    {
        bool ok = true;
        try {
            auditRelocationTableBounds(0x8000, 0x1000, 48, 0x1000, 24, 24, true, Elfpatcher::DT_RELA);
            auditRelocationTableBounds(0x8000, 0x1000, 48, 0x2000, 24, 24, true, Elfpatcher::DT_RELA);
        } catch (const std::exception&) {
            ok = false;
        }
        Require(ok, "valid relocation bounds configuration must be accepted");
    }
}

void TestRelocationEntryBounds() {
    auto auditRelocationEntries = [](const std::vector<std::uint8_t>& table,
                                     const std::size_t dynSymCount,
                                     const std::vector<Domain::ProgramHeader>& headers) {
        if (table.size() % 24 != 0) {
            throw Domain::RelinkerException("Invalid ELF RELA table size");
        }
        std::map<std::uint64_t, std::uint64_t> ranges;
        for (std::size_t offset = 0; offset < table.size(); offset += 24) {
            const auto target = Io::ReadU64(table, offset);
            const auto info = Io::ReadU64(table, offset + 8);
            const auto addend = Io::ReadI64(table, offset + 16);
            const auto type = static_cast<std::uint32_t>(info);
            const auto symbol = static_cast<std::uint32_t>(info >> 32);

            if (type != Elfpatcher::R_X86_64_64 &&
                type != Elfpatcher::R_X86_64_GLOB_DAT &&
                type != Elfpatcher::R_X86_64_JUMP_SLOT &&
                type != Elfpatcher::R_X86_64_RELATIVE) {
                throw Domain::RelinkerException("Relocation type " + std::to_string(type) + " is not supported for this target", target);
            }
            if (target > std::numeric_limits<std::uint64_t>::max() - 8) {
                throw Domain::RelinkerException("Relocation target address overflow", target);
            }
            const auto next = ranges.lower_bound(target);
            if ((next != ranges.end() && next->first < target + 8) || (next != ranges.begin() && std::prev(next)->second > target)) {
                throw Domain::RelinkerException("Overlapping ELF relocation targets", target);
            }
            ranges.emplace(target, target + 8);

            bool loaded = false;
            for (const auto& header : headers) {
                if (header.Type == Elfpatcher::PT_LOAD &&
                    target >= header.MappedAddress &&
                    target - header.MappedAddress < header.MemorySize &&
                    8 <= header.MemorySize - (target - header.MappedAddress)) {
                    loaded = true;
                    break;
                }
            }
            if (!loaded) {
                throw Domain::RelinkerException("RELA r_offset 0x" + std::to_string(target) + " does not fit inside a PT_LOAD segment", target);
            }

            if (type == Elfpatcher::R_X86_64_RELATIVE) {
                if (symbol != 0) {
                    throw Domain::RelinkerException("RELATIVE relocation has a symbol index", target);
                }
            } else if (type == Elfpatcher::R_X86_64_GLOB_DAT || type == Elfpatcher::R_X86_64_JUMP_SLOT) {
                if (symbol == 0 || symbol >= dynSymCount) {
                    throw Domain::RelinkerException("Invalid import symbol index", target);
                }
                if (addend != 0) {
                    throw Domain::RelinkerException("GLOB_DAT/JUMP_SLOT relocation has a nonzero addend", target);
                }
            } else if (type == Elfpatcher::R_X86_64_64) {
                if (symbol != 0 && symbol >= dynSymCount) {
                    throw Domain::RelinkerException("Invalid relocation symbol index", target);
                }
            }
        }
    };

    Domain::ProgramHeader ptLoad{};
    ptLoad.Type = Elfpatcher::PT_LOAD;
    ptLoad.MappedAddress = 0x1000;
    ptLoad.MemorySize = 0x1000;

    auto makeRelaEntry = [](const std::uint64_t rOffset, const std::uint32_t symIndex, const std::uint32_t type, const std::int64_t addend) {
        std::vector<std::uint8_t> entry(24, 0);
        Io::WriteU64(entry, 0, rOffset);
        Io::WriteU64(entry, 8, (static_cast<std::uint64_t>(symIndex) << 32) | static_cast<std::uint64_t>(type));
        Io::WriteI64(entry, 16, addend);
        return entry;
    };

    {
        std::vector<std::uint8_t> misaligned(20, 0);
        bool threw = false;
        try {
            auditRelocationEntries(misaligned, 4, {ptLoad});
        } catch (const Domain::RelinkerException& ex) {
            threw = std::string(ex.what()).find("Invalid ELF RELA table size") != std::string::npos;
        }
        Require(threw, "misaligned RELA entry size must throw");
    }

    {
        auto entry = makeRelaEntry(0x1100, 0, 99, 0);
        bool threw = false;
        try {
            auditRelocationEntries(entry, 4, {ptLoad});
        } catch (const Domain::RelinkerException& ex) {
            threw = std::string(ex.what()).find("is not supported for this target") != std::string::npos && ex.FailureOffset == 0x1100;
        }
        Require(threw, "unsupported relocation type must throw with failure offset");
    }

    {
        auto entry = makeRelaEntry(std::numeric_limits<std::uint64_t>::max() - 4, 0, Elfpatcher::R_X86_64_RELATIVE, 0);
        bool threw = false;
        try {
            auditRelocationEntries(entry, 4, {ptLoad});
        } catch (const Domain::RelinkerException& ex) {
            threw = std::string(ex.what()).find("Relocation target address overflow") != std::string::npos && ex.FailureOffset == std::numeric_limits<std::uint64_t>::max() - 4;
        }
        Require(threw, "relocation target address overflow must throw with failure offset");
    }

    {
        auto entry1 = makeRelaEntry(0x1100, 0, Elfpatcher::R_X86_64_RELATIVE, 0);
        auto entry2 = makeRelaEntry(0x1104, 0, Elfpatcher::R_X86_64_RELATIVE, 0);
        entry1.insert(entry1.end(), entry2.begin(), entry2.end());
        bool threw = false;
        try {
            auditRelocationEntries(entry1, 4, {ptLoad});
        } catch (const Domain::RelinkerException& ex) {
            threw = std::string(ex.what()).find("Overlapping ELF relocation targets") != std::string::npos && ex.FailureOffset == 0x1104;
        }
        Require(threw, "overlapping relocation targets must throw with failure offset");
    }

    {
        auto entry = makeRelaEntry(0x5000, 0, Elfpatcher::R_X86_64_RELATIVE, 0);
        bool threw = false;
        try {
            auditRelocationEntries(entry, 4, {ptLoad});
        } catch (const Domain::RelinkerException& ex) {
            threw = std::string(ex.what()).find("does not fit inside a PT_LOAD segment") != std::string::npos && ex.FailureOffset == 0x5000;
        }
        Require(threw, "relocation target outside PT_LOAD must throw with failure offset");
    }

    {
        auto entry = makeRelaEntry(0x1100, 5, Elfpatcher::R_X86_64_RELATIVE, 0);
        bool threw = false;
        try {
            auditRelocationEntries(entry, 4, {ptLoad});
        } catch (const Domain::RelinkerException& ex) {
            threw = std::string(ex.what()).find("RELATIVE relocation has a symbol index") != std::string::npos && ex.FailureOffset == 0x1100;
        }
        Require(threw, "RELATIVE relocation with nonzero symbol index must throw");
    }

    {
        auto entry = makeRelaEntry(0x1100, 0, Elfpatcher::R_X86_64_GLOB_DAT, 0);
        bool threw = false;
        try {
            auditRelocationEntries(entry, 4, {ptLoad});
        } catch (const Domain::RelinkerException& ex) {
            threw = std::string(ex.what()).find("Invalid import symbol index") != std::string::npos && ex.FailureOffset == 0x1100;
        }
        Require(threw, "import relocation with symbol index 0 must throw");
    }

    {
        auto entry = makeRelaEntry(0x1100, 10, Elfpatcher::R_X86_64_GLOB_DAT, 0);
        bool threw = false;
        try {
            auditRelocationEntries(entry, 4, {ptLoad});
        } catch (const Domain::RelinkerException& ex) {
            threw = std::string(ex.what()).find("Invalid import symbol index") != std::string::npos && ex.FailureOffset == 0x1100;
        }
        Require(threw, "import relocation with symbol index exceeding symbol count must throw");
    }

    {
        auto entry = makeRelaEntry(0x1100, 2, Elfpatcher::R_X86_64_GLOB_DAT, 0x10);
        bool threw = false;
        try {
            auditRelocationEntries(entry, 4, {ptLoad});
        } catch (const Domain::RelinkerException& ex) {
            threw = std::string(ex.what()).find("GLOB_DAT/JUMP_SLOT relocation has a nonzero addend") != std::string::npos && ex.FailureOffset == 0x1100;
        }
        Require(threw, "GLOB_DAT relocation with nonzero addend must throw");
    }

    {
        auto entry = makeRelaEntry(0x1100, 10, Elfpatcher::R_X86_64_64, 0);
        bool threw = false;
        try {
            auditRelocationEntries(entry, 4, {ptLoad});
        } catch (const Domain::RelinkerException& ex) {
            threw = std::string(ex.what()).find("Invalid relocation symbol index") != std::string::npos && ex.FailureOffset == 0x1100;
        }
        Require(threw, "symbol relocation with index exceeding symbol count must throw");
    }

    {
        auto entry1 = makeRelaEntry(0x1100, 0, Elfpatcher::R_X86_64_RELATIVE, 0x10);
        auto entry2 = makeRelaEntry(0x1120, 2, Elfpatcher::R_X86_64_GLOB_DAT, 0);
        entry1.insert(entry1.end(), entry2.begin(), entry2.end());
        bool ok = true;
        try {
            auditRelocationEntries(entry1, 4, {ptLoad});
        } catch (const std::exception&) {
            ok = false;
        }
        Require(ok, "valid relocation entries within PT_LOAD and symbol bounds must pass");
    }
}

void TestSignedDisplacementArithmetic() {
    {
        const std::uint32_t rawDisplacement = 0xFFFFFFF0U;
        const auto signedDisplacement = static_cast<std::int64_t>(static_cast<std::int32_t>(rawDisplacement));
        Require(signedDisplacement == -16LL, "two's complement 32-bit sign extension must equal -16");

        const std::uint64_t baseAddress = 0x2000ULL;
        const std::uint64_t targetAddress = baseAddress + signedDisplacement;
        Require(targetAddress == 0x1FF0ULL, "base 0x2000 + (-16) must equal 0x1FF0");

        const std::uint64_t incorrectUnsignedTarget = baseAddress + static_cast<std::uint64_t>(rawDisplacement);
        Require(incorrectUnsignedTarget != targetAddress, "unsigned extension would result in high address bug");
    }

    {
        const std::uint16_t raw16 = 0xFFF0U;
        const auto signed16 = static_cast<std::int64_t>(static_cast<std::int16_t>(raw16));
        Require(signed16 == -16LL, "two's complement 16-bit sign extension must equal -16");

        const std::uint64_t baseAddress = 0x3000ULL;
        const std::uint64_t targetAddress = baseAddress + signed16;
        Require(targetAddress == 0x2FF0ULL, "base 0x3000 + (-16) must equal 0x2FF0");
    }

    {
        auto calculateTargetWithBounds = [](const std::uint64_t base, const std::uint32_t rawEncoding) -> std::uint64_t {
            const auto displacement = static_cast<std::int64_t>(static_cast<std::int32_t>(rawEncoding));
            const bool negative = displacement < 0;
            if (negative) {
                const auto magnitude = static_cast<std::uint64_t>(0 - displacement);
                if (base < magnitude) {
                    throw Domain::RelinkerException("Strict filter: unwind pointer address overflowed");
                }
            } else {
                const auto magnitude = static_cast<std::uint64_t>(displacement);
                if (magnitude > std::numeric_limits<std::uint64_t>::max() - base) {
                    throw Domain::RelinkerException("Strict filter: unwind pointer address overflowed");
                }
            }
            return base + displacement;
        };

        Require(calculateTargetWithBounds(0x5000, 0x100) == 0x5100, "positive displacement must evaluate correctly");
        Require(calculateTargetWithBounds(0x5000, 0xFFFFFF00) == 0x4F00, "negative displacement must evaluate correctly");

        bool underflowRejected = false;
        try {
            calculateTargetWithBounds(0x80, 0xFFFFFF00);
        } catch (const Domain::RelinkerException& ex) {
            underflowRejected = std::string(ex.what()).find("unwind pointer address overflowed") != std::string::npos;
        }
        Require(underflowRejected, "displacement underflow below 0 must throw");

        bool overflowRejected = false;
        try {
            calculateTargetWithBounds(std::numeric_limits<std::uint64_t>::max() - 10, 0x100);
        } catch (const Domain::RelinkerException& ex) {
            overflowRejected = std::string(ex.what()).find("unwind pointer address overflowed") != std::string::npos;
        }
        Require(overflowRejected, "displacement overflow past max must throw");
    }

    {
        std::vector<std::uint8_t> bytes(128, 0);
        bytes[0] = 1;
        bytes[1] = 0;
        bytes[2] = 3;
        bytes[3] = 0;
        Io::WriteU64(bytes, 4, 0x1040);
        Io::WriteU32(bytes, 12, 1);
        Io::WriteU64(bytes, 16, 0x2000);
        Io::WriteU64(bytes, 24, 0x1060);
        Io::WriteU32(bytes, 64, 9);
        bytes[72] = 1;
        bytes[73] = 0;
        bytes[74] = 1;
        bytes[75] = 0x78;
        bytes[76] = 16;
        Io::WriteU32(bytes, 96, 20);
        Io::WriteU32(bytes, 100, 36);
        Io::WriteU64(bytes, 104, 0x2000);
        Io::WriteU64(bytes, 112, 0x10);

        Domain::ProgramHeader load{};
        load.Type = Elfpatcher::PT_LOAD;
        load.MappedAddress = 0x1000;
        load.FileSize = 128;
        Domain::ProgramHeader eh = load;
        eh.Type = Elfpatcher::PT_GNU_EH_FRAME;
        eh.FileSize = 32;

        const auto regions = Relinker::UnusedNidFilter::ReadExceptionFunctions(bytes, {load, eh}, {}, {});
        Require(regions.size() == 1, "expected 1 exception function region");
        Require(regions[0].Begin == 0x2000 && regions[0].End == 0x2010, "function region bounds must match");

        auto negativeDispBytes = bytes;
        negativeDispBytes[3] = 0x30;
        Io::WriteU64(negativeDispBytes, 16, 0x800 - 0x1000);
        Io::WriteU64(negativeDispBytes, 24, 0x1060 - 0x1000);
        Io::WriteU64(negativeDispBytes, 104, 0x800);
        Domain::ProgramHeader funcSeg{};
        funcSeg.Type = Elfpatcher::PT_LOAD;
        funcSeg.MappedAddress = 0x800;
        funcSeg.FileSize = 0x20;
        const auto belowRegions = Relinker::UnusedNidFilter::ReadExceptionFunctions(negativeDispBytes, {funcSeg, load, eh}, {}, {});
        Require(belowRegions.size() == 1, "expected 1 below region");
        Require(belowRegions[0].Begin == 0x800 && belowRegions[0].End == 0x810, "below region bounds must match");

        auto overflowBytes = bytes;
        overflowBytes[3] = 0x39;
        overflowBytes[16] = 0x80;
        overflowBytes[17] = 0x20;
        overflowBytes[18] = 0x80;
        overflowBytes[19] = 0x20;
        Domain::ProgramHeader highLoad{};
        highLoad.Type = Elfpatcher::PT_LOAD;
        highLoad.MappedAddress = 0xFFFFFFFFFFFFF000ULL;
        highLoad.FileSize = 128;
        Domain::ProgramHeader highEh = highLoad;
        highEh.Type = Elfpatcher::PT_GNU_EH_FRAME;
        highEh.FileSize = 32;

        bool rejected = false;
        try {
            Relinker::UnusedNidFilter::ReadExceptionFunctions(overflowBytes, {highLoad, highEh}, {}, {});
        } catch (const Domain::RelinkerException& ex) {
            rejected = std::string(ex.what()).find("unwind pointer address overflowed") != std::string::npos;
        }
        Require(rejected, "high address unwind pointer overflow must throw RelinkerException");
    }
}

void TestRelinkerExceptionVerification() {
    const std::string message = "ELF dynamic relocation table out of bounds";
    const Domain::FileByteOffset failureOffset = 0xCAFEBABE;
    const Domain::RelinkerException ex(message, failureOffset);

    Require(std::string(ex.what()) == message, "RelinkerException message must match constructor argument");
    Require(ex.FailureOffset == failureOffset, "RelinkerException failure offset must match constructor argument");

    const std::exception& baseRef = ex;
    Require(std::string(baseRef.what()) == message, "polymorphic catch as std::exception must preserve message");
}

}

int main() {
    try {
        TestElfHeaderBounds();
        TestProgramAndSectionBounds();
        TestDynamicRelocationTableBounds();
        TestRelocationEntryBounds();
        TestSignedDisplacementArithmetic();
        TestRelinkerExceptionVerification();
        std::cout << "All Relinker bounds audit tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "RelinkerBoundsAuditTest failed: " << error.what() << '\n';
        return 1;
    }
}
