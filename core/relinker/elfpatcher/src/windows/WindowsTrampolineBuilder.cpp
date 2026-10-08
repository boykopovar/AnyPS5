#include <elfpatcher/windows/WindowsTrampolineBuilder.hpp>
#include <elfpatcher/windows/WindowsStubEmitter.hpp>
#include <codegen/x86/Amd64OnlySubstitutionTable.hpp>
#include <codegen/x86/StubBodyBuilder.hpp>
#include <io/BufferUtils.hpp>
#include <algorithm>
#include <limits>
#include <span>

namespace Elfpatcher::Windows {

namespace {

PeSection& findSection(std::vector<PeSection>& sections, const std::uint32_t rva, const std::size_t length) {
    for (auto& section : sections) {
        if (rva < section.Rva || rva - section.Rva >= section.Data.size())
            continue;
        if (length > section.Data.size() - (rva - section.Rva))
            throw Domain::RelinkerException("AMD-only instruction crosses a PE section boundary", rva);
        return section;
    }
    throw Domain::RelinkerException("AMD-only instruction is outside the PE image", rva);
}

}

void WindowsTrampolineBuilder::Build(const std::vector<Codegen::TrampolineSite>& sites, const WindowsLoadImage& image, std::vector<PeSection>& sections, std::uint32_t& nextRva) const {
    using namespace Codegen::Amd64OnlySubstitutionTable;
    if (sites.empty())
        return;
    const auto sectionRva = nextRva;
    std::vector<std::uint8_t> bytes;
    for (const auto& site : sites) {
        const auto siteRva = image.GetRva(site.Address, site.Length);
        if (site.Length < kJmpRel32.Size || site.OriginalBytes.size() != site.Length || site.Body.size() < kJmpRel32.Size || site.ReturnBranchOffset > site.Body.size() - kJmpRel32.Size || site.Body[site.ReturnBranchOffset] != kJmpRel32.Bytes[0])
            throw Domain::RelinkerException("Invalid AMD-only trampoline site", siteRva);
        auto& section = findSection(sections, siteRva, site.Length);
        const auto offset = static_cast<std::ptrdiff_t>(siteRva - section.Rva);
        if (!std::equal(site.OriginalBytes.begin(), site.OriginalBytes.end(), section.Data.begin() + offset))
            throw Domain::RelinkerException("AMD-only site bytes changed before patching", siteRva);
        bytes.resize(Io::AlignUp(bytes.size(), kStubAlignment), kTrapFill);
        const auto stubRva = CheckedRva(sectionRva + bytes.size());
        const auto bodyOffset = bytes.size();
        bytes.insert(bytes.end(), site.Body.begin(), site.Body.end());
        Codegen::ApplyStubRelocations(std::span<std::uint8_t>(bytes.data() + bodyOffset, site.ReturnBranchOffset), site.Relocations, siteRva, stubRva, site.Offset);
        const auto returnRva = CheckedRva(siteRva + site.Length);
        const auto displacement = static_cast<std::int64_t>(returnRva) - (static_cast<std::int64_t>(stubRva) + static_cast<std::int64_t>(site.ReturnBranchOffset + kJmpRel32.Size));
        if (displacement < std::numeric_limits<std::int32_t>::min() || displacement > std::numeric_limits<std::int32_t>::max())
            throw Domain::RelinkerException("AMD-only stub return exceeds rel32 range", returnRva);
        Io::WriteU32(bytes, bodyOffset + site.ReturnBranchOffset + 1, static_cast<std::uint32_t>(displacement));
        WindowsStubEmitter jump(siteRva);
        jump.Rip({0xe9}, stubRva);
        const auto jumpBytes = jump.TakeBytes();
        std::fill_n(section.Data.begin() + offset, site.Length, kNop1.Bytes[0]);
        std::copy(jumpBytes.begin(), jumpBytes.end(), section.Data.begin() + offset);
    }
    sections.push_back({".amdstub", sectionRva, SectionRead | SectionExecute | 0x20u, std::move(bytes)});
    nextRva = AlignRva(sectionRva + sections.back().Data.size());
}

}
