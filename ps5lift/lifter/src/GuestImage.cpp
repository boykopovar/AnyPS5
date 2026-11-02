#include <lifter/GuestImage.hpp>
#include <lifter/Hex.hpp>

#include <relinker/analysis/UnusedNidFilter/EhFrameReader.hpp>
#include <relinker/parsing/ElfReader.hpp>

#include <algorithm>
#include <cstring>
#include <fstream>
#include <iterator>
#include <stdexcept>

namespace Ps5Lift {

namespace {

constexpr std::uint16_t ElfExecutable = 2;
constexpr std::uint16_t ElfShared = 3;
constexpr std::uint32_t ProgramLoad = 1;
constexpr std::uint32_t ProgramDynamic = 2;
constexpr std::uint32_t ProgramProgramHeaders = 6;
constexpr std::uint32_t ProgramTls = 7;
constexpr std::uint32_t SectionSymbolTable = 2;
constexpr std::uint32_t SectionDynamicSymbols = 11;
constexpr std::uint8_t SymbolObject = 1;
constexpr std::uint8_t SymbolFunction = 2;
constexpr std::uint8_t SymbolTls = 6;
constexpr std::uint8_t SymbolIndirectFunction = 10;
constexpr std::uint16_t SectionAbsolute = 0xfff1;
constexpr std::int64_t DynamicNeeded = 1;
constexpr std::int64_t DynamicHash = 4;
constexpr std::int64_t DynamicStringTable = 5;
constexpr std::int64_t DynamicSymbolTable = 6;
constexpr std::int64_t DynamicRela = 7;
constexpr std::int64_t DynamicRelaSize = 8;
constexpr std::int64_t DynamicRelaEntry = 9;
constexpr std::int64_t DynamicSymbolEntry = 11;
constexpr std::int64_t DynamicRel = 17;
constexpr std::int64_t DynamicPltRel = 20;
constexpr std::int64_t DynamicRelrSize = 35;
constexpr std::int64_t DynamicRelr = 36;
constexpr std::int64_t DynamicRelrEntry = 37;
constexpr std::int64_t DynamicInit = 12;
constexpr std::int64_t DynamicInitArray = 25;
constexpr std::int64_t DynamicInitArraySize = 27;
constexpr std::int64_t DynamicJumpRelocations = 23;
constexpr std::int64_t DynamicJumpRelocationsSize = 2;
constexpr std::uint32_t RelocationNone = 0;
constexpr std::uint32_t RelocationAbsolute = 1;
constexpr std::uint32_t RelocationGlobalData = 6;
constexpr std::uint32_t RelocationJumpSlot = 7;
constexpr std::uint32_t RelocationRelative = 8;
constexpr std::uint32_t RelocationTlsModule = 16;
constexpr std::uint32_t RelocationTlsOffset = 17;
constexpr std::uint32_t RelocationThreadPointerOffset = 18;
constexpr std::uint64_t ImportStride = 16;
constexpr std::uint64_t ImportAlignment = 0x10000;

std::uint64_t AlignUp(const std::uint64_t value, const std::uint64_t alignment) {
    return (value + alignment - 1) & ~(alignment - 1);
}

bool Within(const std::uint64_t offset, const std::uint64_t size, const std::uint64_t limit) {
    return offset <= limit && size <= limit - offset;
}

template <typename T>
T ReadAt(const std::vector<std::uint8_t>& bytes, std::uint64_t offset) {
    if (!Within(offset, sizeof(T), bytes.size())) throw std::runtime_error("ELF read out of bounds at " + std::to_string(offset));
    T value;
    std::memcpy(&value, bytes.data() + offset, sizeof(T));
    return value;
}

std::string ReadString(const std::vector<std::uint8_t>& bytes, std::uint64_t offset) {
    if (offset >= bytes.size()) throw std::runtime_error("ELF string out of bounds at " + std::to_string(offset));
    const auto* begin = reinterpret_cast<const char*>(bytes.data() + offset);
    return std::string(begin, strnlen(begin, bytes.size() - offset));
}

struct Symbol {
    std::string Name;
    std::uint8_t Type;
    std::uint8_t Binding;
    std::uint8_t Visibility;
    std::uint16_t Section;
    std::uint64_t Value;
};

Symbol ReadSymbol(const std::vector<std::uint8_t>& bytes, const std::uint64_t offset, const std::uint64_t strings) {
    Symbol symbol;
    symbol.Name = ReadString(bytes, strings + ReadAt<std::uint32_t>(bytes, offset));
    symbol.Type = ReadAt<std::uint8_t>(bytes, offset + 4) & 0xf;
    symbol.Binding = ReadAt<std::uint8_t>(bytes, offset + 4) >> 4;
    symbol.Visibility = ReadAt<std::uint8_t>(bytes, offset + 5) & 3;
    symbol.Section = ReadAt<std::uint16_t>(bytes, offset + 6);
    symbol.Value = ReadAt<std::uint64_t>(bytes, offset + 8);
    return symbol;
}

struct Relocation {
    std::uint64_t Offset;
    std::uint32_t Type;
    std::uint32_t Symbol;
    std::int64_t Addend;
};

std::uint64_t MappedProgramHeaders(const std::string& path, const Domain::ElfHeader& header, const std::vector<Domain::ProgramHeader>& programs) {
    for (const auto& program : programs) {
        if (program.Type == ProgramProgramHeaders) return program.MappedAddress;
    }
    const std::uint64_t size = std::uint64_t{header.ProgramHeaderEntrySize} * programs.size();
    for (const auto& program : programs) {
        if (program.Type == ProgramLoad && header.ProgramHeaderOffset >= program.Offset && Within(header.ProgramHeaderOffset - program.Offset, size, program.FileSize))
            return program.MappedAddress + (header.ProgramHeaderOffset - program.Offset);
    }
    throw std::runtime_error("No loadable segment of " + path + " maps its program headers");
}

std::uint64_t Address(const Symbol& symbol, const std::uint64_t bias) {
    return symbol.Section == SectionAbsolute ? symbol.Value : symbol.Value + bias;
}

bool Exported(const Symbol& symbol) {
    constexpr std::uint8_t Hidden = 2;
    constexpr std::uint8_t Internal = 1;
    return symbol.Section != 0 && symbol.Type != SymbolTls && symbol.Binding != 0 && symbol.Visibility != Hidden && symbol.Visibility != Internal && !symbol.Name.empty();
}

std::vector<std::uint8_t> ReadFile(const std::string& path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) throw std::runtime_error("Cannot open " + path);
    return std::vector<std::uint8_t>(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
}

std::uint64_t LoadBias(const std::string& path, const Domain::ElfHeader& header, const std::vector<Domain::ProgramHeader>& programs, const std::optional<std::uint64_t> base) {
    if (header.Type != ElfExecutable && header.Type != ElfShared) throw std::runtime_error(path + " is neither an executable nor a shared object");
    if (header.Type == ElfExecutable && base) throw std::runtime_error(path + " is a fixed-address executable, which cannot be moved");
    if (header.Type == ElfShared && !base) throw std::runtime_error(path + " is position-independent and needs a base");
    if (!base) return 0;
    std::uint64_t lowest = ~std::uint64_t{0};
    for (const auto& program : programs) {
        if (program.Type == ProgramLoad && program.MemorySize != 0) lowest = std::min(lowest, program.MappedAddress);
    }
    return *base - lowest;
}

struct ElfFile {
    ElfFile(const std::string& path, std::optional<std::uint64_t> base);

    std::string Path;
    Relinker::ElfReader Reader;
    Domain::ElfHeader Header;
    std::vector<Domain::ProgramHeader> Programs;
    std::vector<Domain::SectionHeader> Sections;
    std::uint64_t Bias;
    std::map<std::int64_t, std::uint64_t> Tags;
    std::vector<std::string> Needed;

    const std::vector<std::uint8_t>& Bytes() const {
        return Reader.GetRawBytes();
    }

    bool Has(const std::int64_t tag) const {
        return Tags.contains(tag);
    }

    std::uint64_t FileOffset(const std::int64_t tag) const {
        return Reader.TranslateVirtualAddress(Tags.at(tag));
    }

    Symbol DynamicSymbol(const std::uint32_t index) const {
        if (!Has(DynamicSymbolTable) || !Has(DynamicStringTable)) throw std::runtime_error("Relocations name symbols but the dynamic segment has no symbol table");
        return ReadSymbol(Bytes(), FileOffset(DynamicSymbolTable) + index * std::uint64_t{24}, FileOffset(DynamicStringTable));
    }
};

void CheckDynamicTags(const ElfFile& elf) {
    const auto requireTag = [&](const std::int64_t tag, const std::int64_t with, const char* what) {
        if (elf.Has(with) && !elf.Has(tag)) throw std::runtime_error(elf.Path + " has " + what + " without its size or entry size");
    };
    const auto requireValue = [&](const std::int64_t tag, const std::uint64_t value, const char* what) {
        if (elf.Has(tag) && elf.Tags.at(tag) != value) throw std::runtime_error(elf.Path + " has " + what + " of " + std::to_string(elf.Tags.at(tag)) + ", not " + std::to_string(value));
    };
    if (elf.Has(DynamicRel)) throw std::runtime_error(elf.Path + " has REL relocations, which x86-64 images do not use");
    if (elf.Has(DynamicNeeded) && !elf.Has(DynamicStringTable)) throw std::runtime_error(elf.Path + " has DT_NEEDED without the DT_STRTAB that names it");
    requireTag(DynamicInitArraySize, DynamicInitArray, "DT_INIT_ARRAY");
    requireTag(DynamicSymbolEntry, DynamicSymbolTable, "DT_SYMTAB");
    requireTag(DynamicRelaSize, DynamicRela, "DT_RELA");
    requireTag(DynamicRelaEntry, DynamicRela, "DT_RELA");
    requireTag(DynamicJumpRelocationsSize, DynamicJumpRelocations, "DT_JMPREL");
    requireTag(DynamicPltRel, DynamicJumpRelocations, "DT_JMPREL");
    requireTag(DynamicRelrSize, DynamicRelr, "DT_RELR");
    requireTag(DynamicRelrEntry, DynamicRelr, "DT_RELR");
    requireValue(DynamicSymbolEntry, 24, "DT_SYMENT");
    requireValue(DynamicRelaEntry, 24, "DT_RELAENT");
    requireValue(DynamicPltRel, DynamicRela, "DT_PLTREL");
    requireValue(DynamicRelrEntry, 8, "DT_RELRENT");
}

ElfFile::ElfFile(const std::string& path, const std::optional<std::uint64_t> base)
    : Path(path), Reader(ReadFile(path)), Header(Reader.ReadHeader()), Programs(Reader.ReadProgramHeaders()), Sections(Reader.ReadSectionHeaders()), Bias(LoadBias(path, Header, Programs, base)) {
    const auto dynamic = std::find_if(Programs.begin(), Programs.end(), [](const auto& program) { return program.Type == ProgramDynamic; });
    if (dynamic == Programs.end()) return;
    std::vector<std::uint64_t> needed;
    for (const auto& tag : Reader.ReadDynamicTags(*dynamic)) {
        if (tag.Tag == DynamicNeeded) needed.push_back(tag.Value);
        Tags.emplace(tag.Tag, tag.Value);
    }
    CheckDynamicTags(*this);
    for (const auto name : needed) Needed.push_back(ReadString(Bytes(), FileOffset(DynamicStringTable) + name));
}

std::vector<GuestSegment> ReadSegments(const ElfFile& elf) {
    std::vector<GuestSegment> segments;
    for (const auto& program : elf.Programs) {
        if (program.Type == ProgramLoad && program.MemorySize != 0) segments.push_back({program.MappedAddress + elf.Bias, program.MemorySize, elf.Reader.ReadSegment(program), program.Flags});
    }
    return segments;
}

GuestTls ReadTls(const ElfFile& elf) {
    std::optional<GuestTls> tls;
    for (const auto& program : elf.Programs) {
        if (program.Type != ProgramTls) continue;
        if (tls) throw std::runtime_error(elf.Path + " has more than one TLS segment");
        tls = GuestTls{program.MappedAddress + elf.Bias, program.FileSize, program.MemorySize, std::max<std::uint64_t>(program.Alignment, 1)};
    }
    return tls.value_or(GuestTls{});
}

std::map<std::uint64_t, std::string> ReadFunctionSymbols(const ElfFile& elf) {
    std::map<std::uint64_t, std::string> functions;
    for (const auto& section : elf.Sections) {
        if (section.Type != SectionSymbolTable || section.EntrySize != 24) continue;
        const auto& strings = elf.Sections.at(section.Link);
        for (std::uint64_t offset = section.Offset; offset + 24 <= section.Offset + section.SectionSize; offset += 24) {
            const auto symbol = ReadSymbol(elf.Bytes(), offset, strings.Offset);
            if (symbol.Type == SymbolFunction && symbol.Section != 0 && symbol.Section != SectionAbsolute && symbol.Value != 0) functions.emplace(symbol.Value + elf.Bias, symbol.Name);
        }
    }
    return functions;
}

struct DynamicExports {
    std::map<std::string, std::uint64_t> Addresses;
    std::set<std::string> IndirectFunctions;
    std::set<std::uint64_t> Functions;
};

DynamicExports ReadExports(const ElfFile& elf) {
    DynamicExports exports;
    if (!elf.Has(DynamicSymbolTable)) return exports;
    std::uint64_t count = 0;
    if (elf.Has(DynamicHash)) {
        count = ReadAt<std::uint32_t>(elf.Bytes(), elf.FileOffset(DynamicHash) + 4);
    } else {
        const auto table = std::find_if(elf.Sections.begin(), elf.Sections.end(), [](const auto& section) { return section.Type == SectionDynamicSymbols; });
        if (table == elf.Sections.end()) throw std::runtime_error(elf.Path + " has dynamic symbols but neither DT_HASH nor a .dynsym section to count them");
        count = table->SectionSize / 24;
    }
    for (std::uint32_t index = 1; index < count; ++index) {
        const auto symbol = elf.DynamicSymbol(index);
        if (!Exported(symbol)) continue;
        if (symbol.Type == SymbolIndirectFunction) {
            exports.IndirectFunctions.insert(symbol.Name);
            continue;
        }
        exports.Addresses.emplace(symbol.Name, Address(symbol, elf.Bias));
        if (symbol.Type == SymbolFunction) exports.Functions.insert(Address(symbol, elf.Bias));
    }
    return exports;
}

std::vector<Relocation> ReadRelocations(const ElfFile& elf) {
    std::vector<Relocation> relocations;
    const auto& bytes = elf.Bytes();
    const auto readTable = [&](const std::int64_t table, const std::int64_t size) {
        if (!elf.Has(table)) return;
        const auto begin = elf.FileOffset(table);
        for (std::uint64_t offset = begin; offset + 24 <= begin + elf.Tags.at(size); offset += 24) {
            const auto info = ReadAt<std::uint64_t>(bytes, offset + 8);
            relocations.push_back({ReadAt<std::uint64_t>(bytes, offset), static_cast<std::uint32_t>(info), static_cast<std::uint32_t>(info >> 32), ReadAt<std::int64_t>(bytes, offset + 16)});
        }
    };
    readTable(DynamicRela, DynamicRelaSize);
    readTable(DynamicJumpRelocations, DynamicJumpRelocationsSize);
    if (!elf.Has(DynamicRelr)) return relocations;
    const auto relative = [&](const std::uint64_t address) {
        relocations.push_back({address, RelocationRelative, 0, ReadAt<std::int64_t>(bytes, elf.Reader.TranslateVirtualAddress(address))});
    };
    const auto begin = elf.FileOffset(DynamicRelr);
    std::uint64_t base = 0;
    for (std::uint64_t offset = begin; offset + 8 <= begin + elf.Tags.at(DynamicRelrSize); offset += 8) {
        const auto entry = ReadAt<std::uint64_t>(bytes, offset);
        if ((entry & 1) == 0) {
            relative(entry);
            base = entry + 8;
            continue;
        }
        for (std::uint64_t bits = entry >> 1, slot = base; bits != 0; bits >>= 1, slot += 8) {
            if (bits & 1) relative(slot);
        }
        base += 63 * 8;
    }
    return relocations;
}

struct ClassifiedRelocations {
    std::vector<GuestRelocation> Relocations;
    std::map<std::uint64_t, std::uint64_t> Pointers;
    std::set<std::uint64_t> ImportSlots;
};

ClassifiedRelocations ClassifyRelocations(const ElfFile& elf, const std::vector<Relocation>& relocations) {
    ClassifiedRelocations classified;
    const auto bias = elf.Bias;
    for (const auto& relocation : relocations) {
        if (relocation.Type == RelocationNone) continue;
        if (relocation.Type == RelocationRelative) {
            classified.Pointers[relocation.Offset] = static_cast<std::uint64_t>(relocation.Addend);
            classified.Relocations.push_back({relocation.Offset + bias, relocation.Type, relocation.Addend, "", true, false, static_cast<std::uint64_t>(relocation.Addend) + bias});
        } else if ((relocation.Type == RelocationAbsolute || relocation.Type == RelocationGlobalData || relocation.Type == RelocationJumpSlot) && relocation.Symbol == 0) {
            classified.Pointers[relocation.Offset] = relocation.Type == RelocationAbsolute ? static_cast<std::uint64_t>(relocation.Addend) : 0;
            classified.Relocations.push_back({relocation.Offset + bias, relocation.Type, relocation.Addend, "", true, false, 0});
        } else if (relocation.Type == RelocationAbsolute || relocation.Type == RelocationGlobalData || relocation.Type == RelocationJumpSlot) {
            const auto symbol = elf.DynamicSymbol(relocation.Symbol);
            const bool defined = symbol.Section != 0;
            if (defined && symbol.Type == SymbolIndirectFunction) throw std::runtime_error("Relocation at 0x" + Hex(relocation.Offset + bias) + " in " + elf.Path + " binds " + symbol.Name + ", an IFUNC, whose resolver ps5lift does not run");
            if (defined) classified.Pointers[relocation.Offset] = symbol.Value + (relocation.Type == RelocationAbsolute ? static_cast<std::uint64_t>(relocation.Addend) : 0);
            else classified.ImportSlots.insert(relocation.Offset);
            classified.Relocations.push_back({relocation.Offset + bias, relocation.Type, relocation.Addend, symbol.Name, defined, symbol.Type == SymbolObject, defined ? Address(symbol, bias) : 0});
        } else if (relocation.Type == RelocationTlsModule || relocation.Type == RelocationTlsOffset || relocation.Type == RelocationThreadPointerOffset) {
            const auto symbol = relocation.Symbol == 0 ? Symbol{} : elf.DynamicSymbol(relocation.Symbol);
            if (relocation.Symbol != 0 && symbol.Section == 0) throw std::runtime_error("TLS relocation at 0x" + Hex(relocation.Offset + bias) + " in " + elf.Path + " names " + symbol.Name + ", which another image defines");
            classified.Relocations.push_back({relocation.Offset + bias, relocation.Type, relocation.Addend, symbol.Name, true, false, symbol.Value});
        } else {
            throw std::runtime_error("Unsupported relocation type " + std::to_string(relocation.Type) + " at 0x" + Hex(relocation.Offset + bias) + " in " + elf.Path);
        }
    }
    return classified;
}

}

GuestImageFile GuestImageFile::Read(const std::string& path, const std::optional<std::uint64_t> base) {
    const ElfFile elf(path, base);
    GuestImageFile file;
    file._path = path;
    file._needed = elf.Needed;
    file._loadBias = elf.Bias;
    file._entry = elf.Header.EntryPoint == 0 ? 0 : elf.Header.EntryPoint + elf.Bias;
    file._programHeaders = {MappedProgramHeaders(path, elf.Header, elf.Programs) + elf.Bias, elf.Programs.size()};
    file._segments = ReadSegments(elf);
    for (const auto& segment : file._segments) file._mappedEnd = std::max(file._mappedEnd, segment.Address + segment.MemorySize);
    file._tls = ReadTls(elf);
    file._functionSymbols = ReadFunctionSymbols(elf);
    if (elf.Has(DynamicInit)) file._init = elf.Tags.at(DynamicInit) + elf.Bias;
    if (elf.Has(DynamicInitArray)) file._initArray = {elf.Tags.at(DynamicInitArray) + elf.Bias, elf.Tags.at(DynamicInitArraySize) / 8};

    auto exports = ReadExports(elf);
    file._exports = std::move(exports.Addresses);
    file._indirectFunctions = std::move(exports.IndirectFunctions);
    file._functionExports = std::move(exports.Functions);

    auto relocations = ClassifyRelocations(elf, ReadRelocations(elf));
    file._relocations = std::move(relocations.Relocations);
    for (const auto& region : Relinker::UnusedNidFilter::ReadExceptionFunctions(elf.Bytes(), elf.Programs, relocations.Pointers, relocations.ImportSlots)) {
        file._exceptionStarts.insert(region.Begin + elf.Bias);
        for (const auto target : region.ExtraTargets) {
            if (target >= region.Begin && target < region.End) file._landingPads[region.Begin + elf.Bias].insert(target + elf.Bias);
        }
    }
    return file;
}

const std::string& GuestImageFile::Path() const {
    return _path;
}

const std::vector<std::string>& GuestImageFile::Needed() const {
    return _needed;
}

std::uint64_t GuestImageFile::End() const {
    std::set<std::string> imported;
    for (const auto& relocation : _relocations) {
        if (!relocation.Defined && !relocation.Object) imported.insert(relocation.Symbol);
    }
    return AlignUp(_mappedEnd, ImportAlignment) + imported.size() * ImportStride;
}

const GuestTls& GuestImageFile::Tls() const {
    return _tls;
}

std::optional<std::uint64_t> GuestImageFile::Export(const std::string& name) const {
    if (_indirectFunctions.contains(name)) throw std::runtime_error(_path + " defines " + name + " as an IFUNC, whose resolver ps5lift does not run");
    const auto found = _exports.find(name);
    if (found == _exports.end()) return std::nullopt;
    return found->second;
}

std::set<std::string> GuestImageFile::UndefinedSymbols() const {
    std::set<std::string> undefined;
    for (const auto& relocation : _relocations) {
        if (!relocation.Defined) undefined.insert(relocation.Symbol);
    }
    return undefined;
}

GuestImage::GuestImage(GuestImageFile file, const GuestTlsPlacement tlsPlacement, const std::map<std::string, std::uint64_t>& definitions)
    : _path(std::move(file._path)), _loadBias(file._loadBias), _programHeaders(file._programHeaders), _exports(std::move(file._exports)), _functionExports(std::move(file._functionExports)),
      _init(file._init), _initArray(file._initArray), _entry(file._entry), _segments(std::move(file._segments)), _functionSymbols(std::move(file._functionSymbols)),
      _exceptionStarts(std::move(file._exceptionStarts)), _landingPads(std::move(file._landingPads)), _tls(file._tls), _tlsPlacement(tlsPlacement) {
    auto nextImport = AlignUp(file._mappedEnd, ImportAlignment);
    std::map<std::string, std::uint64_t> importByName;
    for (const auto& relocation : file._relocations) {
        _relocatedAddresses.push_back(relocation.Offset);
        std::uint64_t value = relocation.Value;
        if (relocation.Type == RelocationTlsModule || relocation.Type == RelocationTlsOffset || relocation.Type == RelocationThreadPointerOffset) {
            if (_tlsPlacement.Module == 0) throw std::runtime_error("TLS relocation at 0x" + Hex(relocation.Offset) + " in " + _path + ", which has no TLS segment");
            const auto offset = relocation.Value + static_cast<std::uint64_t>(relocation.Addend);
            if (relocation.Type == RelocationTlsModule) value = _tlsPlacement.Module;
            else if (relocation.Type == RelocationTlsOffset) value = offset;
            else value = offset - _tlsPlacement.Offset;
            Write64(relocation.Offset, value);
            continue;
        }
        if (!relocation.Defined) {
            if (const auto definition = definitions.find(relocation.Symbol); definition != definitions.end()) {
                value = definition->second;
            } else if (relocation.Object) {
                _dataImports.push_back({relocation.Offset, relocation.Symbol, relocation.Type == RelocationAbsolute ? relocation.Addend : 0});
                Write64(relocation.Offset, 0);
                continue;
            } else {
                auto [found, inserted] = importByName.emplace(relocation.Symbol, nextImport);
                if (inserted) {
                    _imports.emplace(nextImport, relocation.Symbol);
                    nextImport += ImportStride;
                }
                value = found->second;
                if (relocation.Addend == 0) _importSlots.emplace(relocation.Offset, value);
            }
        }
        if (relocation.Type == RelocationAbsolute) value += static_cast<std::uint64_t>(relocation.Addend);
        Write64(relocation.Offset, value);
        if (IsExecutable(value)) _codePointers.insert(value);
    }
    std::sort(_relocatedAddresses.begin(), _relocatedAddresses.end());
}

const std::string& GuestImage::Path() const {
    return _path;
}

std::uint64_t GuestImage::LoadBias() const {
    return _loadBias;
}

const GuestArray& GuestImage::ProgramHeaders() const {
    return _programHeaders;
}

const std::map<std::string, std::uint64_t>& GuestImage::Exports() const {
    return _exports;
}

const std::set<std::uint64_t>& GuestImage::FunctionExports() const {
    return _functionExports;
}

std::uint64_t GuestImage::Init() const {
    return _init;
}

const GuestArray& GuestImage::InitArray() const {
    return _initArray;
}

void GuestImage::Write64(const std::uint64_t address, const std::uint64_t value) {
    for (auto& segment : _segments) {
        if (address < segment.Address || !Within(address - segment.Address, 8, segment.MemorySize)) continue;
        const auto offset = address - segment.Address;
        if (offset + 8 > segment.Bytes.size()) segment.Bytes.resize(offset + 8, 0);
        std::memcpy(segment.Bytes.data() + offset, &value, 8);
        return;
    }
    throw std::runtime_error("Relocation at 0x" + Hex(address) + " is outside every loaded segment");
}

std::uint64_t GuestImage::Entry() const {
    return _entry;
}

bool GuestSegment::IsExecutable() const {
    constexpr std::uint32_t Execute = 1;
    return (Flags & Execute) != 0;
}

bool GuestSegment::IsWritable() const {
    constexpr std::uint32_t Write = 2;
    return (Flags & Write) != 0;
}

const std::vector<GuestSegment>& GuestImage::Segments() const {
    return _segments;
}

std::span<const std::uint8_t> GuestImage::CodeAt(const std::uint64_t address) const {
    for (const auto& segment : _segments) {
        if (!segment.IsExecutable()) continue;
        if (address >= segment.Address && address < segment.Address + segment.Bytes.size()) {
            const auto offset = address - segment.Address;
            return {segment.Bytes.data() + offset, segment.Bytes.size() - offset};
        }
    }
    return {};
}

std::optional<std::uint64_t> GuestImage::ReadData(const std::uint64_t address, const unsigned size) const {
    for (const auto& segment : _segments) {
        if (address < segment.Address || !Within(address - segment.Address, size, segment.Bytes.size())) continue;
        std::uint64_t value = 0;
        std::memcpy(&value, segment.Bytes.data() + (address - segment.Address), size);
        return value;
    }
    return std::nullopt;
}

bool GuestImage::IsExecutable(const std::uint64_t address) const {
    return !CodeAt(address).empty();
}

bool GuestImage::IsReadOnly(const std::uint64_t address, const std::uint64_t size) const {
    for (const auto& segment : _segments) {
        if (address >= segment.Address && Within(address - segment.Address, size, segment.MemorySize)) return !segment.IsWritable();
    }
    return false;
}

bool GuestImage::Relocates(const std::uint64_t address, const std::uint64_t size) const {
    constexpr std::uint64_t Written = 8;
    const auto first = std::lower_bound(_relocatedAddresses.begin(), _relocatedAddresses.end(), address < Written ? 0 : address - Written + 1);
    return first != _relocatedAddresses.end() && *first < address + size;
}

const std::map<std::uint64_t, std::string>& GuestImage::FunctionSymbols() const {
    return _functionSymbols;
}

const std::map<std::uint64_t, std::string>& GuestImage::Imports() const {
    return _imports;
}

std::optional<std::uint64_t> GuestImage::ImportAt(const std::uint64_t slot) const {
    const auto found = _importSlots.find(slot);
    if (found == _importSlots.end()) return std::nullopt;
    return found->second;
}

const std::set<std::uint64_t>& GuestImage::CodePointers() const {
    return _codePointers;
}

const std::set<std::uint64_t>& GuestImage::ExceptionFunctionStarts() const {
    return _exceptionStarts;
}

const std::map<std::uint64_t, std::set<std::uint64_t>>& GuestImage::LandingPads() const {
    return _landingPads;
}

const std::vector<GuestDataImport>& GuestImage::DataImports() const {
    return _dataImports;
}

const GuestTls& GuestImage::Tls() const {
    return _tls;
}

const GuestTlsPlacement& GuestImage::TlsPlacement() const {
    return _tlsPlacement;
}

}
