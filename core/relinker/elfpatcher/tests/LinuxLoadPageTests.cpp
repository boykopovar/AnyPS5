#include <elfpatcher/linux/LinuxElfPatcher.hpp>
#include <elfpatcher/general/EntryStubBuilder.hpp>
#include <elfpatcher/general/ProgramHeaderLayoutBuilder.hpp>
#include <elfpatcher/general/SectionHeaderTableBuilder.hpp>
#include <elfpatcher/general/SegmentFilter.hpp>
#include <codegen/x86/Amd64OnlySubstitutionTable.hpp>
#include <io/ByteWriter.hpp>
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using Bytes = std::vector<std::uint8_t>;

constexpr std::uint64_t kPage = 0x1000;

void Require(const bool ok, const std::string& what) {
    if (!ok) throw std::runtime_error("linux load pages: " + what);
}

template <typename T>
T Read(const Bytes& bytes, const std::uint64_t offset) {
    T value{};
    std::memcpy(&value, bytes.data() + offset, sizeof(T));
    return value;
}

template <typename T>
void Write(Bytes& bytes, const std::uint64_t offset, const T value) {
    std::memcpy(bytes.data() + offset, &value, sizeof(T));
}

Bytes Fixture(const std::size_t size) {
    Bytes bytes(size);
    bytes[0] = 0x7F; bytes[1] = 'E'; bytes[2] = 'L'; bytes[3] = 'F';
    bytes[4] = 2; bytes[5] = 1; bytes[6] = 1;
    Write<std::uint16_t>(bytes, 16, 3);
    Write<std::uint16_t>(bytes, 18, 62);
    Write<std::uint64_t>(bytes, 24, 0x400);
    Write<std::uint64_t>(bytes, 32, 64);
    Write<std::uint16_t>(bytes, 54, 56);
    Write<std::uint16_t>(bytes, 56, 1);
    bytes[0x400] = 0xC3;
    return bytes;
}

Domain::ProgramHeader Load(const std::uint64_t offset, const std::uint64_t vaddr, const std::uint64_t fileSize, const std::uint64_t memorySize, const std::uint32_t flags) {
    return {1, flags, offset, vaddr, vaddr, fileSize, memorySize, 0x1000};
}

Elfpatcher::Linux::LinuxElfPatcher Patcher() {
    const auto byteWriter = std::make_shared<Io::ByteWriter>();
    return Elfpatcher::Linux::LinuxElfPatcher(
        std::make_shared<Elfpatcher::EntryStubBuilder>(),
        std::make_shared<Elfpatcher::ProgramHeaderLayoutBuilder>(std::make_shared<Elfpatcher::SegmentFilter>(), byteWriter),
        std::make_shared<Elfpatcher::SectionHeaderTableBuilder>(byteWriter),
        byteWriter);
}

std::vector<Domain::ProgramHeader> OutputLoads(const Bytes& output) {
    std::vector<Domain::ProgramHeader> loads;
    const auto table = Read<std::uint64_t>(output, 32);
    const auto count = Read<std::uint16_t>(output, 56);
    for (std::uint16_t index = 0; index < count; ++index) {
        const auto header = table + index * 56u;
        if (Read<std::uint32_t>(output, header) != 1) continue;
        loads.push_back({1, Read<std::uint32_t>(output, header + 4), Read<std::uint64_t>(output, header + 8), Read<std::uint64_t>(output, header + 16),
            Read<std::uint64_t>(output, header + 24), Read<std::uint64_t>(output, header + 32), Read<std::uint64_t>(output, header + 40), Read<std::uint64_t>(output, header + 48)});
    }
    return loads;
}

std::map<std::uint64_t, std::uint8_t> MapLikeLinux(const Bytes& output) {
    std::map<std::uint64_t, std::uint8_t> memory;
    for (const auto& load : OutputLoads(output)) {
        const auto prefix = load.MappedAddress % kPage;
        Require(load.Offset % kPage == prefix, "PT_LOAD offset and address differ within a page");
        const auto fileEnd = load.MappedAddress + load.FileSize;
        const auto filePageEnd = (fileEnd + kPage - 1) & ~(kPage - 1);
        for (auto address = load.MappedAddress - prefix; address < filePageEnd; ++address) {
            const auto offset = load.Offset - prefix + (address - (load.MappedAddress - prefix));
            memory[address] = offset < output.size() ? output[offset] : 0;
        }
        if (load.MemorySize > load.FileSize) {
            const auto memoryPageEnd = (load.MappedAddress + load.MemorySize + kPage - 1) & ~(kPage - 1);
            for (auto address = fileEnd; address < memoryPageEnd; ++address)
                memory[address] = 0;
        }
    }
    return memory;
}

void RequireImage(const Bytes& output, const Bytes& source, const std::vector<Domain::ProgramHeader>& headers, const std::string& what) {
    const auto memory = MapLikeLinux(output);
    for (const auto& load : headers) {
        for (std::uint64_t relative = 0; relative < load.MemorySize; ++relative) {
            const auto address = load.MappedAddress + relative;
            const auto found = memory.find(address);
            Require(found != memory.end(), what + ": address not mapped");
            const std::uint8_t expected = relative < load.FileSize ? source[load.Offset + relative] : 0;
            if (found->second != expected) {
                char text[96];
                std::snprintf(text, sizeof(text), ": byte at 0x%llx reads 0x%02x, not 0x%02x", static_cast<unsigned long long>(address), found->second, expected);
                Require(false, what + text);
            }
        }
    }
}

std::uint64_t OutputOffset(const Bytes& output, const std::uint64_t vaddr) {
    for (const auto& load : OutputLoads(output))
        if (load.MappedAddress == vaddr) return load.Offset;
    throw std::runtime_error("linux load pages: no output PT_LOAD at the segment's address");
}

void ZeroFillBelowTheNextSegment() {
    auto source = Fixture(0x3000);
    std::fill(source.begin() + 0x1000, source.begin() + 0x1100, std::uint8_t{0xAA});
    std::fill(source.begin() + 0x1100, source.begin() + 0x1c20, std::uint8_t{0x55});
    std::fill(source.begin() + 0x1c20, source.begin() + 0x2000, std::uint8_t{0x77});
    const std::vector<Domain::ProgramHeader> headers{
        Load(0x400, 0x400, 0x10, 0x10, 5),
        Load(0x1000, 0x1000, 0x100, 0x1c20, 6),
        Load(0x1c20, 0x2c20, 0x3e0, 0x3e0, 0),
    };
    const auto output = Patcher().Patch(source, headers, {}, 0, "$ORIGIN/libs", true, false, {});
    RequireImage(output, source, headers, "bss sharing a page with the next segment");
}

void DataAndZeroFillBelowTheNextSegment() {
    auto source = Fixture(0x4000);
    std::fill(source.begin() + 0x1000, source.begin() + 0x2400, std::uint8_t{0xAA});
    std::fill(source.begin() + 0x2400, source.begin() + 0x2c20, std::uint8_t{0x55});
    std::fill(source.begin() + 0x2c20, source.begin() + 0x3000, std::uint8_t{0x77});
    const std::vector<Domain::ProgramHeader> headers{
        Load(0x400, 0x400, 0x10, 0x10, 5),
        Load(0x1000, 0x1000, 0x1400, 0x1c20, 6),
        Load(0x2c20, 0x2c20, 0x3e0, 0x3e0, 0),
    };
    const auto output = Patcher().Patch(source, headers, {}, 0, "$ORIGIN/libs", true, false, {});
    RequireImage(output, source, headers, "data and bss sharing a page with the next segment");
}

void ZeroFileBytesStayInPlace() {
    auto source = Fixture(0x3000);
    std::fill(source.begin() + 0x1000, source.begin() + 0x1100, std::uint8_t{0xAA});
    std::fill(source.begin() + 0x2c20, source.begin() + 0x3000, std::uint8_t{0x77});
    const std::vector<Domain::ProgramHeader> headers{
        Load(0x400, 0x400, 0x10, 0x10, 5),
        Load(0x1000, 0x1000, 0x100, 0x1c20, 6),
        Load(0x2c20, 0x2c20, 0x3e0, 0x3e0, 0),
    };
    const auto output = Patcher().Patch(source, headers, {}, 0, "$ORIGIN/libs", true, false, {});
    RequireImage(output, source, headers, "zero bytes below the next segment");
    Require(OutputOffset(output, 0x2c20) == 0x2c20, "a segment whose page already reads right was moved");
}

std::uint64_t VaddrOf(const Bytes& output, const std::uint64_t fileOffset) {
    for (const auto& load : OutputLoads(output))
        if (fileOffset >= load.Offset && fileOffset < load.Offset + load.FileSize)
            return load.MappedAddress + (fileOffset - load.Offset);
    throw std::runtime_error("linux load pages: file offset is not covered by an output PT_LOAD");
}

void TrampolineDisplacementAcrossInt64Max() {
    using Codegen::Amd64OnlySubstitutionTable::kJmpRel32;
    auto source = Fixture(0x3000);
    const Bytes insertq = {0xF2, 0x0F, 0x79, 0xD1, 0x10, 0x20};
    std::copy(insertq.begin(), insertq.end(), source.begin() + 0x500);
    Write<std::uint64_t>(source, 24, 0x7FFFFFFFFFFFE100);
    const std::vector<Domain::ProgramHeader> headers{
        Load(0x400, 0x7FFFFFFFFFFFE000, 0x2000, 0x2000, 5),
    };
    Codegen::TrampolineSite site;
    site.Offset = 0x500;
    site.Address = 0x7FFFFFFFFFFFE100;
    site.Length = insertq.size();
    site.OriginalBytes = insertq;
    site.Body = {0xE9, 0x00, 0x00, 0x00, 0x00, 0xDE, 0xAD, 0xBE, 0xEF, 0xC3};
    site.ReturnBranchOffset = 0;
    const auto output = Patcher().Patch(source, headers, {}, 0, "$ORIGIN/libs", true, false, {site});
    const Bytes tail(site.Body.begin() + site.ReturnBranchOffset + kJmpRel32.Size, site.Body.end());
    const auto tailStart = std::search(output.begin(), output.end(), tail.begin(), tail.end());
    Require(tailStart != output.end(), "the trampoline body is missing from the output");
    const auto bodyOff = static_cast<std::uint64_t>(tailStart - output.begin()) - site.ReturnBranchOffset - kJmpRel32.Size;
    const auto bodyVaddr = VaddrOf(output, bodyOff);
    Require(bodyVaddr >= 0x8000000000000000, "the trampoline body did not land above 2^63");
    const auto jumpDisplacement = static_cast<std::int32_t>(bodyVaddr - (site.Address + kJmpRel32.Size));
    const auto returnDisplacement = static_cast<std::int32_t>(site.Address + site.Length - (bodyVaddr + site.ReturnBranchOffset + kJmpRel32.Size));
    Require(Read<std::uint8_t>(output, site.Offset) == kJmpRel32.Bytes[0], "the trampoline site was not rewritten as a rel32 jump");
    Require(Read<std::uint32_t>(output, site.Offset + 1) == static_cast<std::uint32_t>(jumpDisplacement), "the trampoline jump displacement is wrong");
    Require(Read<std::uint32_t>(output, bodyOff + site.ReturnBranchOffset + 1) == static_cast<std::uint32_t>(returnDisplacement), "the trampoline return displacement is wrong");
}

void TrampolineDisplacementBeyondInt64Range() {
    using Codegen::Amd64OnlySubstitutionTable::kJmpRel32;
    auto source = Fixture(0x3000);
    const Bytes insertq = {0xF2, 0x0F, 0x79, 0xD1, 0x10, 0x20};
    std::copy(insertq.begin(), insertq.end(), source.begin() + 0x500);
    Write<std::uint64_t>(source, 24, 0xFFFFFFFFFFFFE100);
    const std::vector<Domain::ProgramHeader> headers{
        Load(0x400, 0x400, 0x1000, 0x1000, 5),
        Load(0x2000, 0xFFFFFFFFFFFFE000, 0x800, 0x800, 5),
    };
    Codegen::TrampolineSite site;
    site.Offset = 0x500;
    site.Address = 0x500;
    site.Length = insertq.size();
    site.OriginalBytes = insertq;
    site.Body = {0xE9, 0x00, 0x00, 0x00, 0x00, 0xDE, 0xAD, 0xBE, 0xEF, 0xC3};
    site.ReturnBranchOffset = 0;
    bool threw = false;
    try {
        (void)Patcher().Patch(source, headers, {}, 0, "$ORIGIN/libs", true, false, {site});
    } catch (const Domain::RelinkerException& error) {
        threw = std::string(error.what()).find("rel32") != std::string::npos;
    }
    Require(threw, "a trampoline whose site and body are more than 2^63 apart was accepted with a wrapped displacement");
}

}

int main() {
    try {
        ZeroFillBelowTheNextSegment();
        DataAndZeroFillBelowTheNextSegment();
        ZeroFileBytesStayInPlace();
        TrampolineDisplacementAcrossInt64Max();
        TrampolineDisplacementBeyondInt64Range();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
