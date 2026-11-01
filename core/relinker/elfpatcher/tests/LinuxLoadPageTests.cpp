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
#include <optional>
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

Domain::ProgramHeader Load(const std::uint64_t offset, const std::uint64_t vaddr, const std::uint64_t fileSize, const std::uint64_t memorySize, const std::uint32_t flags, const std::uint64_t alignment = kPage) {
    return {1, flags, offset, vaddr, vaddr, fileSize, memorySize, alignment};
}

Elfpatcher::Linux::LinuxElfPatcher Patcher(const std::optional<std::uint64_t> fixedBase = std::nullopt) {
    const auto byteWriter = std::make_shared<Io::ByteWriter>();
    return Elfpatcher::Linux::LinuxElfPatcher(
        std::make_shared<Elfpatcher::EntryStubBuilder>(),
        std::make_shared<Elfpatcher::ProgramHeaderLayoutBuilder>(std::make_shared<Elfpatcher::SegmentFilter>(), byteWriter),
        std::make_shared<Elfpatcher::SectionHeaderTableBuilder>(byteWriter),
        byteWriter,
        fixedBase);
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

std::uint64_t OutputAlignment(const Bytes& output, const std::uint64_t vaddr) {
    for (const auto& load : OutputLoads(output))
        if (load.MappedAddress == vaddr) return load.Alignment;
    throw std::runtime_error("linux load pages: no output PT_LOAD at the segment's address");
}

std::uint64_t MaximumAlignment(const Bytes& output) {
    std::uint64_t largest = kPage;
    for (const auto& load : OutputLoads(output)) {
        Require(load.Alignment <= 1 || load.Offset % load.Alignment == load.MappedAddress % load.Alignment, "PT_LOAD offset and address differ modulo its alignment");
        if (load.Alignment != 0 && (load.Alignment & (load.Alignment - 1)) == 0)
            largest = std::max(largest, load.Alignment);
    }
    return largest;
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

void IncongruentSegmentAtPageStart() {
    auto source = Fixture(0x2000);
    std::fill(source.begin() + 0x1000, source.begin() + 0x1010, std::uint8_t{0x33});
    std::fill(source.begin() + 0x1010, source.begin() + 0x1110, std::uint8_t{0xAA});
    const std::vector<Domain::ProgramHeader> headers{
        Load(0x400, 0x400, 0x10, 0x10, 5),
        Load(0x1010, 0x1000, 0x100, 0x100, 6),
    };
    const auto output = Patcher().Patch(source, headers, {}, 0, "$ORIGIN/libs", true, false, {});
    RequireImage(output, source, headers, "segment at a page start with an offset inside a page");
}

void IncongruentSegmentInsideASharedPage() {
    auto source = Fixture(0x2000);
    std::fill(source.begin() + 0x1000, source.begin() + 0x1100, std::uint8_t{0xAA});
    std::fill(source.begin() + 0x1200, source.begin() + 0x1210, std::uint8_t{0x33});
    std::fill(source.begin() + 0x1210, source.begin() + 0x1310, std::uint8_t{0x5A});
    const std::vector<Domain::ProgramHeader> headers{
        Load(0x400, 0x400, 0x10, 0x10, 5),
        Load(0x1000, 0x1000, 0x100, 0x220, 6),
        Load(0x1210, 0x1220, 0x100, 0x100, 4),
    };
    const auto output = Patcher().Patch(source, headers, {}, 0, "$ORIGIN/libs", true, false, {});
    RequireImage(output, source, headers, "segment in a shared page with an offset at another place in its page");
}

void IncongruentOffsetBelowThePagePrefix() {
    auto source = Fixture(0x1000);
    std::fill(source.begin() + 0x100, source.begin() + 0x180, std::uint8_t{0xBB});
    const std::vector<Domain::ProgramHeader> headers{
        Load(0x400, 0x400, 0x10, 0x10, 5),
        Load(0x100, 0x2300, 0x80, 0x80, 4),
    };
    const auto output = Patcher().Patch(source, headers, {}, 0, "$ORIGIN/libs", true, false, {});
    RequireImage(output, source, headers, "segment whose offset lies below its page prefix");
}

void LargeAlignmentMovesToThePage() {
    for (const auto alignment : {std::uint64_t{0x80000000}, std::uint64_t{0x4000}, std::uint64_t{0x8000000000000000}}) {
        auto source = Fixture(0x2000);
        std::fill(source.begin() + 0x1000, source.begin() + 0x1010, std::uint8_t{0x33});
        std::fill(source.begin() + 0x1010, source.begin() + 0x1110, std::uint8_t{0xAA});
        const std::vector<Domain::ProgramHeader> headers{
            Load(0x400, 0x400, 0x10, 0x10, 5),
            Load(0x1010, 0x1000, 0x100, 0x100, 6, alignment),
        };
        const auto output = Patcher().Patch(source, headers, {}, 0, "$ORIGIN/libs", true, false, {});
        Require(output.size() <= source.size() + 3 * kPage, "output grew to " + std::to_string(output.size()) + " bytes for alignment " + std::to_string(alignment));
        RequireImage(output, source, headers, "incongruent segment with a large alignment");
        Require(OutputAlignment(output, 0x1000) == kPage, "moved segment declares an alignment its offset does not honour");
        Require(MaximumAlignment(output) == alignment, "the largest PT_LOAD alignment is " + std::to_string(MaximumAlignment(output)) + ", not " + std::to_string(alignment));
    }
}

void LargeAlignmentOnASharedPage() {
    auto source = Fixture(0x3000);
    std::fill(source.begin() + 0x1000, source.begin() + 0x1100, std::uint8_t{0xAA});
    std::fill(source.begin() + 0x1100, source.begin() + 0x1c20, std::uint8_t{0x55});
    std::fill(source.begin() + 0x1c20, source.begin() + 0x2000, std::uint8_t{0x77});
    const std::vector<Domain::ProgramHeader> headers{
        Load(0x400, 0x400, 0x10, 0x10, 5),
        Load(0x1000, 0x1000, 0x100, 0x1c20, 6),
        Load(0x1c20, 0x2c20, 0x3e0, 0x3e0, 0, std::uint64_t{0x80000000}),
    };
    const auto output = Patcher().Patch(source, headers, {}, 0, "$ORIGIN/libs", true, false, {});
    Require(output.size() <= source.size() + 3 * kPage, "output grew to " + std::to_string(output.size()) + " bytes on the shared page path");
    RequireImage(output, source, headers, "shared page with a large alignment");
    Require(OutputAlignment(output, 0x2c20) == kPage, "moved segment declares an alignment its offset does not honour");
    Require(MaximumAlignment(output) == 0x80000000, "the largest PT_LOAD alignment is " + std::to_string(MaximumAlignment(output)));
}

void CongruentLargeAlignmentOnASharedPage() {
    auto source = Fixture(0x3000);
    std::fill(source.begin() + 0x1000, source.begin() + 0x1100, std::uint8_t{0xAA});
    std::fill(source.begin() + 0x1100, source.begin() + 0x1c20, std::uint8_t{0x55});
    std::fill(source.begin() + 0x1c20, source.begin() + 0x2000, std::uint8_t{0x77});
    const std::vector<Domain::ProgramHeader> headers{
        Load(0x400, 0x400, 0x10, 0x10, 5),
        Load(0x1000, 0x1000, 0x100, 0x4c20, 6),
        Load(0x1c20, 0x5c20, 0x3e0, 0x3e0, 4, 0x4000),
    };
    const auto output = Patcher().Patch(source, headers, {}, 0, "$ORIGIN/libs", true, false, {});
    Require(output.size() <= source.size() + 3 * kPage, "output grew to " + std::to_string(output.size()) + " bytes for a congruent alignment");
    RequireImage(output, source, headers, "congruent segment with a large alignment on a shared page");
    Require(OutputOffset(output, 0x5c20) != 0x1c20 && OutputOffset(output, 0x5c20) % kPage == 0xc20, "a segment whose page reads other bytes was not moved to a page-congruent offset");
    Require(OutputAlignment(output, 0x5c20) == kPage, "moved segment declares an alignment its offset does not honour");
    Require(MaximumAlignment(output) == 0x4000, "the largest PT_LOAD alignment fell to " + std::to_string(MaximumAlignment(output)));
    const auto loads = OutputLoads(output);
    const auto headerBlock = std::find_if(loads.begin(), loads.end(), [](const Domain::ProgramHeader& load) { return load.Offset == 0; });
    Require(headerBlock != loads.end() && headerBlock->Alignment == 0x4000 && headerBlock->MappedAddress % 0x4000 == 0, "the header block does not carry the moved segment's alignment");
    const auto fixed = Patcher(0x400000).Patch(source, headers, {}, 0, "$ORIGIN/libs", false, false, {});
    Require(MaximumAlignment(fixed) == 0x4000, "the largest PT_LOAD alignment at a fixed base fell to " + std::to_string(MaximumAlignment(fixed)));
}

void LargeAlignmentAtAFixedBaseIsRefused() {
    for (const auto alignment : {std::uint64_t{0x80000000}, std::uint64_t{0x8000000000000000}}) {
        auto source = Fixture(0x2000);
        std::fill(source.begin() + 0x1010, source.begin() + 0x1110, std::uint8_t{0xAA});
        const std::vector<Domain::ProgramHeader> headers{
            Load(0x400, 0x400, 0x10, 0x10, 5),
            Load(0x1010, 0x1000, 0x100, 0x100, 6, alignment),
        };
        std::string message;
        try {
            Patcher(0x400000).Patch(source, headers, {}, 0, "$ORIGIN/libs", false, false, {});
        } catch (const std::exception& error) {
            message = error.what();
        }
        Require(message.find("is not a multiple of the PT_LOAD alignment") != std::string::npos, "fixed base accepted an alignment it is not a multiple of: " + message);
    }
}

}

int main() {
    try {
        ZeroFillBelowTheNextSegment();
        DataAndZeroFillBelowTheNextSegment();
        ZeroFileBytesStayInPlace();
        TrampolineDisplacementAcrossInt64Max();
        TrampolineDisplacementBeyondInt64Range();
        IncongruentSegmentAtPageStart();
        IncongruentSegmentInsideASharedPage();
        IncongruentOffsetBelowThePagePrefix();
        LargeAlignmentMovesToThePage();
        LargeAlignmentOnASharedPage();
        CongruentLargeAlignmentOnASharedPage();
        LargeAlignmentAtAFixedBaseIsRefused();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
