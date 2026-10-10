#include <relinker/guest/GuestImage.hpp>
#include <relinker/parsing/ElfReader.hpp>
#include <io/BufferUtils.hpp>
#include <algorithm>
#include <bit>
#include <limits>
#include <iterator>
#include <set>
#include <sstream>
#include <domain/ImportModule.hpp>

namespace Relinker {

std::vector<std::string> GuestImageReader::ReadModuleNames(const std::vector<std::uint8_t>& bytes) const {
    if (bytes.size() < 64 || Io::ReadU32(bytes, 0) != 0x464c457f || bytes[4] != 2 || bytes[5] != 1 || bytes[6] != 1 || Io::ReadU16(bytes, 18) != 62)
        throw Domain::RelinkerException("Expected little-endian ELF64 x86-64 module metadata");
    const auto type = Io::ReadU16(bytes, 16);
    if (type != 3 && type != 0xfe18) return {};
    if (Io::ReadU16(bytes, 52) != 64 || Io::ReadU16(bytes, 54) != 56 || Io::ReadU16(bytes, 56) == 0)
        throw Domain::RelinkerException("Invalid module metadata program headers");
    const auto range = [&](std::uint64_t offset, std::uint64_t size) {
        if (offset > bytes.size() || size > bytes.size() - offset) throw Domain::RelinkerException("Module metadata exceeds file");
    };
    range(Io::ReadU64(bytes, 32), static_cast<std::uint64_t>(Io::ReadU16(bytes, 56)) * 56);
    ElfReader reader(bytes);
    const auto headers = reader.ReadProgramHeaders();
    const Domain::ProgramHeader* dynamic = nullptr;
    const Domain::ProgramHeader* dynlib = nullptr;
    for (const auto& header : headers) {
        if (header.Type == 2) {
            if (dynamic) throw Domain::RelinkerException("Duplicate module metadata dynamic segment");
            dynamic = &header;
        }
        if (header.Type == 0x61000000) {
            if (dynlib) throw Domain::RelinkerException("Duplicate module metadata dynamic data");
            dynlib = &header;
        }
    }
    if (!dynamic) throw Domain::RelinkerException("Missing module metadata dynamic segment");
    std::map<std::uint64_t, std::uint64_t> strings;
    std::vector<std::uint64_t> modules;
    for (const auto& tag : reader.ReadDynamicTags(*dynamic)) {
        if (tag.Tag == 0x6100000d || tag.Tag == 0x61000043) modules.push_back(tag.Value);
        else if (tag.Tag == 5 || tag.Tag == 10 || tag.Tag == 0x61000035 || tag.Tag == 0x61000037) {
            if (!strings.emplace(tag.Tag, tag.Value).second) throw Domain::RelinkerException("Duplicate module metadata string tag");
        }
    }
    if (modules.empty()) return {};
    const auto get = [&](std::uint64_t standard, std::uint64_t sce) {
        if (strings.contains(standard) == strings.contains(sce)) throw Domain::RelinkerException("Missing or ambiguous module metadata string tag");
        return strings.at(strings.contains(standard) ? standard : sce);
    };
    const auto size = get(10, 0x61000037);
    const auto address = get(5, 0x61000035);
    std::uint64_t offset = 0;
    if (strings.contains(5)) {
        bool found = false;
        for (const auto& header : headers) {
            if (header.Type != 1 || address < header.MappedAddress || address - header.MappedAddress > header.FileSize || size > header.FileSize - (address - header.MappedAddress)) continue;
            range(header.Offset, header.FileSize);
            if (found) throw Domain::RelinkerException("Ambiguous module metadata string mapping");
            offset = header.Offset + address - header.MappedAddress;
            found = true;
        }
        if (!found) throw Domain::RelinkerException("Unmapped module metadata strings");
    } else {
        if (!dynlib || address > dynlib->FileSize || size > dynlib->FileSize - address) throw Domain::RelinkerException("Module metadata strings exceed dynamic data");
        range(dynlib->Offset, dynlib->FileSize);
        offset = dynlib->Offset + address;
    }
    range(offset, size);
    if (size == 0 || bytes[offset] != 0) throw Domain::RelinkerException("Invalid module metadata strings");
    std::vector<std::string> names;
    std::set<std::uint64_t> ids;
    for (const auto value : modules) {
        const auto nameOffset = value & 0xffffffffu;
        if (nameOffset >= size) throw Domain::RelinkerException("Module metadata string offset out of bounds");
        const auto start = bytes.begin() + static_cast<std::ptrdiff_t>(offset + nameOffset);
        const auto end = bytes.begin() + static_cast<std::ptrdiff_t>(offset + size);
        const auto zero = std::find(start, end, 0);
        if (zero == end) throw Domain::RelinkerException("Unterminated module metadata string");
        const std::string name(start, zero);
        if (name.empty() || name.find_first_of("/\\:$\r\n") != std::string::npos) throw Domain::RelinkerException("Invalid module metadata name");
        if (!ids.insert(value >> 48).second) throw Domain::RelinkerException("Duplicate module metadata export ID");
        names.push_back(name);
    }
    return names;
}

GuestImage GuestImageReader::Read(const std::filesystem::path& path, std::vector<std::uint8_t> bytes) const {
    const auto fail = [&](const std::string& message) { throw Domain::RelinkerException(path.string() + ": " + message); };
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
    std::vector<std::uint64_t> moduleExports;
    bool terminated = false;
    for (std::uint64_t offset = dynamic->Offset; offset < dynamic->Offset + dynamic->FileSize; offset += 16) {
        const auto tag = Io::ReadU64(bytes, offset);
        const auto value = Io::ReadU64(bytes, offset + 8);
        if (tag == 0) { terminated = true; break; }
        if (tag == 1) needed.push_back(value);
        else if (tag == 0x61000045) moduleImports.push_back(value);
        else if (tag == 0x6100000d || tag == 0x61000043) moduleExports.push_back(value);
        else if (tag < 0x60000000 || tag == 0x6100003f || (tag >= 0x61000027 && tag <= 0x6100003b)) {
            if (!tags.emplace(tag, value).second) fail("Duplicate dynamic tag " + std::to_string(tag));
        }
    }
    if (!terminated) fail("Unterminated dynamic segment");
    for (const auto unsupported : {15u, 16u, 17u, 18u, 19u, 22u, 35u, 36u, 37u}) {
        if (tags.contains(unsupported)) fail("Unsupported dynamic tag " + std::to_string(unsupported));
    }
    if (tags.contains(30) && (tags.at(30) & ~8ull) != 0) fail("Unsupported guest dynamic flags");
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
    else fail("Missing dynamic symbol count");
    if (symSize == 0 || symSize % 24 != 0 || symSize / 24 > std::numeric_limits<std::uint32_t>::max()) fail("Invalid dynamic symbol count");
    const auto symOffset = table(6, 0x61000039, symSize);
    image.Dynamic.DynSymData.assign(bytes.begin() + symOffset, bytes.begin() + symOffset + symSize);
    image.Dynamic.DynStrData.push_back(0);
    std::map<std::uint64_t, std::string> importModules;
    for (const auto value : moduleImports)
        if (!importModules.emplace(value >> 48, string(value & 0xffffffffu)).second) fail("Duplicate import module ID");
    std::vector<std::string> neededLibraries;
    for (const auto offset : needed) neededLibraries.push_back(string(offset));
    std::set<std::string> exports;
    for (std::uint64_t offset = 0; offset < symSize; offset += 24) {
        auto name = string(Io::ReadU32(bytes, symOffset + offset));
        const auto library = Io::ReadU16(bytes, symOffset + offset + 6) == 0 ? Domain::ImportModule(name, importModules, neededLibraries) : std::string{};
        name = name.substr(0, name.find('#'));
        const auto info = bytes[symOffset + offset + 4];
        const auto visibility = bytes[symOffset + offset + 5];
        const auto section = Io::ReadU16(bytes, symOffset + offset + 6);
        const auto value = Io::ReadU64(bytes, symOffset + offset + 8);
        const auto size = Io::ReadU64(bytes, symOffset + offset + 16);
        if (offset == 0 && (info != 0 || visibility != 0 || section != 0 || value != 0 || size != 0 || !name.empty())) fail("Invalid null symbol");
        if (offset == 0) {
            image.Symbols.push_back({{}, 0, 0, 0, 0, 0});
            continue;
        }
        if (offset != 0 && ((info >> 4) > 2 || (visibility & ~3u) != 0 || ((info & 15) != 0 && (info & 15) != 1 && (info & 15) != 2 && (info & 15) != 6))) fail("Unsupported symbol attributes: " + name);
        if (section != 0 && (info >> 4) != 0 && visibility != 1 && visibility != 2) {
            if (name.empty() || !exports.insert(name).second) fail("Duplicate or empty export after stripping #: " + name);
        }
        if ((info & 15) == 6 && section != 0 && (tls == nullptr || value > tls->MemorySize || size > tls->MemorySize - value)) fail("TLS symbol exceeds TLS segment: " + name);
        if (section != 0 && section != AbsoluteSection && (info & 15) != 6) {
            if (section >= 0xff00) fail("Unsupported special symbol section: " + name);
            mapped(value, std::max<std::uint64_t>(size, 1), (info & 15) == 2 ? 1 : 0);
        }
        if (image.Dynamic.DynStrData.size() > std::numeric_limits<std::uint32_t>::max()) fail("String table too large");
        Io::WriteU32(image.Dynamic.DynSymData, offset, static_cast<std::uint32_t>(image.Dynamic.DynStrData.size()));
        Io::AppendString(image.Dynamic.DynStrData, name);
        image.Symbols.push_back({name, info, visibility, section, value, size, library});
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
    std::map<std::uint64_t, std::uint64_t> relocationTargets;
    const auto copyRelocations = [&](std::uint64_t addressTag, std::uint64_t sceAddressTag, std::uint64_t sizeTag, std::uint64_t sceSizeTag, std::vector<std::uint8_t>& output, bool plt) {
        if (!tags.contains(addressTag) && !tags.contains(sceAddressTag) && !tags.contains(sizeTag) && !tags.contains(sceSizeTag)) return;
        const auto size = get(sizeTag, sceSizeTag);
        if (size % 24 != 0) fail("Invalid relocation table size");
        const auto offset = table(addressTag, sceAddressTag, size);
        output.assign(bytes.begin() + offset, bytes.begin() + offset + size);
        for (std::uint64_t index = 0; index < size; index += 24) {
            const auto info = Io::ReadU64(output, index + 8);
            const auto relocation = static_cast<std::uint32_t>(info);
            if ((info >> 32) >= image.Symbols.size() || (plt && relocation != 7) || (relocation != 1 && relocation != 6 && relocation != 7 && relocation != 8 && relocation != 16 && relocation != 17 && relocation != 18)) fail("Unsupported relocation or symbol index");
            const auto target = Io::ReadU64(output, index);
            const auto addend = Io::ReadU64(output, index + 16);
            mapped(target, 8, 2);
            const auto next = relocationTargets.lower_bound(target);
            if ((next != relocationTargets.end() && next->first < target + 8) || (next != relocationTargets.begin() && std::prev(next)->second > target)) fail("Overlapping guest relocations");
            relocationTargets.emplace(target, target + 8);
            if (relocation == 8) {
                if ((info >> 32) != 0) fail("RELATIVE relocation has a symbol");
                mapped(addend, 0, 0);
            } else if (relocation == 1 || relocation == 6 || relocation == 7) {
                const auto& symbol = image.Symbols[info >> 32];
                if ((info >> 32) == 0 || (symbol.Info & 15) == 6 || (relocation != 1 && addend != 0) || (symbol.Section == 0 && symbol.Name.empty())) fail("Invalid guest symbol relocation");
            } else {
                const auto& symbol = image.Symbols[info >> 32];
                if (((info >> 32) == 0 && tls == nullptr) || ((info >> 32) != 0 && (symbol.Info & 15) != 6) || (relocation == 16 && addend != 0)) fail("Invalid guest TLS relocation");
            }
        }
    };
    if (tags.contains(7) || tags.contains(0x6100002f)) {
        if (get(9, 0x61000033) != 24) fail("Invalid RELA entry size");
    }
    copyRelocations(7, 0x6100002f, 8, 0x61000031, image.Dynamic.RelaData, false);
    copyRelocations(23, 0x61000029, 2, 0x6100002d, image.Dynamic.RelaPltData, true);
    if (!image.Dynamic.RelaPltData.empty() && get(20, 0x6100002b) != 7) fail("Unsupported PLT relocation format");
    if (tags.contains(3) || tags.contains(0x61000027)) image.Got = get(3, 0x61000027);
    if (tags.contains(12)) image.Init = tags.at(12);
    if (tags.contains(13)) image.Fini = tags.at(13);
    if (image.Init != 0) mapped(image.Init, 1, 1);
    if (image.Fini != 0) mapped(image.Fini, 1, 1);
    const auto readLifecycleArray = [&](std::uint64_t addressTag, std::uint64_t sizeTag, std::vector<std::uint64_t>& entries) {
        if (!tags.contains(addressTag) && !tags.contains(sizeTag)) return;
        if (!tags.contains(sizeTag) || (tags.at(sizeTag) != 0 && !tags.contains(addressTag))) fail("Incomplete guest lifecycle array");
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
    if (tags.contains(33) && tags.at(33) != 0 && image.Init == 0) fail("Guest PREINIT_ARRAY has no module initializer");
    image.Bytes = std::move(bytes);
    return image;
}

}
