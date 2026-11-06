#include <elfpatcher/windows/WindowsTrampolineBuilder.hpp>
#include <elfpatcher/windows/WindowsStubEmitter.hpp>
#include <codegen/x86/Amd64OnlySubstitutionTable.hpp>
#include <codegen/x86/StubBodyBuilder.hpp>
#include <io/BufferUtils.hpp>
#include <algorithm>
#include <limits>
#include <map>
#include <span>

namespace Elfpatcher::Windows {

namespace {

struct SitePatch {
    PeSection* Section = nullptr;
    std::ptrdiff_t Offset = 0;
    std::size_t Length = 0;
    std::vector<std::uint8_t> JumpBytes;
};

struct IncomingPatch {
    PeSection* Section = nullptr;
    std::ptrdiff_t Offset = 0;
    std::uint32_t Displacement = 0;
};

PeSection& findSection(std::vector<PeSection>& sections, const std::uint32_t rva, const std::size_t length) {
    for (auto& section : sections) {
        if (rva < section.Rva || rva - section.Rva >= section.Data.size())
            continue;
        if (length > section.Data.size() - (rva - section.Rva))
            throw Domain::RelinkerException("AMD-only instruction crosses a PE section boundary", rva);
        if ((section.Characteristics & SectionExecute) == 0)
            throw Domain::RelinkerException("AMD-only instruction is in a non-executable PE section", rva);
        return section;
    }
    throw Domain::RelinkerException("AMD-only instruction is outside the PE image", rva);
}

}

void WindowsTrampolineBuilder::Build(const std::vector<Codegen::TrampolineSite>& sites, const WindowsLoadImage& image, std::vector<PeSection>& sections, std::uint32_t& nextRva) const {
    using namespace Codegen::Amd64OnlySubstitutionTable;
    if (sites.empty())
        return;
    if (nextRva < LoadRva || nextRva % SectionAlignment != 0)
        throw Domain::RelinkerException("Invalid Windows trampoline section RVA", nextRva);
    if (std::any_of(sections.begin(), sections.end(), [](const auto& section) { return section.Name == ".amdstub"; }))
        throw Domain::RelinkerException("Duplicate Windows trampoline section");
    if (!sections.empty() && nextRva < AlignRva(static_cast<std::uint64_t>(sections.back().Rva) + sections.back().MemorySize()))
        throw Domain::RelinkerException("Windows trampoline section overlaps preceding section", nextRva);
    const auto sectionRva = nextRva;
    std::vector<std::uint8_t> bytes;
    std::map<std::uint32_t, std::uint64_t> ranges;
    std::vector<SitePatch> patches;
    std::vector<IncomingPatch> incomingPatches;
    patches.reserve(sites.size());
    for (const auto& site : sites) {
        const auto siteRva = image.GetRva(site.Address, site.Length);
        if (site.Length < kJmpRel32.Size || site.OriginalBytes.size() != site.Length || site.Body.size() < kJmpRel32.Size || site.ReturnBranchOffset > site.Body.size() - kJmpRel32.Size || site.Body[site.ReturnBranchOffset] != kJmpRel32.Bytes[0])
            throw Domain::RelinkerException("Invalid AMD-only trampoline site", siteRva);
        const auto siteEndRva = static_cast<std::uint64_t>(siteRva) + site.Length;
        const auto next = ranges.lower_bound(siteRva);
        if ((next != ranges.end() && next->first < siteEndRva) || (next != ranges.begin() && std::prev(next)->second > siteRva))
            throw Domain::RelinkerException("Overlapping AMD-only trampoline sites", siteRva);
        ranges.emplace(siteRva, siteEndRva);
        auto& section = findSection(sections, siteRva, site.Length);
        const auto offset = static_cast<std::ptrdiff_t>(siteRva - section.Rva);
        if (!std::equal(site.OriginalBytes.begin(), site.OriginalBytes.end(), section.Data.begin() + offset))
            throw Domain::RelinkerException("AMD-only site bytes changed before patching", siteRva);
        bytes.resize(Io::AlignUp(bytes.size(), kStubAlignment), kTrapFill);
        const auto stubRva = CheckedRva(sectionRva + bytes.size());
        const auto bodyOffset = bytes.size();
        bytes.insert(bytes.end(), site.Body.begin(), site.Body.end());
        Codegen::ApplyStubRelocations(std::span<std::uint8_t>(bytes.data() + bodyOffset, site.ReturnBranchOffset), site.Relocations, siteRva, stubRva, site.Offset);
        const auto returnRva = CheckedRva(siteEndRva);
        const auto displacement = static_cast<std::int64_t>(returnRva) - (static_cast<std::int64_t>(stubRva) + static_cast<std::int64_t>(site.ReturnBranchOffset + kJmpRel32.Size));
        if (displacement < std::numeric_limits<std::int32_t>::min() || displacement > std::numeric_limits<std::int32_t>::max())
            throw Domain::RelinkerException("AMD-only stub return exceeds rel32 range", returnRva);
        Io::WriteU32(bytes, bodyOffset + site.ReturnBranchOffset + 1, static_cast<std::uint32_t>(displacement));
        for (const auto& fixup : site.Fixups) {
            if (fixup.BodyOffset + 4 > site.Body.size())
                throw Domain::RelinkerException("Relocated operand is outside the stub body", siteRva);
            const auto targetRva = image.GetRva(fixup.Target);
            const auto operandRva = static_cast<std::int64_t>(stubRva) + static_cast<std::int64_t>(fixup.BodyOffset) + 4;
            const auto relocated = static_cast<std::int64_t>(targetRva) - operandRva;
            if (relocated < std::numeric_limits<std::int32_t>::min() || relocated > std::numeric_limits<std::int32_t>::max())
                throw Domain::RelinkerException("Relocated operand exceeds rel32 range", siteRva);
            Io::WriteU32(bytes, bodyOffset + fixup.BodyOffset, static_cast<std::uint32_t>(relocated));
        }
        for (const auto& branch : site.Incoming) {
            const auto displacementRva = image.GetRva(branch.DisplacementAddress, 4);
            auto& target = findSection(sections, displacementRva, 4);
            const auto entryRva = static_cast<std::int64_t>(stubRva) + static_cast<std::int64_t>(branch.BodyOffset);
            const auto relative = entryRva - static_cast<std::int64_t>(image.GetRva(branch.BranchEnd));
            if (relative < std::numeric_limits<std::int32_t>::min() || relative > std::numeric_limits<std::int32_t>::max())
                throw Domain::RelinkerException("Incoming branch cannot reach the stub", displacementRva);
            incomingPatches.push_back({&target, static_cast<std::ptrdiff_t>(displacementRva - target.Rva), static_cast<std::uint32_t>(relative)});
        }
        WindowsStubEmitter jump(siteRva);
        jump.Rip({0xe9}, stubRva);
        patches.push_back({&section, offset, site.Length, jump.TakeBytes()});
    }
    const auto endRva = AlignRva(sectionRva + bytes.size());
    for (const auto& patch : incomingPatches)
        Io::WriteU32(patch.Section->Data, patch.Offset, patch.Displacement);
    for (const auto& patch : patches) {
        std::fill_n(patch.Section->Data.begin() + patch.Offset, patch.Length, kNop1.Bytes[0]);
        std::copy(patch.JumpBytes.begin(), patch.JumpBytes.end(), patch.Section->Data.begin() + patch.Offset);
    }
    sections.push_back({".amdstub", sectionRva, SectionRead | SectionExecute | 0x20u, std::move(bytes)});
    nextRva = endRva;
}

}
