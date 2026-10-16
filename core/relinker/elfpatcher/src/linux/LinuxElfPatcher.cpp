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
#include <sstream>
#include <string>
#include <utility>

namespace Elfpatcher::Linux {

namespace {

constexpr std::uint64_t kLinuxUserAddressEnd = 0x800000000000;

std::string Hex(const std::uint64_t value) {
    std::ostringstream text;
    text << "0x" << std::hex << value;
    return text.str();
}

std::uint64_t Rebased(const std::uint64_t base, const std::uint64_t address, const std::string& what) {
    if (base > kLinuxUserAddressEnd || address > kLinuxUserAddressEnd - base)
        throw Domain::RelinkerException(what + " " + Hex(address) + " lies outside the Linux user address space at fixed base " + Hex(base));
    return base + address;
}

void RequireWholeEntries(const std::vector<std::uint8_t>& table, const std::uint64_t entrySize, const std::string& what) {
    if (table.size() % entrySize != 0)
        throw Domain::RelinkerException(what + " is not a whole number of entries");
}

void RequireOnlyDependencies(const std::vector<std::uint8_t>& entries) {
    RequireWholeEntries(entries, kDynEntrySize, "Executable dependency table");
    for (std::size_t entry = 0; entry < entries.size(); entry += kDynEntrySize) {
        const auto tag = static_cast<std::int64_t>(Io::ReadU64(entries, entry));
        if (tag != DT_NEEDED)
            throw Domain::RelinkerException("Dynamic tag " + Hex(static_cast<std::uint64_t>(tag)) + " cannot be placed at a fixed Linux base");
    }
}

std::vector<std::uint8_t> RebasedSymbols(const std::vector<std::uint8_t>& table, const std::uint64_t base) {
    RequireWholeEntries(table, kSymEntrySize, "Dynamic symbol table");
    auto rebased = table;
    for (std::size_t entry = 0; entry < rebased.size(); entry += kSymEntrySize) {
        const auto section = Io::ReadU16(rebased, entry + 6);
        if (section == SHN_UNDEF || section == SHN_ABS || (rebased[entry + 4] & 0xf) == STT_TLS)
            continue;
        if (section >= SHN_LORESERVE)
            throw Domain::RelinkerException("Symbol section index " + Hex(section) + " cannot be placed at a fixed Linux base");
        Io::WriteU64(rebased, entry + 8, Rebased(base, Io::ReadU64(rebased, entry + 8), "Symbol"));
    }
    return rebased;
}

std::vector<std::uint8_t> RebasedRelocations(const std::vector<std::uint8_t>& table, const std::uint64_t base) {
    RequireWholeEntries(table, kRelaEntrySize, "Relocation table");
    auto rebased = table;
    for (std::size_t entry = 0; entry < rebased.size(); entry += kRelaEntrySize) {
        const auto type = static_cast<std::uint32_t>(Io::ReadU64(rebased, entry + 8));
        switch (type) {
        case R_X86_64_NONE:
            continue;
        case R_X86_64_RELATIVE:
        case R_X86_64_IRELATIVE: {
            const auto addend = static_cast<std::int64_t>(Io::ReadU64(rebased, entry + 16));
            if (addend < 0)
                throw Domain::RelinkerException("Relocation target " + std::to_string(addend) + " lies below the image at a fixed Linux base");
            Io::WriteU64(rebased, entry + 16, Rebased(base, static_cast<std::uint64_t>(addend), "Relocation target"));
            break;
        }
        case R_X86_64_64:
        case R_X86_64_PC32:
        case R_X86_64_COPY:
        case R_X86_64_GLOB_DAT:
        case R_X86_64_JUMP_SLOT:
        case R_X86_64_32:
        case R_X86_64_32S:
        case R_X86_64_DTPMOD64:
        case R_X86_64_DTPOFF64:
        case R_X86_64_TPOFF64:
            break;
        default:
            throw Domain::RelinkerException("Relocation type " + std::to_string(type) + " cannot be placed at a fixed Linux base");
        }
        Io::WriteU64(rebased, entry, Rebased(base, Io::ReadU64(rebased, entry), "Relocation offset"));
    }
    return rebased;
}

void RebaseProgramHeaders(std::vector<std::uint8_t>& buf, const std::uint64_t tableOffset, const std::uint16_t entrySize, const std::uint16_t count, const std::uint64_t base) {
    for (std::uint16_t index = 0; index < count; ++index) {
        const auto header = static_cast<std::size_t>(tableOffset + static_cast<std::uint64_t>(index) * entrySize);
        const auto type = Io::ReadU32(buf, header + kPhdrTypeOffset);
        if (type == PT_NULL || type == PT_GNU_STACK)
            continue;
        const auto address = Rebased(base, Io::ReadU64(buf, header + kPhdrVaddrOffset), "Program header address");
        const auto physical = Rebased(base, Io::ReadU64(buf, header + kPhdrPaddrOffset), "Program header physical address");
        if (type == PT_LOAD) {
            const auto alignment = Io::ReadU64(buf, header + kPhdrAlignOffset);
            if (alignment > 1 && base % alignment != 0)
                throw Domain::RelinkerException("Fixed Linux base " + Hex(base) + " is not a multiple of the PT_LOAD alignment " + Hex(alignment));
            if (Io::ReadU64(buf, header + kPhdrMemSizeOffset) > kLinuxUserAddressEnd - address)
                throw Domain::RelinkerException("PT_LOAD at " + Hex(address) + " ends outside the Linux user address space");
        }
        Io::WriteU64(buf, header + kPhdrVaddrOffset, address);
        Io::WriteU64(buf, header + kPhdrPaddrOffset, physical);
    }
}

}

LinuxElfPatcher::LinuxElfPatcher(
    std::shared_ptr<IEntryStubBuilder> entryStubBuilder,
    std::shared_ptr<IProgramHeaderLayoutBuilder> programHeaderLayoutBuilder,
    std::shared_ptr<ISectionHeaderTableBuilder> sectionHeaderTableBuilder,
    std::shared_ptr<Io::IByteWriter> byteWriter,
    std::optional<std::uint64_t> fixedBase
)
    : _entryStubBuilder(std::move(entryStubBuilder))
    , _programHeaderLayoutBuilder(std::move(programHeaderLayoutBuilder))
    , _sectionHeaderTableBuilder(std::move(sectionHeaderTableBuilder))
    , _byteWriter(std::move(byteWriter))
    , _fixedBase(fixedBase)
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
    std::vector<std::uint8_t> sourceElf,
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
    if (_fixedBase && lazyBinding)
        throw Domain::RelinkerException("A fixed Linux base requires eager binding; --lazy-binding is incompatible");
    if (_fixedBase)
        RequireOnlyDependencies(dynSection.DynamicSegmentData);
    const std::uint64_t base = _fixedBase.value_or(0);
    const auto address = [this, base](const std::uint64_t vaddr) {
        return _fixedBase ? Rebased(base, vaddr, "Address") : vaddr;
    };
    const auto table = [this, base](const std::vector<std::uint8_t>& data, const bool relocations) {
        if (!_fixedBase) return data;
        return relocations ? RebasedRelocations(data, base) : RebasedSymbols(data, base);
    };

    std::vector<std::uint8_t> buf = std::move(sourceElf);

    const std::uint32_t type = _fixedBase ? ET_EXEC : ET_DYN;
    buf[kEhdrOsAbiOffset] = 0;
    buf[kEhdrAbiVersionOffset] = 0;
    buf[kEhdrTypeOffset] = static_cast<std::uint8_t>(type & 0xFF);
    buf[kEhdrTypeOffset + 1] = static_cast<std::uint8_t>((type >> 8) & 0xFF);
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
    const auto symbols = table(dynSection.DynSymData, false);
    for (std::uint8_t b : symbols)
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
    const auto relocations = table(dynSection.RelaData, true);
    for (std::uint8_t b : relocations)
        buf.push_back(b);
    alignBuf(buf, kRelaAlignment);

    const auto relaPltOff = static_cast<std::uint64_t>(buf.size());
    const auto pltRelocations = table(dynSection.RelaPltData, true);
    for (std::uint8_t b : pltRelocations)
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
    _appendDynEntry(dynSegBuf, DT_STRTAB, address(vaddrOfExtraBlockOffset(dynStrOff)));
    _appendDynEntry(dynSegBuf, DT_STRSZ, dynStrSize);
    _appendDynEntry(dynSegBuf, DT_SYMTAB, address(vaddrOfExtraBlockOffset(dynSymOff)));
    _appendDynEntry(dynSegBuf, DT_SYMENT, kSymEntrySize);
    _appendDynEntry(dynSegBuf, DT_HASH, address(vaddrOfExtraBlockOffset(hashOff)));
    _appendDynEntry(dynSegBuf, DT_DEBUG, 0);
    if (!dynSection.RelaData.empty()) {
        _appendDynEntry(dynSegBuf, DT_RELA, address(vaddrOfExtraBlockOffset(relaOff)));
        _appendDynEntry(dynSegBuf, DT_RELASZ, dynSection.RelaData.size());
        _appendDynEntry(dynSegBuf, DT_RELAENT, kRelaEntrySize);
    }
    if (!dynSection.RelaPltData.empty()) {
        _appendDynEntry(dynSegBuf, DT_JMPREL, address(vaddrOfExtraBlockOffset(relaPltOff)));
        _appendDynEntry(dynSegBuf, DT_PLTRELSZ, dynSection.RelaPltData.size());
        _appendDynEntry(dynSegBuf, DT_PLTREL, static_cast<std::uint64_t>(DT_RELA));
        _appendDynEntry(dynSegBuf, DT_PLTGOT, address(originalPltGotVaddr));
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
    const auto stubBytes = _entryStubBuilder->BuildEntryStub(address(stubVaddr), address(realEntryVaddr));
    for (std::uint8_t b : stubBytes)
        buf.push_back(b);
    _byteWriter->WriteU64(buf, kEhdrEntryOffset, address(stubVaddr));

    for (const auto& site : trampolines)
        _appendTrampoline(buf, site, extraBlockOff, vaddrOfExtraBlockOffset);

    static constexpr char kInterp[] = "/lib64/ld-linux-x86-64.so.2";
    const auto interpOff = static_cast<std::uint64_t>(buf.size());
    constexpr std::uint64_t interpSize = sizeof(kInterp);
    for (char i : kInterp)
        buf.push_back(static_cast<std::uint8_t>(i));

    std::uint64_t tableOff = phOff;
    std::uint16_t tableSlots = phNum;
    if (const auto required = _programHeaderLayoutBuilder->RequiredProgramHeaderCount(originalHeaders); required > phNum && required <= std::numeric_limits<std::uint16_t>::max()) {
        alignBuf(buf, kPhdrHeaderAlignment);
        tableOff = static_cast<std::uint64_t>(buf.size());
        tableSlots = static_cast<std::uint16_t>(required);
        buf.resize(buf.size() + required * phEntSize);
        _byteWriter->WriteU64(buf, kEhdrPhOffOffset, tableOff);
    }

    const std::uint64_t extraBlockSize = static_cast<std::uint64_t>(buf.size()) - extraBlockOff;

    ProgramHeaderLayoutRequest layoutRequest{};
    layoutRequest.PhOff = tableOff;
    layoutRequest.PhEntSize = phEntSize;
    layoutRequest.PhNum = tableSlots;
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
    if (_fixedBase)
        RebaseProgramHeaders(buf, tableOff, phEntSize, writtenPh, base);

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
