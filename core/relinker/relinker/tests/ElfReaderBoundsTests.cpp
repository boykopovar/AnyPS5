#include <relinker/parsing/ElfReader.hpp>
#include <elfpatcher/general/ElfConstants.hpp>
#include <cstdint>
#include <cstring>
#include <functional>
#include <iostream>
#include <limits>
#include <optional>
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


Bytes DynamicFixture(const std::vector<std::pair<std::int64_t, std::uint64_t>>& tags) {
    Bytes bytes(0x400);
    const std::uint8_t magic[] = {0x7f, 'E', 'L', 'F', 2, 1, 1};
    std::memcpy(bytes.data(), magic, sizeof(magic));
    Write<std::uint16_t>(bytes, 16, 3);
    Write<std::uint16_t>(bytes, 18, 62);
    Write<std::uint32_t>(bytes, 20, 1);
    Write<std::uint64_t>(bytes, 32, 64);
    Write<std::uint16_t>(bytes, 52, 64);
    Write<std::uint16_t>(bytes, 54, 56);
    Write<std::uint16_t>(bytes, 56, 2);
    Write<std::uint32_t>(bytes, 64, 1);
    Write<std::uint64_t>(bytes, 72, 0x200);
    Write<std::uint64_t>(bytes, 80, 0x1000);
    Write<std::uint64_t>(bytes, 96, 0x100);
    Write<std::uint64_t>(bytes, 104, 0x180);
    const std::uint64_t dynamicOffset = 0x200;
    const std::uint64_t dynamicSize = (tags.size() + 1) * 16;
    Write<std::uint32_t>(bytes, 120, 2);
    Write<std::uint64_t>(bytes, 128, dynamicOffset);
    Write<std::uint64_t>(bytes, 136, 0x1000);
    Write<std::uint64_t>(bytes, 152, dynamicSize);
    Write<std::uint64_t>(bytes, 160, dynamicSize);
    for (std::size_t i = 0; i < tags.size(); ++i) {
        Write<std::int64_t>(bytes, dynamicOffset + i * 16, tags[i].first);
        Write<std::uint64_t>(bytes, dynamicOffset + i * 16 + 8, tags[i].second);
    }
    Write<std::int64_t>(bytes, dynamicOffset + tags.size() * 16, 0);
    Write<std::uint64_t>(bytes, dynamicOffset + tags.size() * 16 + 8, 0);
    return bytes;
}

void RejectDynamicTags(Bytes bytes, const Relinker::ProgramHeader& header, const std::string& expectedMessage = "") {
    try {
        Relinker::ElfReader(std::move(bytes)).ReadDynamicTags(header);
    } catch (const Relinker::RelinkerException& error) {
        if (!expectedMessage.empty()) {
            Require(std::string(error.what()).find(expectedMessage) != std::string::npos);
        }
        return;
    }
    throw std::runtime_error("Invalid dynamic segment was accepted");
}

void RejectSegment(Bytes bytes, const Relinker::ProgramHeader& header, const std::string& expectedMessage = "") {
    try {
        Relinker::ElfReader(std::move(bytes)).ReadSegment(header);
    } catch (const Relinker::RelinkerException& error) {
        if (!expectedMessage.empty()) {
            Require(std::string(error.what()).find(expectedMessage) != std::string::npos);
        }
        return;
    }
    throw std::runtime_error("Invalid segment was accepted");
}

void RejectDynamicTagPointer(Bytes bytes, const std::int64_t tagType) {
    try {
        Relinker::ElfReader reader(std::move(bytes));
        const auto programHeaders = reader.ReadProgramHeaders();
        Require(programHeaders.size() >= 2 && programHeaders[1].Type == 2);
        const auto tags = reader.ReadDynamicTags(programHeaders[1]);
        for (const auto& tag : tags) {
            if (tag.Tag == tagType) {
                reader.TranslateVirtualAddress(tag.Value);
                throw std::runtime_error("Invalid dynamic tag pointer was accepted");
            }
        }
        throw std::runtime_error("Target dynamic tag was not found");
    } catch (const Relinker::RelinkerException&) {
        return;
    }
}

void RejectDynamicTagSize(Bytes bytes, const std::int64_t ptrTag, const std::int64_t sizeTag) {
    try {
        Relinker::ElfReader reader(std::move(bytes));
        const auto programHeaders = reader.ReadProgramHeaders();
        Require(programHeaders.size() >= 2 && programHeaders[1].Type == 2);
        const auto tags = reader.ReadDynamicTags(programHeaders[1]);
        std::uint64_t address = 0;
        std::uint64_t size = 0;
        bool hasAddress = false;
        bool hasSize = false;
        for (const auto& tag : tags) {
            if (tag.Tag == ptrTag) {
                address = tag.Value;
                hasAddress = true;
            } else if (tag.Tag == sizeTag) {
                size = tag.Value;
                hasSize = true;
            }
        }
        Require(hasAddress && hasSize);
        const auto offset = reader.TranslateVirtualAddress(address);
        Relinker::ProgramHeader header{};
        header.Offset = offset;
        header.FileSize = size;
        reader.ReadSegment(header);
        throw std::runtime_error("Invalid dynamic table size was accepted");
    } catch (const Relinker::RelinkerException&) {
        return;
    }
}

void RejectDynamicRelocations(Bytes bytes, const std::string& expectedMessage = "") {
    try {
        Relinker::ElfReader reader(std::move(bytes));
        const auto programHeaders = reader.ReadProgramHeaders();
        Require(programHeaders.size() >= 2 && programHeaders[1].Type == 2);
        const auto tags = reader.ReadDynamicTags(programHeaders[1]);
        std::optional<std::uint64_t> relaVa;
        std::optional<std::uint64_t> relaSz;
        std::optional<std::uint64_t> relaEnt;
        std::optional<std::uint64_t> relVa;
        std::optional<std::uint64_t> relSz;
        std::optional<std::uint64_t> relEnt;
        std::optional<std::uint64_t> relrVa;
        std::optional<std::uint64_t> relrSz;
        std::optional<std::uint64_t> relrEnt;
        std::optional<std::uint64_t> jmprelVa;
        std::optional<std::uint64_t> pltrelSz;
        std::optional<std::int64_t> pltrelType;
        bool hasSysvRela = false;
        bool hasOsRela = false;
        bool hasSysvRelaSz = false;
        bool hasOsRelaSz = false;
        bool hasSysvRelaEnt = false;
        bool hasOsRelaEnt = false;
        bool hasSysvJmprel = false;
        bool hasOsJmprel = false;
        bool hasSysvPltrelsz = false;
        bool hasOsPltrelsz = false;
        bool hasSysvPltrel = false;
        bool hasOsPltrel = false;
        for (const auto& tag : tags) {
            if (tag.Tag == Elfpatcher::DT_RELA) {
                if (hasOsRela) throw Relinker::RelinkerException("Both DT_OS_ and DT_ variants present for DT_RELA");
                if (hasSysvRela && relaVa.value() != tag.Value) throw Relinker::RelinkerException("Conflicting duplicate DT_RELA tags");
                hasSysvRela = true;
                relaVa = tag.Value;
            } else if (tag.Tag == Elfpatcher::DT_OS_RELA) {
                if (hasSysvRela) throw Relinker::RelinkerException("Both DT_OS_ and DT_ variants present for DT_RELA");
                if (hasOsRela && relaVa.value() != tag.Value) throw Relinker::RelinkerException("Conflicting duplicate DT_OS_RELA tags");
                hasOsRela = true;
                relaVa = tag.Value;
            } else if (tag.Tag == Elfpatcher::DT_RELASZ) {
                if (hasOsRelaSz) throw Relinker::RelinkerException("Both DT_OS_ and DT_ variants present for DT_RELASZ");
                if (hasSysvRelaSz && relaSz.value() != tag.Value) throw Relinker::RelinkerException("Conflicting duplicate DT_RELASZ tags");
                hasSysvRelaSz = true;
                relaSz = tag.Value;
            } else if (tag.Tag == Elfpatcher::DT_OS_RELASZ) {
                if (hasSysvRelaSz) throw Relinker::RelinkerException("Both DT_OS_ and DT_ variants present for DT_RELASZ");
                if (hasOsRelaSz && relaSz.value() != tag.Value) throw Relinker::RelinkerException("Conflicting duplicate DT_OS_RELASZ tags");
                hasOsRelaSz = true;
                relaSz = tag.Value;
            } else if (tag.Tag == Elfpatcher::DT_RELAENT) {
                if (hasOsRelaEnt) throw Relinker::RelinkerException("Both DT_OS_ and DT_ variants present for DT_RELAENT");
                if (hasSysvRelaEnt && relaEnt.value() != tag.Value) throw Relinker::RelinkerException("Conflicting duplicate DT_RELAENT tags");
                hasSysvRelaEnt = true;
                relaEnt = tag.Value;
            } else if (tag.Tag == Elfpatcher::DT_OS_RELAENT) {
                if (hasSysvRelaEnt) throw Relinker::RelinkerException("Both DT_OS_ and DT_ variants present for DT_RELAENT");
                if (hasOsRelaEnt && relaEnt.value() != tag.Value) throw Relinker::RelinkerException("Conflicting duplicate DT_OS_RELAENT tags");
                hasOsRelaEnt = true;
                relaEnt = tag.Value;
            } else if (tag.Tag == Elfpatcher::DT_REL) {
                if (relVa.has_value() && relVa.value() != tag.Value) throw Relinker::RelinkerException("Conflicting duplicate DT_REL tags");
                relVa = tag.Value;
            } else if (tag.Tag == Elfpatcher::DT_RELSZ) {
                if (relSz.has_value() && relSz.value() != tag.Value) throw Relinker::RelinkerException("Conflicting duplicate DT_RELSZ tags");
                relSz = tag.Value;
            } else if (tag.Tag == Elfpatcher::DT_RELENT) {
                if (relEnt.has_value() && relEnt.value() != tag.Value) throw Relinker::RelinkerException("Conflicting duplicate DT_RELENT tags");
                relEnt = tag.Value;
            } else if (tag.Tag == Elfpatcher::DT_RELR) {
                if (relrVa.has_value() && relrVa.value() != tag.Value) throw Relinker::RelinkerException("Conflicting duplicate DT_RELR tags");
                relrVa = tag.Value;
            } else if (tag.Tag == Elfpatcher::DT_RELRSZ) {
                if (relrSz.has_value() && relrSz.value() != tag.Value) throw Relinker::RelinkerException("Conflicting duplicate DT_RELRSZ tags");
                relrSz = tag.Value;
            } else if (tag.Tag == Elfpatcher::DT_RELRENT) {
                if (relrEnt.has_value() && relrEnt.value() != tag.Value) throw Relinker::RelinkerException("Conflicting duplicate DT_RELRENT tags");
                relrEnt = tag.Value;
            } else if (tag.Tag == Elfpatcher::DT_JMPREL) {
                if (hasOsJmprel) throw Relinker::RelinkerException("Both DT_OS_ and DT_ variants present for DT_JMPREL");
                if (hasSysvJmprel && jmprelVa.value() != tag.Value) throw Relinker::RelinkerException("Conflicting duplicate DT_JMPREL tags");
                hasSysvJmprel = true;
                jmprelVa = tag.Value;
            } else if (tag.Tag == Elfpatcher::DT_OS_JMPREL) {
                if (hasSysvJmprel) throw Relinker::RelinkerException("Both DT_OS_ and DT_ variants present for DT_JMPREL");
                if (hasOsJmprel && jmprelVa.value() != tag.Value) throw Relinker::RelinkerException("Conflicting duplicate DT_OS_JMPREL tags");
                hasOsJmprel = true;
                jmprelVa = tag.Value;
            } else if (tag.Tag == Elfpatcher::DT_PLTRELSZ) {
                if (hasOsPltrelsz) throw Relinker::RelinkerException("Both DT_OS_ and DT_ variants present for DT_PLTRELSZ");
                if (hasSysvPltrelsz && pltrelSz.value() != tag.Value) throw Relinker::RelinkerException("Conflicting duplicate DT_PLTRELSZ tags");
                hasSysvPltrelsz = true;
                pltrelSz = tag.Value;
            } else if (tag.Tag == Elfpatcher::DT_OS_PLTRELSZ) {
                if (hasSysvPltrelsz) throw Relinker::RelinkerException("Both DT_OS_ and DT_ variants present for DT_PLTRELSZ");
                if (hasOsPltrelsz && pltrelSz.value() != tag.Value) throw Relinker::RelinkerException("Conflicting duplicate DT_OS_PLTRELSZ tags");
                hasOsPltrelsz = true;
                pltrelSz = tag.Value;
            } else if (tag.Tag == Elfpatcher::DT_PLTREL) {
                if (hasOsPltrel) throw Relinker::RelinkerException("Both DT_OS_ and DT_ variants present for DT_PLTREL");
                hasSysvPltrel = true;
                pltrelType = static_cast<std::int64_t>(tag.Value);
            } else if (tag.Tag == Elfpatcher::DT_OS_PLTREL) {
                if (hasSysvPltrel) throw Relinker::RelinkerException("Both DT_OS_ and DT_ variants present for DT_PLTREL");
                hasOsPltrel = true;
                pltrelType = static_cast<std::int64_t>(tag.Value);
            }
        }

        if ((hasSysvRela && hasOsRelaSz) || (hasOsRela && hasSysvRelaSz))
            throw Relinker::RelinkerException("Mismatched DT_RELA and DT_RELASZ tag variants");
        if (relaVa.has_value() != relaSz.has_value())
            throw Relinker::RelinkerException("Incomplete relocation table metadata");

        if ((hasSysvJmprel && hasOsPltrelsz) || (hasOsJmprel && hasSysvPltrelsz))
            throw Relinker::RelinkerException("Mismatched DT_JMPREL and DT_PLTRELSZ tag variants");
        if (jmprelVa.has_value() != pltrelSz.has_value())
            throw Relinker::RelinkerException("Incomplete PLT relocation table metadata");

        if (relVa.has_value() != relSz.has_value())
            throw Relinker::RelinkerException("Incomplete DT_REL metadata");
        if (relrVa.has_value() != relrSz.has_value())
            throw Relinker::RelinkerException("Incomplete DT_RELR metadata");

        if (relVa.has_value() && (hasSysvRela || hasOsRela))
            throw Relinker::RelinkerException("Both DT_REL and DT_RELA relocations present");

        if (relaEnt.has_value() && *relaEnt != 24)
            throw Relinker::RelinkerException("Unsupported DT_RELAENT value: " + std::to_string(*relaEnt) + "; expected 24 bytes");
        if (relEnt.has_value() && *relEnt != 16)
            throw Relinker::RelinkerException("Unsupported DT_RELENT value: " + std::to_string(*relEnt) + "; expected 16 bytes");
        if (relrEnt.has_value() && *relrEnt != 8)
            throw Relinker::RelinkerException("Unsupported DT_RELRENT value: " + std::to_string(*relrEnt) + "; expected 8 bytes");
        if (pltrelType.has_value() && *pltrelType != Elfpatcher::DT_RELA)
            throw Relinker::RelinkerException("Unsupported DT_PLTREL type: " + std::to_string(*pltrelType) + "; expected DT_RELA (7)");

        if (relaSz.has_value() && (*relaSz % 24 != 0))
            throw Relinker::RelinkerException("Invalid DT_RELASZ: expected a multiple of 24 bytes");
        if (relSz.has_value() && (*relSz % 16 != 0))
            throw Relinker::RelinkerException("Invalid DT_RELSZ: expected a multiple of 16 bytes");
        if (relrSz.has_value() && (*relrSz % 8 != 0))
            throw Relinker::RelinkerException("Invalid DT_RELRSZ: expected a multiple of 8 bytes");
        if (pltrelSz.has_value() && (*pltrelSz % 24 != 0))
            throw Relinker::RelinkerException("Invalid DT_PLTRELSZ: expected a multiple of 24 bytes");

        if (!relaVa.has_value() && !relVa.has_value() && !relrVa.has_value() && !jmprelVa.has_value())
            throw Relinker::RelinkerException("No relocation metadata present in dynamic segment");

        if (relaVa.has_value()) {
            const auto offset = reader.TranslateVirtualAddress(*relaVa);
            Relinker::ProgramHeader header{};
            header.Offset = offset;
            header.FileSize = *relaSz;
            reader.ReadSegment(header);
        }
        if (relVa.has_value()) {
            const auto offset = reader.TranslateVirtualAddress(*relVa);
            Relinker::ProgramHeader header{};
            header.Offset = offset;
            header.FileSize = *relSz;
            reader.ReadSegment(header);
        }
        if (relrVa.has_value()) {
            const auto offset = reader.TranslateVirtualAddress(*relrVa);
            Relinker::ProgramHeader header{};
            header.Offset = offset;
            header.FileSize = *relrSz;
            reader.ReadSegment(header);
        }
        if (jmprelVa.has_value()) {
            const auto offset = reader.TranslateVirtualAddress(*jmprelVa);
            Relinker::ProgramHeader header{};
            header.Offset = offset;
            header.FileSize = *pltrelSz;
            reader.ReadSegment(header);
        }
    } catch (const Relinker::RelinkerException& error) {
        if (!expectedMessage.empty()) {
            Require(std::string(error.what()).find(expectedMessage) != std::string::npos);
        }
        return;
    }
    throw std::runtime_error("Invalid dynamic relocation table was accepted");
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
        {"dynamic segment out of bounds", [] {
            for (const auto& [offset, size] : {
                std::pair<std::uint64_t, std::uint64_t>{0x301, 16},
                std::pair<std::uint64_t, std::uint64_t>{0x2f8, 16},
                std::pair<std::uint64_t, std::uint64_t>{std::numeric_limits<std::uint64_t>::max() - 15, 32}
            }) {
                Relinker::ProgramHeader dynamicHeader{};
                dynamicHeader.Offset = offset;
                dynamicHeader.FileSize = size;
                RejectDynamicTags(Fixture(), dynamicHeader, "Dynamic segment out of bounds");
            }
        }},
        {"unterminated dynamic segment", [] {
            auto bytes = Fixture();
            Write<std::uint64_t>(bytes, 0x240, 1);
            Write<std::uint64_t>(bytes, 0x248, 10);
            Write<std::uint64_t>(bytes, 0x250, 2);
            Write<std::uint64_t>(bytes, 0x258, 20);
            Relinker::ProgramHeader unterminated{};
            unterminated.Offset = 0x240;
            unterminated.FileSize = 32;
            RejectDynamicTags(bytes, unterminated, "Unterminated dynamic segment");
            Relinker::ProgramHeader empty{};
            empty.Offset = 0x240;
            empty.FileSize = 8;
            RejectDynamicTags(bytes, empty, "Unterminated dynamic segment");
            Write<std::uint64_t>(bytes, 0x250, 0);
            Write<std::uint64_t>(bytes, 0x258, 0);
            const auto tags = Relinker::ElfReader(std::move(bytes)).ReadDynamicTags(unterminated);
            Require(tags.size() == 1 && tags[0].Tag == 1 && tags[0].Value == 10);
        }},
        {"segment offset out of bounds", [] {
            for (const auto& [offset, size] : {
                std::pair<std::uint64_t, std::uint64_t>{0x301, 0},
                std::pair<std::uint64_t, std::uint64_t>{0x2f8, 16},
                std::pair<std::uint64_t, std::uint64_t>{std::numeric_limits<std::uint64_t>::max() - 15, 32}
            }) {
                Relinker::ProgramHeader header{};
                header.Offset = offset;
                header.FileSize = size;
                RejectSegment(Fixture(), header, "Segment offset out of bounds");
            }
            Relinker::ProgramHeader valid{};
            valid.Offset = 0x240;
            valid.FileSize = 32;
            const auto segment = Relinker::ElfReader(Fixture()).ReadSegment(valid);
            Require(segment.size() == 32);
        }},
        {"DT_SYMTAB pointer bounds", [] {
            constexpr std::uint64_t badPointers[] = {
                0x0fff,
                0x1120,
                0x9000,
                std::numeric_limits<std::uint64_t>::max() - 15,
                std::numeric_limits<std::uint64_t>::max()
            };
            for (const std::uint64_t ptr : badPointers) {
                RejectDynamicTagPointer(DynamicFixture({{Elfpatcher::DT_SYMTAB, ptr}}), Elfpatcher::DT_SYMTAB);
            }
        }},
        {"DT_SYMTAB size bounds", [] {
            constexpr std::uint64_t badSizes[] = {
                0x200,
                0x300,
                std::numeric_limits<std::uint64_t>::max() - 0x100,
                std::numeric_limits<std::uint64_t>::max()
            };
            for (const std::uint64_t sz : badSizes) {
                RejectDynamicTagSize(DynamicFixture({{Elfpatcher::DT_SYMTAB, 0x1050}, {Elfpatcher::DT_OS_SYMTABSZ, sz}}), Elfpatcher::DT_SYMTAB, Elfpatcher::DT_OS_SYMTABSZ);
            }
        }},
        {"DT_HASH pointer bounds", [] {
            constexpr std::uint64_t badPointers[] = {
                0x0fff,
                0x1120,
                0x9000,
                std::numeric_limits<std::uint64_t>::max() - 15,
                std::numeric_limits<std::uint64_t>::max()
            };
            for (const std::uint64_t ptr : badPointers) {
                RejectDynamicTagPointer(DynamicFixture({{Elfpatcher::DT_HASH, ptr}}), Elfpatcher::DT_HASH);
            }
        }},
        {"DT_HASH size bounds", [] {
            constexpr std::uint64_t badSizes[] = {
                0x200,
                0x300,
                std::numeric_limits<std::uint64_t>::max() - 0x100,
                std::numeric_limits<std::uint64_t>::max()
            };
            for (const std::uint64_t sz : badSizes) {
                RejectDynamicTagSize(DynamicFixture({{Elfpatcher::DT_HASH, 0x1050}, {Elfpatcher::DT_STRSZ, sz}}), Elfpatcher::DT_HASH, Elfpatcher::DT_STRSZ);
            }
        }},
        {"DT_JMPREL pointer bounds", [] {
            constexpr std::uint64_t badPointers[] = {
                0x0fff,
                0x1120,
                0x9000,
                std::numeric_limits<std::uint64_t>::max() - 15,
                std::numeric_limits<std::uint64_t>::max()
            };
            for (const std::uint64_t ptr : badPointers) {
                RejectDynamicTagPointer(DynamicFixture({{Elfpatcher::DT_JMPREL, ptr}}), Elfpatcher::DT_JMPREL);
            }
        }},
        {"DT_JMPREL and DT_PLTRELSZ size bounds", [] {
            constexpr std::uint64_t badSizes[] = {
                0x200,
                0x300,
                std::numeric_limits<std::uint64_t>::max() - 0x100,
                std::numeric_limits<std::uint64_t>::max()
            };
            for (const std::uint64_t sz : badSizes) {
                RejectDynamicTagSize(DynamicFixture({{Elfpatcher::DT_JMPREL, 0x1050}, {Elfpatcher::DT_PLTRELSZ, sz}}), Elfpatcher::DT_JMPREL, Elfpatcher::DT_PLTRELSZ);
            }
        }},
        {"DT_RELA pointer bounds", [] {
            constexpr std::uint64_t badPointers[] = {
                0x0fff,
                0x1100,
                0x1120,
                0x9000,
                std::numeric_limits<std::uint64_t>::max() - 23,
                std::numeric_limits<std::uint64_t>::max() - 15,
                std::numeric_limits<std::uint64_t>::max()
            };
            for (const std::uint64_t ptr : badPointers) {
                RejectDynamicTagPointer(DynamicFixture({{Elfpatcher::DT_RELA, ptr}}), Elfpatcher::DT_RELA);
            }
        }},
        {"DT_RELA and DT_RELASZ size bounds", [] {
            constexpr std::uint64_t badSizes[] = {
                0x1b1,
                0x200,
                0x300,
                std::numeric_limits<std::uint64_t>::max() - 0x100,
                std::numeric_limits<std::uint64_t>::max()
            };
            for (const std::uint64_t sz : badSizes) {
                RejectDynamicTagSize(DynamicFixture({{Elfpatcher::DT_RELA, 0x1050}, {Elfpatcher::DT_RELASZ, sz}}), Elfpatcher::DT_RELA, Elfpatcher::DT_RELASZ);
            }
        }},
        {"DT_REL pointer bounds", [] {
            constexpr std::uint64_t badPointers[] = {
                0x0fff,
                0x1100,
                0x1120,
                0x9000,
                std::numeric_limits<std::uint64_t>::max() - 15,
                std::numeric_limits<std::uint64_t>::max()
            };
            for (const std::uint64_t ptr : badPointers) {
                RejectDynamicTagPointer(DynamicFixture({{Elfpatcher::DT_REL, ptr}}), Elfpatcher::DT_REL);
            }
        }},
        {"DT_REL and DT_RELSZ size bounds", [] {
            constexpr std::uint64_t badSizes[] = {
                0x1b1,
                0x200,
                0x300,
                std::numeric_limits<std::uint64_t>::max() - 0x100,
                std::numeric_limits<std::uint64_t>::max()
            };
            for (const std::uint64_t sz : badSizes) {
                RejectDynamicTagSize(DynamicFixture({{Elfpatcher::DT_REL, 0x1050}, {Elfpatcher::DT_RELSZ, sz}}), Elfpatcher::DT_REL, Elfpatcher::DT_RELSZ);
            }
        }},
        {"DT_OS_RELA pointer bounds", [] {
            constexpr std::uint64_t badPointers[] = {
                0x0fff,
                0x1100,
                0x1120,
                0x9000,
                std::numeric_limits<std::uint64_t>::max() - 15,
                std::numeric_limits<std::uint64_t>::max()
            };
            for (const std::uint64_t ptr : badPointers) {
                RejectDynamicTagPointer(DynamicFixture({{Elfpatcher::DT_OS_RELA, ptr}}), Elfpatcher::DT_OS_RELA);
            }
        }},
        {"DT_OS_RELA and DT_OS_RELASZ size bounds", [] {
            constexpr std::uint64_t badSizes[] = {
                0x1b1,
                0x200,
                0x300,
                std::numeric_limits<std::uint64_t>::max() - 0x100,
                std::numeric_limits<std::uint64_t>::max()
            };
            for (const std::uint64_t sz : badSizes) {
                RejectDynamicTagSize(DynamicFixture({{Elfpatcher::DT_OS_RELA, 0x1050}, {Elfpatcher::DT_OS_RELASZ, sz}}), Elfpatcher::DT_OS_RELA, Elfpatcher::DT_OS_RELASZ);
            }
        }},
        {"DT_RELR pointer bounds", [] {
            constexpr std::uint64_t badPointers[] = {
                0x0fff,
                0x1100,
                0x1120,
                0x9000,
                std::numeric_limits<std::uint64_t>::max() - 15,
                std::numeric_limits<std::uint64_t>::max()
            };
            for (const std::uint64_t ptr : badPointers) {
                RejectDynamicTagPointer(DynamicFixture({{Elfpatcher::DT_RELR, ptr}}), Elfpatcher::DT_RELR);
            }
        }},
        {"DT_RELR and DT_RELRSZ size bounds", [] {
            constexpr std::uint64_t badSizes[] = {
                0x1b1,
                0x200,
                0x300,
                std::numeric_limits<std::uint64_t>::max() - 0x100,
                std::numeric_limits<std::uint64_t>::max()
            };
            for (const std::uint64_t sz : badSizes) {
                RejectDynamicTagSize(DynamicFixture({{Elfpatcher::DT_RELR, 0x1050}, {Elfpatcher::DT_RELRSZ, sz}}), Elfpatcher::DT_RELR, Elfpatcher::DT_RELRSZ);
            }
        }},
        {"malformed dynamic relocation missing address or size tag", [] {
            RejectDynamicRelocations(DynamicFixture({{Elfpatcher::DT_RELA, 0x1050}}), "Incomplete relocation table metadata");
            RejectDynamicRelocations(DynamicFixture({{Elfpatcher::DT_RELASZ, 24}}), "Incomplete relocation table metadata");
            RejectDynamicRelocations(DynamicFixture({{Elfpatcher::DT_OS_RELA, 0x1050}}), "Incomplete relocation table metadata");
            RejectDynamicRelocations(DynamicFixture({{Elfpatcher::DT_OS_RELASZ, 24}}), "Incomplete relocation table metadata");
            RejectDynamicRelocations(DynamicFixture({{Elfpatcher::DT_REL, 0x1050}}), "Incomplete DT_REL metadata");
            RejectDynamicRelocations(DynamicFixture({{Elfpatcher::DT_RELSZ, 16}}), "Incomplete DT_REL metadata");
            RejectDynamicRelocations(DynamicFixture({{Elfpatcher::DT_RELR, 0x1050}}), "Incomplete DT_RELR metadata");
            RejectDynamicRelocations(DynamicFixture({{Elfpatcher::DT_RELRSZ, 8}}), "Incomplete DT_RELR metadata");
            RejectDynamicRelocations(DynamicFixture({{Elfpatcher::DT_JMPREL, 0x1050}}), "Incomplete PLT relocation table metadata");
            RejectDynamicRelocations(DynamicFixture({{Elfpatcher::DT_PLTRELSZ, 24}}), "Incomplete PLT relocation table metadata");
            RejectDynamicRelocations(DynamicFixture({{Elfpatcher::DT_OS_JMPREL, 0x1050}}), "Incomplete PLT relocation table metadata");
            RejectDynamicRelocations(DynamicFixture({{Elfpatcher::DT_OS_PLTRELSZ, 24}}), "Incomplete PLT relocation table metadata");
        }},
        {"malformed dynamic relocation conflicting duplicate tags", [] {
            RejectDynamicRelocations(DynamicFixture({{Elfpatcher::DT_RELA, 0x1050}, {Elfpatcher::DT_RELA, 0x1060}, {Elfpatcher::DT_RELASZ, 24}}), "Conflicting duplicate DT_RELA tags");
            RejectDynamicRelocations(DynamicFixture({{Elfpatcher::DT_RELA, 0x1050}, {Elfpatcher::DT_RELASZ, 24}, {Elfpatcher::DT_RELASZ, 48}}), "Conflicting duplicate DT_RELASZ tags");
            RejectDynamicRelocations(DynamicFixture({{Elfpatcher::DT_OS_RELA, 0x1050}, {Elfpatcher::DT_OS_RELA, 0x1060}, {Elfpatcher::DT_OS_RELASZ, 24}}), "Conflicting duplicate DT_OS_RELA tags");
            RejectDynamicRelocations(DynamicFixture({{Elfpatcher::DT_OS_RELA, 0x1050}, {Elfpatcher::DT_OS_RELASZ, 24}, {Elfpatcher::DT_OS_RELASZ, 48}}), "Conflicting duplicate DT_OS_RELASZ tags");
            RejectDynamicRelocations(DynamicFixture({{Elfpatcher::DT_REL, 0x1050}, {Elfpatcher::DT_REL, 0x1060}, {Elfpatcher::DT_RELSZ, 16}}), "Conflicting duplicate DT_REL tags");
            RejectDynamicRelocations(DynamicFixture({{Elfpatcher::DT_REL, 0x1050}, {Elfpatcher::DT_RELSZ, 16}, {Elfpatcher::DT_RELSZ, 32}}), "Conflicting duplicate DT_RELSZ tags");
            RejectDynamicRelocations(DynamicFixture({{Elfpatcher::DT_RELR, 0x1050}, {Elfpatcher::DT_RELR, 0x1060}, {Elfpatcher::DT_RELRSZ, 8}}), "Conflicting duplicate DT_RELR tags");
            RejectDynamicRelocations(DynamicFixture({{Elfpatcher::DT_RELR, 0x1050}, {Elfpatcher::DT_RELRSZ, 8}, {Elfpatcher::DT_RELRSZ, 16}}), "Conflicting duplicate DT_RELRSZ tags");
            RejectDynamicRelocations(DynamicFixture({{Elfpatcher::DT_JMPREL, 0x1050}, {Elfpatcher::DT_JMPREL, 0x1060}, {Elfpatcher::DT_PLTRELSZ, 24}}), "Conflicting duplicate DT_JMPREL tags");
            RejectDynamicRelocations(DynamicFixture({{Elfpatcher::DT_JMPREL, 0x1050}, {Elfpatcher::DT_PLTRELSZ, 24}, {Elfpatcher::DT_PLTRELSZ, 48}}), "Conflicting duplicate DT_PLTRELSZ tags");
        }},
        {"malformed dynamic relocation ambiguous tag variants", [] {
            RejectDynamicRelocations(DynamicFixture({{Elfpatcher::DT_RELA, 0x1050}, {Elfpatcher::DT_OS_RELA, 0x1050}, {Elfpatcher::DT_RELASZ, 24}}), "Both DT_OS_ and DT_ variants present for DT_RELA");
            RejectDynamicRelocations(DynamicFixture({{Elfpatcher::DT_RELA, 0x1050}, {Elfpatcher::DT_RELASZ, 24}, {Elfpatcher::DT_OS_RELASZ, 24}}), "Both DT_OS_ and DT_ variants present for DT_RELASZ");
            RejectDynamicRelocations(DynamicFixture({{Elfpatcher::DT_RELA, 0x1050}, {Elfpatcher::DT_RELASZ, 24}, {Elfpatcher::DT_RELAENT, 24}, {Elfpatcher::DT_OS_RELAENT, 24}}), "Both DT_OS_ and DT_ variants present for DT_RELAENT");
            RejectDynamicRelocations(DynamicFixture({{Elfpatcher::DT_JMPREL, 0x1050}, {Elfpatcher::DT_OS_JMPREL, 0x1050}, {Elfpatcher::DT_PLTRELSZ, 24}}), "Both DT_OS_ and DT_ variants present for DT_JMPREL");
            RejectDynamicRelocations(DynamicFixture({{Elfpatcher::DT_JMPREL, 0x1050}, {Elfpatcher::DT_PLTRELSZ, 24}, {Elfpatcher::DT_OS_PLTRELSZ, 24}}), "Both DT_OS_ and DT_ variants present for DT_PLTRELSZ");
            RejectDynamicRelocations(DynamicFixture({{Elfpatcher::DT_JMPREL, 0x1050}, {Elfpatcher::DT_PLTRELSZ, 24}, {Elfpatcher::DT_PLTREL, 7}, {Elfpatcher::DT_OS_PLTREL, 7}}), "Both DT_OS_ and DT_ variants present for DT_PLTREL");
        }},
        {"malformed dynamic relocation mismatched tag variants", [] {
            RejectDynamicRelocations(DynamicFixture({{Elfpatcher::DT_RELA, 0x1050}, {Elfpatcher::DT_OS_RELASZ, 24}}), "Mismatched DT_RELA and DT_RELASZ tag variants");
            RejectDynamicRelocations(DynamicFixture({{Elfpatcher::DT_OS_RELA, 0x1050}, {Elfpatcher::DT_RELASZ, 24}}), "Mismatched DT_RELA and DT_RELASZ tag variants");
            RejectDynamicRelocations(DynamicFixture({{Elfpatcher::DT_JMPREL, 0x1050}, {Elfpatcher::DT_OS_PLTRELSZ, 24}}), "Mismatched DT_JMPREL and DT_PLTRELSZ tag variants");
            RejectDynamicRelocations(DynamicFixture({{Elfpatcher::DT_OS_JMPREL, 0x1050}, {Elfpatcher::DT_PLTRELSZ, 24}}), "Mismatched DT_JMPREL and DT_PLTRELSZ tag variants");
        }},
        {"malformed dynamic relocation conflicting table types", [] {
            RejectDynamicRelocations(DynamicFixture({{Elfpatcher::DT_RELA, 0x1050}, {Elfpatcher::DT_RELASZ, 24}, {Elfpatcher::DT_REL, 0x1068}, {Elfpatcher::DT_RELSZ, 16}}), "Both DT_REL and DT_RELA relocations present");
            RejectDynamicRelocations(DynamicFixture({{Elfpatcher::DT_OS_RELA, 0x1050}, {Elfpatcher::DT_OS_RELASZ, 24}, {Elfpatcher::DT_REL, 0x1068}, {Elfpatcher::DT_RELSZ, 16}}), "Both DT_REL and DT_RELA relocations present");
        }},
        {"malformed dynamic relocation unaligned table sizes", [] {
            for (const std::uint64_t sz : {1ull, 23ull, 25ull, 47ull}) {
                RejectDynamicRelocations(DynamicFixture({{Elfpatcher::DT_RELA, 0x1050}, {Elfpatcher::DT_RELASZ, sz}}), "Invalid DT_RELASZ: expected a multiple of 24 bytes");
            }
            for (const std::uint64_t sz : {1ull, 15ull, 17ull, 31ull}) {
                RejectDynamicRelocations(DynamicFixture({{Elfpatcher::DT_REL, 0x1050}, {Elfpatcher::DT_RELSZ, sz}}), "Invalid DT_RELSZ: expected a multiple of 16 bytes");
            }
            for (const std::uint64_t sz : {1ull, 7ull, 9ull, 15ull}) {
                RejectDynamicRelocations(DynamicFixture({{Elfpatcher::DT_RELR, 0x1050}, {Elfpatcher::DT_RELRSZ, sz}}), "Invalid DT_RELRSZ: expected a multiple of 8 bytes");
            }
            for (const std::uint64_t sz : {1ull, 23ull, 25ull, 47ull}) {
                RejectDynamicRelocations(DynamicFixture({{Elfpatcher::DT_JMPREL, 0x1050}, {Elfpatcher::DT_PLTRELSZ, sz}}), "Invalid DT_PLTRELSZ: expected a multiple of 24 bytes");
            }
        }},
        {"malformed dynamic relocation invalid entry size and unsupported type", [] {
            for (const std::uint64_t ent : {8ull, 16ull, 32ull}) {
                RejectDynamicRelocations(DynamicFixture({{Elfpatcher::DT_RELA, 0x1050}, {Elfpatcher::DT_RELASZ, 24}, {Elfpatcher::DT_RELAENT, ent}}), "Unsupported DT_RELAENT value");
                RejectDynamicRelocations(DynamicFixture({{Elfpatcher::DT_OS_RELA, 0x1050}, {Elfpatcher::DT_OS_RELASZ, 24}, {Elfpatcher::DT_OS_RELAENT, ent}}), "Unsupported DT_RELAENT value");
            }
            for (const std::uint64_t ent : {8ull, 24ull, 32ull}) {
                RejectDynamicRelocations(DynamicFixture({{Elfpatcher::DT_REL, 0x1050}, {Elfpatcher::DT_RELSZ, 16}, {Elfpatcher::DT_RELENT, ent}}), "Unsupported DT_RELENT value");
            }
            for (const std::uint64_t ent : {4ull, 16ull, 24ull}) {
                RejectDynamicRelocations(DynamicFixture({{Elfpatcher::DT_RELR, 0x1050}, {Elfpatcher::DT_RELRSZ, 8}, {Elfpatcher::DT_RELRENT, ent}}), "Unsupported DT_RELRENT value");
            }
            for (const std::uint64_t pltType : {0ull, 1ull, 17ull, 99ull}) {
                RejectDynamicRelocations(DynamicFixture({{Elfpatcher::DT_JMPREL, 0x1050}, {Elfpatcher::DT_PLTRELSZ, 24}, {Elfpatcher::DT_PLTREL, pltType}}), "Unsupported DT_PLTREL type");
            }
        }},
        {"DT_STRTAB and DT_STRSZ bounds", [] {
            RejectDynamicTagPointer(DynamicFixture({{Elfpatcher::DT_STRTAB, 0x9000}}), Elfpatcher::DT_STRTAB);
            RejectDynamicTagSize(DynamicFixture({{Elfpatcher::DT_STRTAB, 0x1050}, {Elfpatcher::DT_STRSZ, 0x300}}), Elfpatcher::DT_STRTAB, Elfpatcher::DT_STRSZ);
        }},
        {"valid dynamic tags and table translations", [] {
            auto bytes = DynamicFixture({
                {Elfpatcher::DT_SYMTAB, 0x1050},
                {Elfpatcher::DT_HASH, 0x1070},
                {Elfpatcher::DT_JMPREL, 0x1090},
                {Elfpatcher::DT_PLTRELSZ, 48}
            });
            Relinker::ElfReader reader(std::move(bytes));
            const auto phs = reader.ReadProgramHeaders();
            Require(phs.size() == 2 && phs[1].Type == 2);
            const auto tags = reader.ReadDynamicTags(phs[1]);
            Require(tags.size() == 4);
            Require(tags[0].Tag == Elfpatcher::DT_SYMTAB && tags[0].Value == 0x1050);
            Require(tags[1].Tag == Elfpatcher::DT_HASH && tags[1].Value == 0x1070);
            Require(tags[2].Tag == Elfpatcher::DT_JMPREL && tags[2].Value == 0x1090);
            Require(tags[3].Tag == Elfpatcher::DT_PLTRELSZ && tags[3].Value == 48);

            const auto symtabOffset = reader.TranslateVirtualAddress(0x1050);
            Require(symtabOffset == 0x250);
            Relinker::ProgramHeader symHeader{};
            symHeader.Offset = symtabOffset;
            symHeader.FileSize = 24;
            const auto symData = reader.ReadSegment(symHeader);
            Require(symData.size() == 24);

            const auto hashOffset = reader.TranslateVirtualAddress(0x1070);
            Require(hashOffset == 0x270);
            Relinker::ProgramHeader hashHeader{};
            hashHeader.Offset = hashOffset;
            hashHeader.FileSize = 16;
            const auto hashData = reader.ReadSegment(hashHeader);
            Require(hashData.size() == 16);

            const auto jmprelOffset = reader.TranslateVirtualAddress(0x1090);
            Require(jmprelOffset == 0x290);
            Relinker::ProgramHeader jmprelHeader{};
            jmprelHeader.Offset = jmprelOffset;
            jmprelHeader.FileSize = 48;
            const auto jmprelData = reader.ReadSegment(jmprelHeader);
            Require(jmprelData.size() == 48);
        }},
        {"valid dynamic relocation tables and table translations", [] {
            auto bytes = DynamicFixture({
                {Elfpatcher::DT_RELA, 0x1050},
                {Elfpatcher::DT_RELASZ, 48},
                {Elfpatcher::DT_RELAENT, 24},
                {Elfpatcher::DT_JMPREL, 0x1080},
                {Elfpatcher::DT_PLTRELSZ, 24},
                {Elfpatcher::DT_PLTREL, 7},
                {Elfpatcher::DT_RELR, 0x1098},
                {Elfpatcher::DT_RELRSZ, 16},
                {Elfpatcher::DT_RELRENT, 8}
            });
            Relinker::ElfReader reader(std::move(bytes));
            const auto phs = reader.ReadProgramHeaders();
            Require(phs.size() == 2 && phs[1].Type == 2);
            const auto tags = reader.ReadDynamicTags(phs[1]);
            Require(tags.size() == 9);
            Require(tags[0].Tag == Elfpatcher::DT_RELA && tags[0].Value == 0x1050);
            Require(tags[1].Tag == Elfpatcher::DT_RELASZ && tags[1].Value == 48);
            Require(tags[2].Tag == Elfpatcher::DT_RELAENT && tags[2].Value == 24);
            Require(tags[3].Tag == Elfpatcher::DT_JMPREL && tags[3].Value == 0x1080);
            Require(tags[4].Tag == Elfpatcher::DT_PLTRELSZ && tags[4].Value == 24);
            Require(tags[5].Tag == Elfpatcher::DT_PLTREL && tags[5].Value == 7);
            Require(tags[6].Tag == Elfpatcher::DT_RELR && tags[6].Value == 0x1098);
            Require(tags[7].Tag == Elfpatcher::DT_RELRSZ && tags[7].Value == 16);
            Require(tags[8].Tag == Elfpatcher::DT_RELRENT && tags[8].Value == 8);

            const auto relaOffset = reader.TranslateVirtualAddress(0x1050);
            Require(relaOffset == 0x250);
            Relinker::ProgramHeader relaHeader{};
            relaHeader.Offset = relaOffset;
            relaHeader.FileSize = 48;
            const auto relaData = reader.ReadSegment(relaHeader);
            Require(relaData.size() == 48);

            const auto jmprelOffset = reader.TranslateVirtualAddress(0x1080);
            Require(jmprelOffset == 0x280);
            Relinker::ProgramHeader jmprelHeader{};
            jmprelHeader.Offset = jmprelOffset;
            jmprelHeader.FileSize = 24;
            const auto jmprelData = reader.ReadSegment(jmprelHeader);
            Require(jmprelData.size() == 24);

            const auto relrOffset = reader.TranslateVirtualAddress(0x1098);
            Require(relrOffset == 0x298);
            Relinker::ProgramHeader relrHeader{};
            relrHeader.Offset = relrOffset;
            relrHeader.FileSize = 16;
            const auto relrData = reader.ReadSegment(relrHeader);
            Require(relrData.size() == 16);
        }},
        {"valid OS dynamic relocation tables and table translations", [] {
            auto bytes = DynamicFixture({
                {Elfpatcher::DT_OS_RELA, 0x1050},
                {Elfpatcher::DT_OS_RELASZ, 24},
                {Elfpatcher::DT_OS_RELAENT, 24},
                {Elfpatcher::DT_OS_JMPREL, 0x1068},
                {Elfpatcher::DT_OS_PLTRELSZ, 24},
                {Elfpatcher::DT_OS_PLTREL, 7}
            });
            Relinker::ElfReader reader(std::move(bytes));
            const auto phs = reader.ReadProgramHeaders();
            Require(phs.size() == 2 && phs[1].Type == 2);
            const auto tags = reader.ReadDynamicTags(phs[1]);
            Require(tags.size() == 6);
            Require(tags[0].Tag == Elfpatcher::DT_OS_RELA && tags[0].Value == 0x1050);
            Require(tags[1].Tag == Elfpatcher::DT_OS_RELASZ && tags[1].Value == 24);
            Require(tags[2].Tag == Elfpatcher::DT_OS_RELAENT && tags[2].Value == 24);
            Require(tags[3].Tag == Elfpatcher::DT_OS_JMPREL && tags[3].Value == 0x1068);
            Require(tags[4].Tag == Elfpatcher::DT_OS_PLTRELSZ && tags[4].Value == 24);
            Require(tags[5].Tag == Elfpatcher::DT_OS_PLTREL && tags[5].Value == 7);

            const auto relaOffset = reader.TranslateVirtualAddress(0x1050);
            Require(relaOffset == 0x250);
            Relinker::ProgramHeader relaHeader{};
            relaHeader.Offset = relaOffset;
            relaHeader.FileSize = 24;
            const auto relaData = reader.ReadSegment(relaHeader);
            Require(relaData.size() == 24);
        }},
        {"valid DT_REL dynamic relocation tables and table translations", [] {
            auto bytes = DynamicFixture({
                {Elfpatcher::DT_REL, 0x1050},
                {Elfpatcher::DT_RELSZ, 32},
                {Elfpatcher::DT_RELENT, 16}
            });
            Relinker::ElfReader reader(std::move(bytes));
            const auto phs = reader.ReadProgramHeaders();
            Require(phs.size() == 2 && phs[1].Type == 2);
            const auto tags = reader.ReadDynamicTags(phs[1]);
            Require(tags.size() == 3);
            Require(tags[0].Tag == Elfpatcher::DT_REL && tags[0].Value == 0x1050);
            Require(tags[1].Tag == Elfpatcher::DT_RELSZ && tags[1].Value == 32);
            Require(tags[2].Tag == Elfpatcher::DT_RELENT && tags[2].Value == 16);

            const auto relOffset = reader.TranslateVirtualAddress(0x1050);
            Require(relOffset == 0x250);
            Relinker::ProgramHeader relHeader{};
            relHeader.Offset = relOffset;
            relHeader.FileSize = 32;
            const auto relData = reader.ReadSegment(relHeader);
            Require(relData.size() == 32);
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
