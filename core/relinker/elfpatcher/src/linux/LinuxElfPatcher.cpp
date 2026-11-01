#include <elfpatcher/linux/LinuxElfPatcher.hpp>
#include <elfpatcher/general/AddressDistance.hpp>
#include <elfpatcher/general/ElfConstants.hpp>
#include <elfpatcher/general/ProgramHeaderLayoutRequest.hpp>
#include <elfpatcher/general/SectionHeaderTableRequest.hpp>
#include <codegen/x86/Amd64OnlySubstitutionTable.hpp>
#include <codegen/x86/StubBodyBuilder.hpp>
#include <domain/ImportModule.hpp>
#include <io/BufferUtils.hpp>
#include <nid/NidCompute.hpp>
#include <algorithm>
#include <limits>
#include <map>
#include <set>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

namespace Elfpatcher::Linux {

namespace {

constexpr std::uint64_t kLinuxUserAddressEnd = 0x800000000000;
constexpr std::uint64_t kLazyPltGotBytes = 24;
constexpr std::uint64_t kPltGotEntryBytes = 8;

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
        case R_X86_64_GLOB_DAT:
        case R_X86_64_JUMP_SLOT:
        case R_X86_64_32:
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

void RequireZeroImportSlotAddends(const std::vector<std::uint8_t>& table) {
    for (std::size_t entry = 0; entry < table.size(); entry += kRelaEntrySize) {
        const auto type = static_cast<std::uint32_t>(Io::ReadU64(table, entry + 8));
        if ((type == R_X86_64_GLOB_DAT || type == R_X86_64_JUMP_SLOT) && Io::ReadU64(table, entry + 16) != 0)
            throw Domain::RelinkerException("GLOB_DAT/JUMP_SLOT relocation has a nonzero addend", Io::ReadU64(table, entry));
    }
}

void RequireFileRange(const std::vector<std::uint8_t>& buf, const Domain::ProgramHeader& load) {
    if (load.Offset > buf.size() || load.FileSize > buf.size() - load.Offset)
        throw Domain::RelinkerException("PT_LOAD at " + Hex(load.MappedAddress) + " has file bytes outside the input");
}

void RequireLoaded(const std::vector<Domain::ProgramHeader>& headers, const std::uint64_t address, const std::uint64_t size, const std::uint32_t flags, const std::string& what) {
    for (const auto& header : headers)
        if (header.Type == PT_LOAD && (header.Flags == 0 || (header.Flags & flags) == flags) && address >= header.MappedAddress && address - header.MappedAddress < header.MemorySize && size <= header.MemorySize - (address - header.MappedAddress))
            return;
    const auto range = size > 1 ? " (" + std::to_string(size) + " bytes) does not fit inside " : std::string(" is not inside ");
    throw Domain::RelinkerException(what + " " + Hex(address) + range + ((flags & PF_X) != 0 ? "an executable" : "a") + " PT_LOAD segment");
}

std::uint64_t RelocationWidth(const std::vector<std::uint8_t>& relocations, const std::size_t entry) {
    const auto type = static_cast<std::uint32_t>(Io::ReadU64(relocations, entry + 8));
    return type == R_X86_64_PC32 || type == R_X86_64_32 ? 4 : 8;
}

void SeparateSharedLoadPages(std::vector<std::uint8_t>& buf, std::vector<Domain::ProgramHeader>& headers) {
    std::vector<std::size_t> loads;
    for (std::size_t index = 0; index < headers.size(); ++index) {
        if (headers[index].Type == PT_LOAD && headers[index].MemorySize != 0) {
            RequireFileRange(buf, headers[index]);
            loads.push_back(index);
        }
    }
    std::stable_sort(loads.begin(), loads.end(), [&headers](const std::size_t left, const std::size_t right) {
        return headers[left].MappedAddress < headers[right].MappedAddress;
    });
    for (std::size_t position = 1; position < loads.size(); ++position) {
        auto& previous = headers[loads[position - 1]];
        const auto& current = headers[loads[position]];
        if (current.MappedAddress - previous.MappedAddress >= previous.MemorySize)
            continue;
        const auto previousEnd = previous.MappedAddress + previous.MemorySize;
        if (current.MappedAddress == previous.MappedAddress || previousEnd > current.MappedAddress + current.MemorySize)
            throw Domain::RelinkerException("Overlapping PT_LOAD memory ranges", current.MappedAddress);
        if (((previous.Flags | current.Flags) & (PF_W | PF_X)) == (PF_W | PF_X)
            && current.MappedAddress % kDefaultLoadAlignment != 0)
            throw Domain::RelinkerException("Overlapping PT_LOAD memory ranges", current.MappedAddress);
        const auto previousFileEnd = previous.MappedAddress + previous.FileSize;
        const auto currentFileEnd = current.MappedAddress + current.FileSize;
        const auto compareEnd = std::min(previousEnd, std::max(previousFileEnd, currentFileEnd));
        for (auto address = current.MappedAddress; address < compareEnd;) {
            const bool previousBacked = address < previousFileEnd;
            const bool currentBacked = address < currentFileEnd;
            auto runEnd = compareEnd;
            if (previousBacked)
                runEnd = std::min(runEnd, previousFileEnd);
            if (currentBacked)
                runEnd = std::min(runEnd, currentFileEnd);
            const auto length = static_cast<std::size_t>(runEnd - address);
            const auto previousAt = static_cast<std::size_t>(previous.Offset + (address - previous.MappedAddress));
            const auto currentAt = static_cast<std::size_t>(current.Offset + (address - current.MappedAddress));
            const bool congruent = previousBacked && currentBacked
                ? std::equal(buf.begin() + previousAt, buf.begin() + previousAt + length, buf.begin() + currentAt)
                : previousBacked
                    ? std::all_of(buf.begin() + previousAt, buf.begin() + previousAt + length, [](const std::uint8_t byte) { return byte == 0; })
                    : std::all_of(buf.begin() + currentAt, buf.begin() + currentAt + length, [](const std::uint8_t byte) { return byte == 0; });
            if (!congruent)
                throw Domain::RelinkerException("Overlapping PT_LOAD memory ranges", current.MappedAddress);
            address = runEnd;
        }
        previous.MemorySize = current.MappedAddress - previous.MappedAddress;
        if (previous.FileSize > previous.MemorySize)
            previous.FileSize = previous.MemorySize;
    }
    for (std::size_t position = 0; position < loads.size(); ++position) {
        auto& load = headers[loads[position]];
        const auto prefix = load.MappedAddress % kDefaultLoadAlignment;
        if (prefix == 0 || load.FileSize == 0 || load.Offset % kDefaultLoadAlignment != prefix)
            continue;
        const auto pageStart = load.MappedAddress - prefix;
        std::vector<std::uint8_t> image(static_cast<std::size_t>(prefix), 0);
        std::vector<bool> covered(static_cast<std::size_t>(prefix), false);
        for (std::size_t below = 0; below < position; ++below) {
            const auto& other = headers[loads[below]];
            const auto end = std::min(other.MappedAddress + other.MemorySize, load.MappedAddress);
            for (auto address = std::max(other.MappedAddress, pageStart); address < end; ++address) {
                const auto relative = address - other.MappedAddress;
                covered[static_cast<std::size_t>(address - pageStart)] = true;
                image[static_cast<std::size_t>(address - pageStart)] = relative < other.FileSize ? buf[static_cast<std::size_t>(other.Offset + relative)] : 0;
            }
        }
        const auto mapped = load.Offset - prefix;
        bool differs = false;
        for (std::size_t index = 0; index < image.size() && !differs; ++index)
            differs = covered[index] && buf[static_cast<std::size_t>(mapped) + index] != image[index];
        if (!differs)
            continue;
        const auto alignment = std::max<std::uint64_t>(load.Alignment, kDefaultLoadAlignment);
        while ((buf.size() + prefix) % alignment != load.MappedAddress % alignment)
            buf.push_back(0);
        buf.insert(buf.end(), image.begin(), image.end());
        const auto offset = static_cast<std::uint64_t>(buf.size());
        buf.resize(static_cast<std::size_t>(offset + load.FileSize));
        std::copy_n(buf.begin() + static_cast<std::ptrdiff_t>(load.Offset), static_cast<std::size_t>(load.FileSize), buf.begin() + static_cast<std::ptrdiff_t>(offset));
        load.Offset = offset;
    }
}

struct TracedImport {
    std::uint64_t Slot;
    std::string Name;
    std::string Library;
};

std::string ReadDynamicString(const std::vector<std::uint8_t>& strings, const std::uint32_t offset) {
    if (offset >= strings.size())
        throw Domain::RelinkerException("Import symbol name lies outside the dynamic string table");
    std::string value;
    for (std::size_t index = offset; index < strings.size() && strings[index] != 0; ++index)
        value.push_back(static_cast<char>(strings[index]));
    return value;
}

std::vector<TracedImport> TracedImports(const Domain::SysVDynamicSection& dynSection) {
    std::vector<TracedImport> imports;
    const auto collect = [&](const std::vector<std::uint8_t>& table) {
        RequireWholeEntries(table, kRelaEntrySize, "Relocation table");
        for (std::size_t entry = 0; entry < table.size(); entry += kRelaEntrySize) {
            const auto info = Io::ReadU64(table, entry + 8);
            const auto type = static_cast<std::uint32_t>(info);
            const auto symbol = info >> 32;
            const auto target = Io::ReadU64(table, entry);
            if ((type != R_X86_64_GLOB_DAT && type != R_X86_64_JUMP_SLOT) || symbol == 0)
                continue;
            if (type == R_X86_64_GLOB_DAT && !dynSection.FunctionImports.contains(target))
                continue;
            if (symbol >= dynSection.DynSymData.size() / kSymEntrySize)
                throw Domain::RelinkerException("Invalid import symbol index", target);
            const auto symbolOffset = static_cast<std::size_t>(symbol * kSymEntrySize);
            if (Io::ReadU16(dynSection.DynSymData, symbolOffset + 6) != SHN_UNDEF || dynSection.DynSymData[symbolOffset + 4] >> 4 == STB_WEAK)
                continue;
            auto name = ReadDynamicString(dynSection.DynStrData, Io::ReadU32(dynSection.DynSymData, symbolOffset));
            if (name.empty())
                continue;
            const auto library = dynSection.ImportModules.find(target);
            imports.push_back({target, std::move(name), library == dynSection.ImportModules.end() ? std::string{} : library->second});
        }
    };
    collect(dynSection.RelaData);
    collect(dynSection.RelaPltData);
    return imports;
}

Domain::SysVDynamicSection ImportsByNid(Domain::SysVDynamicSection dynSection) {
    RequireWholeEntries(dynSection.DynamicSegmentData, kDynEntrySize, "Executable dependency table");
    for (std::size_t entry = 0; entry < dynSection.DynamicSegmentData.size(); entry += kDynEntrySize) {
        if (static_cast<std::int64_t>(Io::ReadU64(dynSection.DynamicSegmentData, entry)) != DT_NEEDED)
            continue;
        const auto offset = Io::ReadU64(dynSection.DynamicSegmentData, entry + 8);
        if (offset >= dynSection.DynStrData.size())
            throw Domain::RelinkerException("DT_NEEDED string offset is out of bounds", offset);
        const auto name = ReadDynamicString(dynSection.DynStrData, static_cast<std::uint32_t>(offset));
        const auto library = Domain::NeededLibraryName(name);
        if (library == name)
            continue;
        Io::WriteU64(dynSection.DynamicSegmentData, entry + 8, dynSection.DynStrData.size());
        Io::AppendString(dynSection.DynStrData, library);
    }
    for (auto& library : dynSection.NeededLibraries)
        library = Domain::NeededLibraryName(library);

    std::set<std::uint64_t> symbols;
    for (const auto* table : {&dynSection.RelaData, &dynSection.RelaPltData}) {
        RequireWholeEntries(*table, kRelaEntrySize, "Relocation table");
        for (std::size_t entry = 0; entry < table->size(); entry += kRelaEntrySize)
            if (dynSection.NamedImports.contains(Io::ReadU64(*table, entry)))
                symbols.insert(Io::ReadU64(*table, entry + 8) >> 32);
    }
    for (const auto symbol : symbols) {
        if (symbol >= dynSection.DynSymData.size() / kSymEntrySize)
            throw Domain::RelinkerException("Invalid import symbol index", symbol);
        const auto symbolOffset = static_cast<std::size_t>(symbol * kSymEntrySize);
        const auto name = ReadDynamicString(dynSection.DynStrData, Io::ReadU32(dynSection.DynSymData, symbolOffset));
        if (name.empty() || name.find('#') != std::string::npos)
            continue;
        if (dynSection.DynStrData.size() > std::numeric_limits<std::uint32_t>::max())
            throw Domain::RelinkerException("Dynamic string table too large");
        Io::WriteU32(dynSection.DynSymData, symbolOffset, static_cast<std::uint32_t>(dynSection.DynStrData.size()));
        Io::AppendString(dynSection.DynStrData, Nid::ComputeNid(name, ""));
    }
    return dynSection;
}

std::vector<std::uint8_t> TracePrologue(const std::uint64_t vaddr, const std::uint64_t table, const std::uint64_t tableEnd, const std::uint64_t function) {
    std::vector<std::uint8_t> code = {0x52, 0x53, 0x41, 0x54, 0x48, 0x83, 0xec, 0x08};
    const auto rip = [&](std::initializer_list<std::uint8_t> opcode, const std::uint64_t target) {
        code.insert(code.end(), opcode);
        std::int64_t displacement = 0;
        if (!SignedDistance(target, vaddr + code.size() + 4, displacement) || displacement < std::numeric_limits<std::int32_t>::min() || displacement > std::numeric_limits<std::int32_t>::max())
            throw Domain::RelinkerException("Import trace table exceeds rel32 range");
        Io::AppendU32(code, static_cast<std::uint32_t>(displacement));
    };
    rip({0x48, 0x8d, 0x1d}, table);
    rip({0x4c, 0x8d, 0x25}, tableEnd);
    const auto loop = code.size();
    code.insert(code.end(), {0x4c, 0x39, 0xe3, 0x73, 0x00});
    const auto exit = code.size();
    code.insert(code.end(), {0x48, 0x8b, 0x03, 0x48, 0x8b, 0x3c, 0x03, 0x48, 0x8b, 0x73, 0x08, 0x48, 0x01, 0xde, 0x48, 0x8b, 0x53, 0x10, 0x48, 0x01, 0xda});
    rip({0xff, 0x15}, function);
    code.insert(code.end(), {0x48, 0x8b, 0x3b, 0x48, 0x01, 0xdf, 0x48, 0x81, 0xe7, 0x00, 0xf0, 0xff, 0xff, 0xbe, 0x00, 0x10, 0x00, 0x00, 0xba, 0x07, 0x00, 0x00, 0x00, 0x50, 0xb8, 0x0a, 0x00, 0x00, 0x00, 0x0f, 0x05, 0x58});
    code.insert(code.end(), {0x48, 0x8b, 0x0b, 0x48, 0x89, 0x04, 0x0b, 0x48, 0x83, 0xc3, 0x18, 0xeb});
    code.push_back(static_cast<std::uint8_t>(static_cast<std::int8_t>(static_cast<std::int64_t>(loop) - static_cast<std::int64_t>(code.size() + 1))));
    code[exit - 1] = static_cast<std::uint8_t>(code.size() - exit);
    code.insert(code.end(), {0x48, 0x83, 0xc4, 0x08, 0x41, 0x5c, 0x5b, 0x5a});
    return code;
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
    std::optional<std::uint64_t> fixedBase,
    bool traceImports
)
    : _entryStubBuilder(std::move(entryStubBuilder))
    , _programHeaderLayoutBuilder(std::move(programHeaderLayoutBuilder))
    , _sectionHeaderTableBuilder(std::move(sectionHeaderTableBuilder))
    , _byteWriter(std::move(byteWriter))
    , _fixedBase(fixedBase)
    , _traceImports(traceImports)
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
    std::int64_t returnDisplacement = 0, jumpDisplacement = 0;
    if (!SignedDistance(site.Address + site.Length, bodyVaddr + site.ReturnBranchOffset + kJmpRel32.Size, returnDisplacement) || !SignedDistance(bodyVaddr, site.Address + kJmpRel32.Size, jumpDisplacement) || !inRange(returnDisplacement) || !inRange(jumpDisplacement))
        throw Domain::RelinkerException("AMD-only stub exceeds rel32 range", site.Offset);
    _byteWriter->WriteU32(buf, static_cast<std::size_t>(bodyOff + site.ReturnBranchOffset + 1), static_cast<std::uint32_t>(returnDisplacement));

    std::fill_n(buf.begin() + static_cast<std::ptrdiff_t>(site.Offset), site.Length, kNop1.Bytes[0]);
    buf[static_cast<std::size_t>(site.Offset)] = kJmpRel32.Bytes[0];
    _byteWriter->WriteU32(buf, static_cast<std::size_t>(site.Offset + 1), static_cast<std::uint32_t>(jumpDisplacement));
}

std::vector<std::uint8_t> LinuxElfPatcher::Patch(
    std::vector<std::uint8_t> sourceElf,
    const std::vector<Domain::ProgramHeader>& originalHeaders,
    const Domain::SysVDynamicSection& sourceDynSection,
    const std::uint64_t originalPltGotVaddr,
    const std::string& runPath,
    const bool lazyBinding,
    const bool dependencyDiagnostics,
    const std::vector<Codegen::TrampolineSite>& trampolines)
{
    const auto dynSection = ImportsByNid(sourceDynSection);
    if (runPath.empty() || runPath.front() == ':' || runPath.back() == ':' || runPath.find("::") != std::string::npos)
        throw Domain::RelinkerException("Empty Linux run path component is not supported");
    if (dependencyDiagnostics)
        throw Domain::RelinkerException("Linux target does not support --windows-diagnostics");
    if (_fixedBase && lazyBinding)
        throw Domain::RelinkerException("A fixed Linux base requires eager binding; --lazy-binding is incompatible");
    if (_traceImports && lazyBinding)
        throw Domain::RelinkerException("--trace-imports conflicts with --lazy-binding");
    if (_traceImports && std::find(dynSection.NeededLibraries.begin(), dynSection.NeededLibraries.end(), "libc.prx") == dynSection.NeededLibraries.end())
        throw Domain::RelinkerException("--trace-imports requires libc.prx among the libraries");
    RequireZeroImportSlotAddends(dynSection.RelaData);
    RequireZeroImportSlotAddends(dynSection.RelaPltData);
    if (_fixedBase)
        RequireOnlyDependencies(dynSection.DynamicSegmentData);
    RequireLoaded(originalHeaders, Io::ReadU64(sourceElf, kEhdrEntryOffset), 1, PF_X, "e_entry");
    for (const auto& [relocationTable, name] : {std::pair{&dynSection.RelaData, "RELA r_offset"}, std::pair{&dynSection.RelaPltData, "JMPREL r_offset"}})
        for (std::size_t entry = 0; entry < relocationTable->size(); entry += kRelaEntrySize)
            RequireLoaded(originalHeaders, Io::ReadU64(*relocationTable, entry), RelocationWidth(*relocationTable, entry), 0, name);
    if (!dynSection.RelaPltData.empty())
        RequireLoaded(originalHeaders, originalPltGotVaddr, lazyBinding ? kLazyPltGotBytes : kPltGotEntryBytes, 0, "DT_PLTGOT");
    const auto traced = _traceImports ? TracedImports(dynSection) : std::vector<TracedImport>{};
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
    std::fill_n(buf.begin() + static_cast<std::ptrdiff_t>(kEhdrOsAbiOffset), 9, std::uint8_t{0});
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
    const std::uint64_t traceSymbolStrOff = static_cast<std::uint64_t>(buf.size()) - dynStrOff;
    if (_traceImports) {
        for (char c : std::string_view("TraceImport_nid_no_patch"))
            buf.push_back(static_cast<std::uint8_t>(c));
        buf.push_back(0);
    }
    const std::uint64_t dynStrSize = static_cast<std::uint64_t>(buf.size()) - dynStrOff;
    alignBuf(buf, kDynStrAlignment);

    const auto dynSymOff = static_cast<std::uint64_t>(buf.size());
    auto symbols = table(dynSection.DynSymData, false);
    const auto traceSymbol = symbols.size() / kSymEntrySize;
    if (_traceImports) {
        Io::AppendU32(symbols, static_cast<std::uint32_t>(traceSymbolStrOff));
        symbols.push_back(static_cast<std::uint8_t>((STB_GLOBAL << 4) | STT_FUNC));
        symbols.push_back(0);
        Io::AppendU16(symbols, SHN_UNDEF);
        Io::AppendU64(symbols, 0);
        Io::AppendU64(symbols, 0);
    }
    for (std::uint8_t b : symbols)
        buf.push_back(b);
    alignBuf(buf, kDynSymAlignment);

    const auto hashOff = static_cast<std::uint64_t>(buf.size());
    const auto symbolCount = static_cast<std::uint32_t>(symbols.size() / kSymEntrySize);
    Io::AppendU32(buf, 1);
    Io::AppendU32(buf, symbolCount);
    Io::AppendU32(buf, symbolCount > 1 ? 1 : 0);
    for (std::uint32_t index = 0; index < symbolCount; ++index)
        Io::AppendU32(buf, index != 0 && index + 1 < symbolCount ? index + 1 : 0);
    alignBuf(buf, kDynSymAlignment);

    const auto relaOff = static_cast<std::uint64_t>(buf.size());
    auto relocations = table(dynSection.RelaData, true);
    const auto traceRelocation = relocations.size();
    if (_traceImports) {
        Io::AppendU64(relocations, 0);
        Io::AppendU64(relocations, (static_cast<std::uint64_t>(traceSymbol) << 32) | R_X86_64_GLOB_DAT);
        Io::AppendU64(relocations, 0);
    }
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
    if (!relocations.empty()) {
        _appendDynEntry(dynSegBuf, DT_RELA, address(vaddrOfExtraBlockOffset(relaOff)));
        _appendDynEntry(dynSegBuf, DT_RELASZ, relocations.size());
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

    std::uint64_t traceFunctionVaddr = 0;
    std::uint64_t traceTableVaddr = 0;
    std::uint64_t traceTableEndVaddr = 0;
    std::vector<std::size_t> traceEntries;
    if (_traceImports) {
        alignBuf(buf, 8);
        const auto functionOff = static_cast<std::uint64_t>(buf.size());
        traceFunctionVaddr = vaddrOfExtraBlockOffset(functionOff);
        Io::AppendU64(buf, 0);
        Io::WriteU64(buf, static_cast<std::size_t>(relaOff + traceRelocation), address(traceFunctionVaddr));
        traceTableVaddr = vaddrOfExtraBlockOffset(buf.size());
        for (std::size_t index = 0; index < traced.size(); ++index) {
            traceEntries.push_back(buf.size());
            buf.resize(buf.size() + 24);
        }
        traceTableEndVaddr = vaddrOfExtraBlockOffset(buf.size());
        std::map<std::string, std::uint64_t> strings;
        const auto addString = [&](const std::string& value) {
            if (const auto existing = strings.find(value); existing != strings.end()) return existing->second;
            const auto vaddr = vaddrOfExtraBlockOffset(buf.size());
            for (char c : value)
                buf.push_back(static_cast<std::uint8_t>(c));
            buf.push_back(0);
            strings.emplace(value, vaddr);
            return vaddr;
        };
        for (std::size_t index = 0; index < traced.size(); ++index) {
            const auto entryVaddr = vaddrOfExtraBlockOffset(traceEntries[index]);
            const auto relative = [entryVaddr](const std::uint64_t vaddr) { return vaddr - entryVaddr; };
            Io::WriteU64(buf, traceEntries[index], relative(traced[index].Slot));
            Io::WriteU64(buf, traceEntries[index] + 8, relative(addString(traced[index].Name)));
            Io::WriteU64(buf, traceEntries[index] + 16, relative(addString(traced[index].Library)));
        }
    }

    const std::uint64_t realEntryVaddr = *reinterpret_cast<const std::uint64_t*>(buf.data() + kEhdrEntryOffset);
    const auto stubOff = static_cast<std::uint64_t>(buf.size());
    const auto stubVaddr = vaddrOfExtraBlockOffset(stubOff);
    if (_traceImports) {
        for (std::uint8_t b : TracePrologue(stubVaddr, traceTableVaddr, traceTableEndVaddr, traceFunctionVaddr))
            buf.push_back(b);
    }
    const auto entryStubVaddr = vaddrOfExtraBlockOffset(buf.size());
    for (std::uint8_t b : _entryStubBuilder->BuildEntryStub(address(entryStubVaddr), address(realEntryVaddr)))
        buf.push_back(b);
    const auto stubSize = static_cast<std::uint64_t>(buf.size()) - stubOff;
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

    auto headers = originalHeaders;
    SeparateSharedLoadPages(buf, headers);

    ProgramHeaderLayoutRequest layoutRequest{};
    layoutRequest.PhOff = tableOff;
    layoutRequest.PhEntSize = phEntSize;
    layoutRequest.PhNum = tableSlots;
    layoutRequest.OriginalHeaders = headers;
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
    sectionRequest.DynSymSize = symbols.size();
    sectionRequest.DynamicSegmentOffset = dynSegOff;
    sectionRequest.DynamicSegmentSize = dynSegBuf.size();
    sectionRequest.StubOffset = stubOff;
    sectionRequest.StubSize = stubSize;

    _sectionHeaderTableBuilder->WriteTable(buf, sectionRequest);

    return buf;
}

}
