#include <relinker/guest/GuestImage.hpp>
#include <relinker/parsing/ElfReader.hpp>
#include <io/BufferUtils.hpp>
#include <io/NativePath.hpp>
#include <algorithm>
#include <bit>
#include <limits>
#include <iterator>
#include <set>
#include <sstream>
#include <utility>
#include <domain/ImportModule.hpp>

namespace Relinker {

GuestImage GuestImageReader::Read(const std::filesystem::path& path, std::vector<std::uint8_t> bytes, const bool windows) const try {
    const auto fail = [&](const std::string& message) { throw Domain::RelinkerException(Io::Utf8Path(path) + ": " + message); };
    const auto range = [&](std::uint64_t offset, std::uint64_t size) {
        if (offset > bytes.size() || size > bytes.size() - offset) {
            std::ostringstream message;
            message << "ELF range exceeds file: offset=0x" << std::hex << offset << ", size=0x" << size << ", fileSize=0x" << bytes.size();
            fail(message.str());
        }
    };
    range(0, 64);
    if (Io::ReadU32(bytes, 0) != 0x464c457f || bytes[4] != 2 || bytes[5] != 1 || bytes[6] != 1 || Io::ReadU16(bytes, 18) != 62 || Io::ReadU32(bytes, 20) != 1) fail("Expected little-endian ELF64 x86-64");
    const auto type = Io::ReadU16(bytes, 16);
    if (type != 3 && type != 0xfe18) fail("ELF is not a shared module");
    if (Io::ReadU16(bytes, 52) != 64 || Io::ReadU16(bytes, 54) != 56 || Io::ReadU16(bytes, 56) == 0) fail("Invalid ELF program header table");
    range(Io::ReadU64(bytes, 32), static_cast<std::uint64_t>(Io::ReadU16(bytes, 56)) * 56);
    GuestImage image;
    image.SourcePath = path;
    image.OutputName = path.filename().string() + GuestModuleSuffix;
    image.Headers = ElfReader(bytes).ReadProgramHeaders();
    const Domain::ProgramHeader* dynamic = nullptr;
    const Domain::ProgramHeader* dynlib = nullptr;
    const Domain::ProgramHeader* tls = nullptr;
    std::map<std::uint64_t, std::uint64_t> loads;
    constexpr std::uint32_t sceComment = 0x6fffff00;
    constexpr std::uint32_t sceLibVersion = 0x6fffff01;
    for (const auto& header : image.Headers) {
        if (header.Type == sceComment || header.Type == sceLibVersion) continue;
        range(header.Offset, header.FileSize);
        if (header.Type == 1) {
            if (header.FileSize > header.MemorySize || header.MemorySize > std::numeric_limits<std::uint64_t>::max() - header.MappedAddress || (header.Flags & ~7u) != 0) fail("Invalid load segment");
            if (header.Alignment > 1 && (!std::has_single_bit(header.Alignment) || header.Offset % header.Alignment != header.MappedAddress % header.Alignment)) fail("Invalid load segment alignment");
            const auto next = loads.lower_bound(header.MappedAddress);
            if ((next != loads.end() && next->first < header.MappedAddress + header.MemorySize) || (next != loads.begin() && std::prev(next)->second > header.MappedAddress)) fail("Overlapping load segments");
            loads.emplace(header.MappedAddress, header.MappedAddress + header.MemorySize);
        }
        if (header.Type == 2) {
            if (dynamic != nullptr) fail("Duplicate dynamic segment");
            dynamic = &header;
        }
        if (header.Type == 0x61000000) {
            if (dynlib != nullptr) fail("Duplicate SCE dynamic data");
            dynlib = &header;
        }
        if (header.Type == 7) {
            if (tls != nullptr || header.FileSize > header.MemorySize || (header.Alignment > 1 && !std::has_single_bit(header.Alignment))) fail("Invalid TLS segment");
            tls = &header;
        }
    }
    if (loads.empty() || dynamic == nullptr || dynamic->FileSize % 16 != 0) fail("Missing or invalid ELF load/dynamic segments");
    std::map<std::uint64_t, std::uint64_t> tags;
    std::vector<std::uint64_t> needed;
    std::vector<std::uint64_t> moduleImports;
    std::vector<std::uint64_t> ps4ModuleImports;
    std::vector<std::uint64_t> moduleExports;
    bool terminated = false;
    for (std::uint64_t offset = dynamic->Offset; offset < dynamic->Offset + dynamic->FileSize; offset += 16) {
        const auto tag = Io::ReadU64(bytes, offset);
        const auto value = Io::ReadU64(bytes, offset + 8);
        if (tag == 0) { terminated = true; break; }
        if (tag == 1) needed.push_back(value);
        else if (tag == 0x61000045) moduleImports.push_back(value);
        else if (tag == 0x6100000f) ps4ModuleImports.push_back(value);
        else if (tag == 0x6100000d || tag == 0x61000043) moduleExports.push_back(value);
        else if (tag < 0x60000000 || tag == 0x6ffffef5 || tag == 0x6100003f || (tag >= 0x61000027 && tag <= 0x6100003b)) {
            if (!tags.emplace(tag, value).second) fail("Duplicate dynamic tag " + std::to_string(tag));
        }
    }
    if (!terminated) fail("Unterminated dynamic segment");
    for (const auto unsupported : {15u, 16u, 17u, 18u, 19u, 22u, 35u, 36u, 37u}) {
        if (tags.contains(unsupported)) fail("Unsupported dynamic tag " + std::to_string(unsupported));
    }
    if (tags.contains(30) && (tags.at(30) & ~(2ull | 8ull)) != 0) fail("Unsupported guest dynamic flags");
    image.Symbolic = tags.contains(30) && (tags.at(30) & 2ull) != 0;
    const auto mapped = [&](std::uint64_t address, std::uint64_t size, std::uint32_t flags) {
        for (const auto& header : image.Headers) {
            if (header.Type == 1 && (header.Flags & flags) == flags && address >= header.MappedAddress && address - header.MappedAddress <= header.MemorySize && size <= header.MemorySize - (address - header.MappedAddress)) return;
        }
        fail("Unmapped or inaccessible guest address " + std::to_string(address));
    };
    const auto translate = [&](std::uint64_t address, std::uint64_t size) {
        for (const auto& header : image.Headers) {
            if (header.Type == 1 && address >= header.MappedAddress && address - header.MappedAddress <= header.FileSize && size <= header.FileSize - (address - header.MappedAddress)) return header.Offset + address - header.MappedAddress;
        }
        fail("Unmapped ELF address " + std::to_string(address));
        return std::uint64_t{};
    };
    const auto get = [&](std::uint64_t standard, std::uint64_t sce) {
        if (tags.contains(standard) == tags.contains(sce)) fail("Missing or ambiguous dynamic tag " + std::to_string(standard));
        return tags.at(tags.contains(standard) ? standard : sce);
    };
    const auto table = [&](std::uint64_t standard, std::uint64_t sce, std::uint64_t size) {
        const auto value = get(standard, sce);
        if (tags.contains(standard)) return translate(value, size);
        if (dynlib == nullptr || value > dynlib->FileSize || size > dynlib->FileSize - value) fail("SCE table exceeds dynamic data segment");
        return dynlib->Offset + value;
    };
    const auto strSize = get(10, 0x61000037);
    const auto strOffset = table(5, 0x61000035, strSize);
    const auto string = [&](std::uint64_t offset) {
        if (offset >= strSize) fail("String offset out of bounds");
        const auto start = bytes.begin() + static_cast<std::ptrdiff_t>(strOffset + offset);
        const auto end = bytes.begin() + static_cast<std::ptrdiff_t>(strOffset + strSize);
        const auto zero = std::find(start, end, 0);
        if (zero == end) fail("Unterminated ELF string");
        return std::string(start, zero);
    };
    if (strSize == 0 || bytes[strOffset] != 0 || get(11, 0x6100003b) != 24) fail("Invalid string or symbol table");
    std::uint64_t symSize = 0;
    if (tags.contains(0x6100003f)) symSize = tags.at(0x6100003f);
    else if (tags.contains(4)) symSize = static_cast<std::uint64_t>(Io::ReadU32(bytes, translate(tags.at(4), 8) + 4)) * 24;
    else if (tags.contains(0x6ffffef5)) {
        const auto address = tags.at(0x6ffffef5);
        const auto header = translate(address, 16);
        const auto bucketCount = Io::ReadU32(bytes, header);
        const auto firstSymbol = Io::ReadU32(bytes, header + 4);
        const auto bloomCount = Io::ReadU32(bytes, header + 8);
        if (bucketCount == 0 || bloomCount == 0 || firstSymbol == 0) fail("Invalid GNU hash header");
        const std::uint64_t bucketOffset = 16 + static_cast<std::uint64_t>(bloomCount) * 8;
        const auto chainOffset = bucketOffset + static_cast<std::uint64_t>(bucketCount) * 4;
        const auto start = translate(address, chainOffset);
        std::uint64_t lastBucket = 0;
        for (std::uint64_t index = 0; index < bucketCount; ++index) {
            const auto bucket = Io::ReadU32(bytes, start + bucketOffset + index * 4);
            if (bucket != 0 && bucket < firstSymbol) fail("Invalid GNU hash bucket");
            lastBucket = std::max(lastBucket, static_cast<std::uint64_t>(bucket));
        }
        std::uint64_t symbolCount = firstSymbol;
        if (lastBucket != 0) {
            symbolCount = lastBucket;
            for (;;) {
                const auto offset = chainOffset + (symbolCount - firstSymbol) * 4;
                if (offset > std::numeric_limits<std::uint64_t>::max() - address) fail("Invalid GNU hash chain");
                const auto chain = Io::ReadU32(bytes, translate(address + offset, 4));
                ++symbolCount;
                if (chain & 1) break;
            }
        }
        symSize = symbolCount * 24;
    } else fail("Missing dynamic symbol count");
    if (symSize == 0 || symSize % 24 != 0 || symSize / 24 > std::numeric_limits<std::uint32_t>::max()) fail("Invalid dynamic symbol count");
    const auto symOffset = table(6, 0x61000039, symSize);
    image.Dynamic.DynSymData.assign(bytes.begin() + symOffset, bytes.begin() + symOffset + symSize);
    image.Dynamic.DynStrData.push_back(0);
    std::map<std::uint64_t, std::string> importModules;
    if (!moduleImports.empty() && !ps4ModuleImports.empty()) fail("Guest module mixes PS4 and PS5 needed-module tags");
    for (const auto value : moduleImports.empty() ? ps4ModuleImports : moduleImports)
        if (!importModules.emplace(value >> 48, string(value & 0xffffffffu)).second) fail("Duplicate import module ID");
    std::vector<std::string> neededLibraries;
    for (const auto offset : needed) neededLibraries.push_back(string(offset));
    const bool consoleFileNames = std::any_of(neededLibraries.begin(), neededLibraries.end(),
        [](const std::string& name) { return Domain::NeededLibraryName(name) != name; });
    std::map<std::string, std::size_t> exports;
    for (std::uint64_t offset = 0; offset < symSize; offset += 24) {
        auto name = string(Io::ReadU32(bytes, symOffset + offset));
        const auto library = Io::ReadU16(bytes, symOffset + offset + 6) == 0 ? Domain::ImportModule(name, importModules, neededLibraries) : std::string{};
        if (Io::ReadU16(bytes, symOffset + offset + 6) == 0 && library.empty()) {
            if (auto declared = Domain::DeclaredImportModule(name, importModules); !declared.empty()) image.Dynamic.UnmatchedImportModules.insert(std::move(declared));
        }
        const auto qualifiedName = name;
        name = name.substr(0, name.find('#'));
        const auto info = bytes[symOffset + offset + 4];
        const auto visibility = bytes[symOffset + offset + 5];
        const auto section = Io::ReadU16(bytes, symOffset + offset + 6);
        const auto value = Io::ReadU64(bytes, symOffset + offset + 8);
        const auto size = Io::ReadU64(bytes, symOffset + offset + 16);
        const GuestSymbol symbol{name, info, visibility, section, value, size, library, qualifiedName};
        if (offset == 0 && (info != 0 || visibility != 0 || section != 0 || value != 0 || size != 0 || !name.empty())) fail("Invalid null symbol");
        if (offset == 0) {
            image.Symbols.push_back({{}, 0, 0, 0, 0, 0});
            continue;
        }
        if (offset != 0 && ((info >> 4) > 2 || (visibility & ~3u) != 0 || ((info & 15) != 0 && (info & 15) != 1 && (info & 15) != 2 && info != 3 && (info & 15) != 6))) fail("Unsupported symbol attributes: " + name);
        if (section != 0 && (info >> 4) != 0 && visibility != 1 && visibility != 2) {
            const auto [found, inserted] = exports.emplace(name, image.Symbols.size());
            if (name.empty() || (!inserted && (!windows || !IdenticalOrdinaryFunctionExport(image.Symbols[found->second], symbol)))) fail("Duplicate or empty export after stripping #: " + name);
        }
        if ((info & 15) == 6 && section != 0 && (tls == nullptr || value > tls->MemorySize || size > tls->MemorySize - value)) fail("TLS symbol exceeds TLS segment: " + name);
        if (section != 0 && section != AbsoluteSection && (info & 15) != 6) {
            if (section >= 0xff00) fail("Unsupported special symbol section: " + name);
            mapped(value, std::max<std::uint64_t>(size, 1), (info & 15) == 2 ? 1 : 0);
        }
        if (image.Dynamic.DynStrData.size() > std::numeric_limits<std::uint32_t>::max()) fail("String table too large");
        Io::WriteU32(image.Dynamic.DynSymData, offset, static_cast<std::uint32_t>(image.Dynamic.DynStrData.size()));
        Io::AppendString(image.Dynamic.DynStrData, name);
        if (consoleFileNames && section == 0 && !name.empty() && name == qualifiedName) image.NamedImports.push_back(image.Symbols.size());
        image.Symbols.push_back(symbol);
    }
    if (tags.contains(14)) {
        image.Soname = string(tags.at(14));
        if (image.Soname.empty() || image.Soname.find_first_of("\\:$\r\n") != std::string::npos) fail("Invalid guest SONAME: " + image.Soname);
    }
    std::set<std::uint64_t> moduleIds;
    for (const auto value : moduleExports) {
        const auto name = string(value & 0xffffffffu);
        if (name.empty() || name.find_first_of("/\\:$\r\n") != std::string::npos) fail("Invalid guest module name: " + name);
        if (!moduleIds.insert(value >> 48).second) fail("Duplicate export module ID");
        image.ModuleNames.push_back(name);
    }
    std::set<std::string> dependencies;
    for (const auto offset : needed) {
        const auto name = string(offset);
        if (name.empty() || name.find_first_of("\\:$\r\n") != std::string::npos) fail("Invalid dependency: " + name);
        if (dependencies.insert(name).second) image.Dependencies.push_back(name);
    }
    const auto relocationRange = [&](std::uint64_t addressTag, std::uint64_t sceAddressTag, std::uint64_t sizeTag, std::uint64_t sceSizeTag) -> std::pair<std::uint64_t, std::uint64_t> {
        if (!tags.contains(addressTag) && !tags.contains(sceAddressTag) && !tags.contains(sizeTag) && !tags.contains(sceSizeTag)) return {0, 0};
        const auto size = get(sizeTag, sceSizeTag);
        if (size % 24 != 0) fail("Invalid relocation table size");
        return {table(addressTag, sceAddressTag, size), size};
    };
    if (tags.contains(7) || tags.contains(0x6100002f)) {
        if (get(9, 0x61000033) != 24) fail("Invalid RELA entry size");
    }
    const auto [relaOffset, relaSize] = relocationRange(7, 0x6100002f, 8, 0x61000031);
    const auto [pltOffset, pltSize] = relocationRange(23, 0x61000029, 2, 0x6100002d);
    if (pltSize != 0 && get(20, 0x6100002b) != 7) fail("Unsupported PLT relocation format");
    if (relaSize != 0 && pltSize != 0 && relaOffset < pltOffset + pltSize && pltOffset < relaOffset + relaSize
        && relaOffset % 24 != pltOffset % 24)
        fail("Unaligned overlapping relocation tables");
    std::map<std::uint64_t, std::uint64_t> relocationTargets;
    const auto copyRelocations = [&](std::uint64_t offset, std::uint64_t size, std::vector<std::uint8_t>& output, bool plt) {
        output.reserve(size);
        for (std::uint64_t index = 0; index < size; index += 24) {
            if (!plt && offset + index >= pltOffset && offset + index - pltOffset < pltSize) continue;
            const auto info = Io::ReadU64(bytes, offset + index + 8);
            const auto relocation = static_cast<std::uint32_t>(info);
            if (relocation == 0) continue;
            if ((info >> 32) >= image.Symbols.size() || (plt && relocation != 7) || (relocation != 1 && relocation != 6 && relocation != 7 && relocation != 8 && relocation != 16 && relocation != 17 && relocation != 18)) fail("Unsupported relocation or symbol index");
            const auto target = Io::ReadU64(bytes, offset + index);
            const auto addend = Io::ReadU64(bytes, offset + index + 16);
            mapped(target, 8, 2);
            const auto next = relocationTargets.lower_bound(target);
            if ((next != relocationTargets.end() && next->first < target + 8) || (next != relocationTargets.begin() && std::prev(next)->second > target)) fail("Overlapping guest relocations");
            relocationTargets.emplace(target, target + 8);
            bool ownTlsBlock = false;
            if (relocation == 8) {
                if ((info >> 32) != 0) {
                    const auto& symbol = image.Symbols[info >> 32];
                    if (symbol.Info == 3) {
                        if (symbol.Value != 0) fail("RELATIVE relocation has a symbol");
                    } else if (symbol.Section == 0 || addend > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()) || symbol.Value != addend)
                        fail("RELATIVE relocation has a symbol");
                    mapped(addend, 0, 0);
                    Io::AppendU64(output, target);
                    Io::AppendU64(output, relocation);
                    Io::AppendU64(output, addend);
                    continue;
                }
                mapped(addend, 0, 0);
            } else if (relocation == 1 || relocation == 6 || relocation == 7) {
                const auto& symbol = image.Symbols[info >> 32];
                if ((info >> 32) == 0 || (symbol.Info & 15) == 6 || (relocation != 1 && addend != 0) || (symbol.Section == 0 && symbol.Name.empty())) fail("Invalid guest symbol relocation");
            } else {
                const auto& symbol = image.Symbols[info >> 32];
                ownTlsBlock = (info >> 32) != 0 && (relocation == 16 || relocation == 17) && symbol.Info == 3 && symbol.Section == 0 && symbol.Value == 0 && tls != nullptr;
                if (((info >> 32) == 0 && tls == nullptr) || ((info >> 32) != 0 && (symbol.Info & 15) != 6 && !ownTlsBlock) || (relocation == 16 && addend != 0)) fail("Invalid guest TLS relocation");
            }
            const auto position = output.size();
            output.insert(output.end(), bytes.begin() + offset + index, bytes.begin() + offset + index + 24);
            if (ownTlsBlock) Io::WriteU64(output, position + 8, relocation);
        }
    };
    copyRelocations(relaOffset, relaSize, image.Dynamic.RelaData, false);
    copyRelocations(pltOffset, pltSize, image.Dynamic.RelaPltData, true);
    if (tags.contains(3) || tags.contains(0x61000027)) image.Got = get(3, 0x61000027);
    if (tags.contains(12)) {
        image.Init = tags.at(12);
        image.HasInit = true;
    }
    if (tags.contains(13)) {
        image.Fini = tags.at(13);
        image.HasFini = true;
    }
    if (image.HasInit) mapped(image.Init, 1, 1);
    if (image.HasFini) mapped(image.Fini, 1, 1);
    const auto readLifecycleArray = [&](std::uint64_t addressTag, std::uint64_t sizeTag, std::vector<std::uint64_t>& entries) {
        if (!tags.contains(sizeTag)) return;
        if (tags.at(sizeTag) != 0 && !tags.contains(addressTag)) fail("Incomplete guest lifecycle array");
        const auto size = tags.at(sizeTag);
        if (size == 0) return;
        if (size % 8 != 0) fail("Unaligned guest lifecycle array size");
        const auto address = tags.at(addressTag);
        mapped(address, size, 4);
        const auto fileOffset = translate(address, size);
        for (std::uint64_t offset = 0; offset < size; offset += 8) {
            bool relocated = false;
            for (const auto* table : {&image.Dynamic.RelaData, &image.Dynamic.RelaPltData}) {
                for (std::size_t position = 0; position < table->size(); position += 24) {
                    if (Io::ReadU64(*table, position) != address + offset) continue;
                    relocated = true;
                    const auto info = Io::ReadU64(*table, position + 8);
                    const auto addend = Io::ReadU64(*table, position + 16);
                    const auto type = static_cast<std::uint32_t>(info);
                    if (type == 8) mapped(addend, 1, 1);
                    else if (type == 1) {
                        const auto& symbol = image.Symbols.at(info >> 32);
                        if ((symbol.Info & 15) != 2 || addend != 0) fail("Invalid guest lifecycle function relocation");
                        if (symbol.Section != 0 && symbol.Section != AbsoluteSection) mapped(symbol.Value, 1, 1);
                    } else fail("Unsupported guest lifecycle relocation");
                }
            }
            if (!relocated && Io::ReadU64(bytes, fileOffset + offset) != 0) fail("Unrelocated guest lifecycle pointer");
            entries.push_back(address + offset);
        }
    };
    readLifecycleArray(25, 27, image.InitArray);
    readLifecycleArray(26, 28, image.FiniArray);
    if (tags.contains(33) && tags.at(33) != 0 && !image.HasInit) fail("Guest PREINIT_ARRAY has no module initializer");
    image.Bytes = std::move(bytes);
    return image;
} catch (Domain::RelinkerException& error) {
    if (error.InputPath.empty()) error.InputPath = Io::Utf8Path(path);
    throw;
}

}
