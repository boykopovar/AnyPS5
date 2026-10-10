#include <elfpatcher/linux/LinuxElfPatcher.hpp>
#include <elfpatcher/general/ElfConstants.hpp>
#include <elfpatcher/general/ProgramHeaderLayoutRequest.hpp>
#include <elfpatcher/general/SectionHeaderTableRequest.hpp>
#include <codegen/x86/Amd64OnlySubstitutionTable.hpp>
#include <codegen/x86/StubBodyBuilder.hpp>
#include <io/BufferUtils.hpp>
#include <algorithm>
#include <limits>
#include <span>
#include <string>

namespace Elfpatcher::Linux {

LinuxElfPatcher::LinuxElfPatcher(
    std::shared_ptr<IEntryStubBuilder> entryStubBuilder,
    std::shared_ptr<IProgramHeaderLayoutBuilder> programHeaderLayoutBuilder,
    std::shared_ptr<ISectionHeaderTableBuilder> sectionHeaderTableBuilder,
    std::shared_ptr<Io::IByteWriter> byteWriter
)
    : _entryStubBuilder(std::move(entryStubBuilder))
    , _programHeaderLayoutBuilder(std::move(programHeaderLayoutBuilder))
    , _sectionHeaderTableBuilder(std::move(sectionHeaderTableBuilder))
    , _byteWriter(std::move(byteWriter))
{
}

void LinuxElfPatcher::_appendDynEntry(std::vector<std::uint8_t>& buf, std::int64_t tag, std::uint64_t val) const {
    _byteWriter->AppendI64(buf, tag);
    _byteWriter->AppendU64(buf, val);
}

void LinuxElfPatcher::_appendTrampoline(
    std::vector<std::uint8_t>& buf,
    const Codegen::TrampolineSite& site,
    const std::uint64_t extraBlockOffset,
    const std::function<std::uint64_t(std::uint64_t)>& vaddrOfExtraBlockOffset) const
{
    using namespace Codegen::Amd64OnlySubstitutionTable;
    if (site.Length < kJmpRel32.Size || site.OriginalBytes.size() != site.Length || site.Body.size() < kJmpRel32.Size || site.ReturnBranchOffset > site.Body.size() - kJmpRel32.Size || site.Body[site.ReturnBranchOffset] != kJmpRel32.Bytes[0])
        throw Domain::RelinkerException("Invalid AMD-only trampoline site", site.Offset);
    if (site.Offset > extraBlockOffset || site.Length > extraBlockOffset - site.Offset)
        throw Domain::RelinkerException("AMD-only instruction is outside the original image", site.Offset);
    if (!std::equal(site.OriginalBytes.begin(), site.OriginalBytes.end(), buf.begin() + static_cast<std::ptrdiff_t>(site.Offset)))
        throw Domain::RelinkerException("AMD-only site bytes changed before patching", site.Offset);

    while (buf.size() % kStubAlignment != 0)
        buf.push_back(kTrapFill);
    const auto bodyOff = static_cast<std::uint64_t>(buf.size());
    const auto bodyVaddr = vaddrOfExtraBlockOffset(bodyOff);
    for (const std::uint8_t b : site.Body)
        buf.push_back(b);
    Codegen::ApplyStubRelocations(std::span<std::uint8_t>(buf.data() + bodyOff, site.ReturnBranchOffset), site.Relocations, site.Address, bodyVaddr, site.Offset);

    const auto inRange = [](const std::int64_t displacement) {
        return displacement >= std::numeric_limits<std::int32_t>::min() && displacement <= std::numeric_limits<std::int32_t>::max();
    };
    const auto returnDisplacement = static_cast<std::int64_t>(site.Address + site.Length) - static_cast<std::int64_t>(bodyVaddr + site.ReturnBranchOffset + kJmpRel32.Size);
    const auto jumpDisplacement = static_cast<std::int64_t>(bodyVaddr) - static_cast<std::int64_t>(site.Address + kJmpRel32.Size);
    if (!inRange(returnDisplacement) || !inRange(jumpDisplacement))
        throw Domain::RelinkerException("AMD-only stub exceeds rel32 range", site.Offset);
    _byteWriter->WriteU32(buf, static_cast<std::size_t>(bodyOff + site.ReturnBranchOffset + 1), static_cast<std::uint32_t>(returnDisplacement));

    std::fill_n(buf.begin() + static_cast<std::ptrdiff_t>(site.Offset), site.Length, kNop1.Bytes[0]);
    buf[static_cast<std::size_t>(site.Offset)] = kJmpRel32.Bytes[0];
    _byteWriter->WriteU32(buf, static_cast<std::size_t>(site.Offset + 1), static_cast<std::uint32_t>(jumpDisplacement));
}

std::vector<std::uint8_t> LinuxElfPatcher::Patch(
    const std::vector<std::uint8_t>& sourceElf,
    const std::vector<Domain::ProgramHeader>& originalHeaders,
    const Domain::SysVDynamicSection& dynSection,
    const std::uint64_t originalPltGotVaddr,
    const std::string& runPath,
    const bool lazyBinding,
    const bool dependencyDiagnostics,
    const std::vector<Codegen::TrampolineSite>& trampolines)
{
    if (dependencyDiagnostics)
        throw Domain::RelinkerException("Linux target does not support --windows-diagnostics");

    std::vector<std::uint8_t> buf = sourceElf;

    buf[kEhdrOsAbiOffset] = 0;
    buf[kEhdrAbiVersionOffset] = 0;
    buf[kEhdrTypeOffset] = static_cast<std::uint8_t>(ET_DYN & 0xFF);
    buf[kEhdrTypeOffset + 1] = static_cast<std::uint8_t>((ET_DYN >> 8) & 0xFF);
    _byteWriter->WriteU64(buf, kEhdrShOffOffset, 0);
    _byteWriter->WriteU16(buf, kEhdrShNumOffset, 0);
    _byteWriter->WriteU16(buf, kEhdrShStrNdxOffset, 0);

    const std::uint64_t phOff = *reinterpret_cast<const std::uint64_t*>(buf.data() + kEhdrPhOffOffset);
    const std::uint16_t phEntSize = *reinterpret_cast<const std::uint16_t*>(buf.data() + kEhdrPhEntSizeOffset);
    const std::uint16_t phNum = *reinterpret_cast<const std::uint16_t*>(buf.data() + kEhdrPhNumOffset);

    const auto alignBuf = [](std::vector<std::uint8_t>& b, std::size_t alignment) {
        while (b.size() % alignment != 0)
            b.push_back(0);
    };

    const auto dynStrOff = static_cast<std::uint64_t>(buf.size());
    for (std::uint8_t b : dynSection.DynStrData)
        buf.push_back(b);
    const std::uint64_t runPathStrOff = static_cast<std::uint64_t>(buf.size()) - dynStrOff;
    for (char c : runPath)
        buf.push_back(static_cast<std::uint8_t>(c));
    buf.push_back(0);
    const std::uint64_t dynStrSize = static_cast<std::uint64_t>(buf.size()) - dynStrOff;
    alignBuf(buf, kDynStrAlignment);

    const auto dynSymOff = static_cast<std::uint64_t>(buf.size());
    for (std::uint8_t b : dynSection.DynSymData)
        buf.push_back(b);
    alignBuf(buf, kDynSymAlignment);

    const auto hashOff = static_cast<std::uint64_t>(buf.size());
    const auto symbolCount = static_cast<std::uint32_t>(dynSection.DynSymData.size() / kSymEntrySize);
    Io::AppendU32(buf, 1);
    Io::AppendU32(buf, symbolCount);
    Io::AppendU32(buf, symbolCount > 1 ? 1 : 0);
    for (std::uint32_t index = 0; index < symbolCount; ++index)
        Io::AppendU32(buf, index != 0 && index + 1 < symbolCount ? index + 1 : 0);
    alignBuf(buf, kDynSymAlignment);

    const auto relaOff = static_cast<std::uint64_t>(buf.size());
    for (std::uint8_t b : dynSection.RelaData)
        buf.push_back(b);
    alignBuf(buf, kRelaAlignment);

    const auto relaPltOff = static_cast<std::uint64_t>(buf.size());
    for (std::uint8_t b : dynSection.RelaPltData)
        buf.push_back(b);
    alignBuf(buf, kRelaPltAlignment);

    const std::uint64_t extraBlockOff = dynStrOff;
    const std::uint64_t extraBlockVaddr = _programHeaderLayoutBuilder->ComputeExtraBlockVaddr(originalHeaders, extraBlockOff);

    const auto vaddrOfExtraBlockOffset = [extraBlockOff, extraBlockVaddr](std::uint64_t fileOffset) -> std::uint64_t {
        if (fileOffset < extraBlockOff)
            throw Domain::RelinkerException("Extra block member offset lies before the extra block");
        return extraBlockVaddr + (fileOffset - extraBlockOff);
    };

    std::vector<std::uint8_t> dynSegBuf;
    for (std::uint8_t b : dynSection.DynamicSegmentData)
        dynSegBuf.push_back(b);
    _appendDynEntry(dynSegBuf, DT_STRTAB, vaddrOfExtraBlockOffset(dynStrOff));
    _appendDynEntry(dynSegBuf, DT_STRSZ, dynStrSize);
    _appendDynEntry(dynSegBuf, DT_SYMTAB, vaddrOfExtraBlockOffset(dynSymOff));
    _appendDynEntry(dynSegBuf, DT_SYMENT, kSymEntrySize);
    _appendDynEntry(dynSegBuf, DT_HASH, vaddrOfExtraBlockOffset(hashOff));
    _appendDynEntry(dynSegBuf, DT_DEBUG, 0);
    if (!dynSection.RelaData.empty()) {
        _appendDynEntry(dynSegBuf, DT_RELA, vaddrOfExtraBlockOffset(relaOff));
        _appendDynEntry(dynSegBuf, DT_RELASZ, dynSection.RelaData.size());
        _appendDynEntry(dynSegBuf, DT_RELAENT, kRelaEntrySize);
    }
    if (!dynSection.RelaPltData.empty()) {
        _appendDynEntry(dynSegBuf, DT_JMPREL, vaddrOfExtraBlockOffset(relaPltOff));
        _appendDynEntry(dynSegBuf, DT_PLTRELSZ, dynSection.RelaPltData.size());
        _appendDynEntry(dynSegBuf, DT_PLTREL, static_cast<std::uint64_t>(DT_RELA));
        _appendDynEntry(dynSegBuf, DT_PLTGOT, originalPltGotVaddr);
    }
    if (!lazyBinding)
        _appendDynEntry(dynSegBuf, DT_FLAGS, DF_BIND_NOW);
    _appendDynEntry(dynSegBuf, DT_RUNPATH, runPathStrOff);
    _appendDynEntry(dynSegBuf, DT_NULL, 0);

    const auto dynSegOff = static_cast<std::uint64_t>(buf.size());
    for (std::uint8_t b : dynSegBuf)
        buf.push_back(b);

    const std::uint64_t realEntryVaddr = *reinterpret_cast<const std::uint64_t*>(buf.data() + kEhdrEntryOffset);
    const auto stubOff = static_cast<std::uint64_t>(buf.size());
    const auto stubVaddr = vaddrOfExtraBlockOffset(stubOff);
    const auto stubBytes = _entryStubBuilder->BuildEntryStub(stubVaddr, realEntryVaddr);
    for (std::uint8_t b : stubBytes)
        buf.push_back(b);
    _byteWriter->WriteU64(buf, kEhdrEntryOffset, stubVaddr);

    for (const auto& site : trampolines)
        _appendTrampoline(buf, site, extraBlockOff, vaddrOfExtraBlockOffset);

    static constexpr char kInterp[] = "/lib64/ld-linux-x86-64.so.2";
    const auto interpOff = static_cast<std::uint64_t>(buf.size());
    constexpr std::uint64_t interpSize = sizeof(kInterp);
    for (char i : kInterp)
        buf.push_back(static_cast<std::uint8_t>(i));

    const std::uint64_t extraBlockSize = static_cast<std::uint64_t>(buf.size()) - extraBlockOff;

    ProgramHeaderLayoutRequest layoutRequest{};
    layoutRequest.PhOff = phOff;
    layoutRequest.PhEntSize = phEntSize;
    layoutRequest.PhNum = phNum;
    layoutRequest.OriginalHeaders = originalHeaders;
    layoutRequest.ExtraBlockOffset = extraBlockOff;
    layoutRequest.ExtraBlockVaddr = extraBlockVaddr;
    layoutRequest.ExtraBlockSize = extraBlockSize;
    layoutRequest.DynamicSegmentOffset = dynSegOff;
    layoutRequest.DynamicSegmentSize = dynSegBuf.size();
    layoutRequest.InterpOffset = interpOff;
    layoutRequest.InterpSize = interpSize;

    const std::uint16_t writtenPh = _programHeaderLayoutBuilder->WriteLayout(buf, layoutRequest);
    _byteWriter->WriteU16(buf, kEhdrPhNumOffset, writtenPh);

    SectionHeaderTableRequest sectionRequest{};
    sectionRequest.DynStrOffset = dynStrOff;
    sectionRequest.DynStrSize = dynStrSize;
    sectionRequest.DynSymOffset = dynSymOff;
    sectionRequest.DynSymSize = dynSection.DynSymData.size();
    sectionRequest.DynamicSegmentOffset = dynSegOff;
    sectionRequest.DynamicSegmentSize = dynSegBuf.size();
    sectionRequest.StubOffset = stubOff;
    sectionRequest.StubSize = stubBytes.size();

    _sectionHeaderTableBuilder->WriteTable(buf, sectionRequest);

    return buf;
}

}
