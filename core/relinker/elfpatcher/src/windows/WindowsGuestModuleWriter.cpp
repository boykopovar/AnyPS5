#include <elfpatcher/general/GuestModuleWriter.hpp>
#include <elfpatcher/windows/WindowsLoadImage.hpp>
#include <elfpatcher/windows/WindowsTlsBuilder.hpp>
#include <elfpatcher/windows/WindowsPeWriter.hpp>
#include <elfpatcher/windows/WindowsRelocationBuilder.hpp>
#include <io/BufferUtils.hpp>
#include <algorithm>
#include <map>
#include <iterator>

namespace Elfpatcher {

std::vector<std::uint8_t> GuestModuleWriter::WriteWindows(const Relinker::GuestImage& guest, Domain::GuestRuntime& runtime) const {
    using namespace Windows;
    WindowsLoadImage image(guest.Bytes, guest.Headers);
    std::vector<std::uint32_t> relocations;
    std::vector<std::pair<std::uint64_t, std::uint32_t>> tlsModules;
    std::map<std::uint64_t, std::uint64_t> targets;
    const auto apply = [&](const std::vector<std::uint8_t>& table) {
        for (std::size_t offset = 0; offset < table.size(); offset += 24) {
            const auto target = Io::ReadU64(table, offset);
            const auto info = Io::ReadU64(table, offset + 8);
            const auto addend = Io::ReadU64(table, offset + 16);
            const auto type = static_cast<std::uint32_t>(info);
            const auto symbolIndex = info >> 32;
            const auto& symbol = guest.Symbols.at(symbolIndex);
            const auto rva = image.GetRva(target, 8);
            for (const auto& header : guest.Headers) {
                if (header.Type == 7 && target >= header.MappedAddress && target - header.MappedAddress < header.FileSize) throw Domain::RelinkerException("Windows guest TLS template relocations require explicit support", target);
            }
            const auto next = targets.lower_bound(target);
            if ((next != targets.end() && next->first < target + 8) || (next != targets.begin() && std::prev(next)->second > target)) throw Domain::RelinkerException("Overlapping guest relocations", target);
            targets.emplace(target, target + 8);
            image.RequireWritable(target, 8);
            if (type == 8) {
                if (symbolIndex != 0) throw Domain::RelinkerException("RELATIVE guest relocation has a symbol", target);
                image.WritePointer(target, image.GetRelocatedAddress(addend));
                relocations.push_back(rva);
            } else if (type == 16) {
                if (addend != 0 || (symbolIndex != 0 && (symbol.Info & 15) != 6)) throw Domain::RelinkerException("Invalid guest TLS module relocation", target);
                if (symbolIndex != 0 && symbol.Section == 0) {
                    image.WritePointer(target, 0);
                    runtime.Imports.push_back({symbol.Name, rva, 0, 16});
                } else tlsModules.emplace_back(target, rva);
            } else if (type == 17) {
                if (symbolIndex != 0 && (symbol.Info & 15) != 6) throw Domain::RelinkerException("Invalid guest TLS offset relocation", target);
                if (symbolIndex != 0 && symbol.Section == 0) {
                    image.WritePointer(target, 0);
                    runtime.Imports.push_back({symbol.Name, rva, addend, 17});
                } else image.WritePointer(target, symbol.Value + addend);
            } else if (type == 18) {
                const auto tls = std::find_if(guest.Headers.begin(), guest.Headers.end(), [](const auto& header) { return header.Type == 7; });
                if (tls == guest.Headers.end() || (symbolIndex != 0 && (symbol.Section == 0 || (symbol.Info & 15) != 6))) throw Domain::RelinkerException("Unsupported external static TLS relocation", target);
                const auto size = Io::AlignUp64(tls->MemorySize, std::max<std::uint64_t>(tls->Alignment, 16));
                image.WritePointer(target, symbol.Value + addend - size);
            } else if (type == 1 || type == 6 || type == 7) {
                if (symbolIndex == 0 || (type != 1 && addend != 0) || (symbol.Info & 15) == 6) throw Domain::RelinkerException("Invalid guest symbol relocation", target);
                if (symbol.Section != 0) {
                    if (symbol.Section >= 0xff00) throw Domain::RelinkerException("Unsupported special guest symbol section", target);
                    image.WritePointer(target, image.GetRelocatedAddress(symbol.Value) + addend);
                    relocations.push_back(rva);
                } else {
                    if (symbol.Name.empty()) throw Domain::RelinkerException("Empty guest import", target);
                    image.WritePointer(target, 0);
                    runtime.Imports.push_back({symbol.Name, rva, addend});
                }
            } else throw Domain::RelinkerException("Unsupported Windows guest relocation " + std::to_string(type), target);
        }
    };
    apply(guest.Dynamic.RelaData);
    apply(guest.Dynamic.RelaPltData);
    auto sections = image.BuildSections();
    auto nextRva = image.GetEndRva();
    std::array<PeDirectory, 16> directories{};
    std::uint32_t tlsIndex = 0;
    directories[9] = WindowsTlsBuilder().Build(guest.Bytes, guest.Headers, image, sections, relocations, nextRva, &tlsIndex);
    for (const auto& [target, rva] : tlsModules) {
        if (tlsIndex == 0) throw Domain::RelinkerException("Guest TLS relocation has no TLS block", target);
        bool written = false;
        for (auto& section : sections) {
            if (rva < section.Rva || rva - section.Rva > section.Data.size() || section.Data.size() - (rva - section.Rva) < 8) continue;
            Io::WriteU64(section.Data, rva - section.Rva, ImageBase + tlsIndex);
            written = true;
            break;
        }
        if (!written) throw Domain::RelinkerException("Guest TLS target is unmapped", target);
        relocations.push_back(rva);
    }
    for (const auto& header : guest.Headers) {
        if (header.Type != 0x6474e550) continue;
        std::vector<std::uint8_t> metadata(4);
        Io::WriteU32(metadata, 0, image.GetRva(header.MappedAddress, header.FileSize));
        sections.push_back({".ehmeta", nextRva, SectionRead | 0x40u, std::move(metadata)});
        nextRva = AlignRva(nextRva + sections.back().Data.size());
    }
    std::map<std::string, std::uint32_t> exports;
    PeSection tlsExports{".tlsrefs", nextRva, SectionRead | 0x40u, {}};
    for (const auto& symbol : guest.Symbols) {
        if (symbol.Section == 0 || (symbol.Info >> 4) == 0 || symbol.Visibility == 1 || symbol.Visibility == 2) continue;
        if (symbol.Section >= 0xff00) throw Domain::RelinkerException("Unsupported guest export section: " + symbol.Name);
        std::uint32_t rva = 0;
        if ((symbol.Info & 15) == 6) {
            if (tlsIndex == 0) throw Domain::RelinkerException("Guest TLS export has no TLS block: " + symbol.Name);
            rva = CheckedRva(tlsExports.Rva + tlsExports.Data.size());
            Io::AppendU64(tlsExports.Data, ImageBase + tlsIndex);
            Io::AppendU64(tlsExports.Data, symbol.Value);
            relocations.push_back(rva);
        } else rva = image.GetRva(symbol.Value, std::max<std::uint64_t>(symbol.Size, 1));
        if (!exports.emplace(symbol.Name, rva).second) throw Domain::RelinkerException("Duplicate guest export: " + symbol.Name);
    }
    if (!tlsExports.Data.empty()) {
        nextRva = AlignRva(nextRva + tlsExports.Data.size());
        sections.push_back(std::move(tlsExports));
    }
    if (exports.size() > 65535) throw Domain::RelinkerException("Too many guest PE exports");
    PeSection exportSection{".edata", nextRva, SectionRead | 0x40u, std::vector<std::uint8_t>(40)};
    auto& data = exportSection.Data;
    const auto count = CheckedRva(exports.size());
    const auto functions = data.size();
    data.resize(data.size() + count * 4);
    const auto names = data.size();
    data.resize(data.size() + count * 4);
    const auto ordinals = data.size();
    data.resize(data.size() + count * 2);
    Io::WriteU32(data, 12, CheckedRva(nextRva + data.size()));
    Io::AppendString(data, guest.OutputName);
    Io::WriteU32(data, 16, 1);
    Io::WriteU32(data, 20, count);
    Io::WriteU32(data, 24, count);
    Io::WriteU32(data, 28, CheckedRva(nextRva + functions));
    Io::WriteU32(data, 32, CheckedRva(nextRva + names));
    Io::WriteU32(data, 36, CheckedRva(nextRva + ordinals));
    std::size_t index = 0;
    for (const auto& [name, rva] : exports) {
        Io::WriteU32(data, functions + index * 4, rva);
        Io::WriteU32(data, names + index * 4, CheckedRva(nextRva + data.size()));
        Io::WriteU16(data, ordinals + index * 2, static_cast<std::uint16_t>(index));
        Io::AppendString(data, name);
        ++index;
    }
    directories[0] = {nextRva, CheckedRva(data.size())};
    nextRva = AlignRva(nextRva + data.size());
    sections.push_back(std::move(exportSection));
    auto relocationData = WindowsRelocationBuilder().BuildBaseRelocations(relocations);
    if (!relocationData.empty()) {
        directories[5] = {nextRva, CheckedRva(relocationData.size())};
        sections.push_back({".reloc", nextRva, SectionRead | 0x02000040u, std::move(relocationData)});
        nextRva = AlignRva(nextRva + sections.back().Data.size());
    }
    const auto entry = nextRva;
    sections.push_back({".dllmain", entry, SectionRead | SectionExecute | 0x20u, {0xb8, 1, 0, 0, 0, 0xc3}});
    runtime.InitRva = guest.Init == 0 ? 0 : image.GetRva(guest.Init);
    runtime.FiniRva = guest.Fini == 0 ? 0 : image.GetRva(guest.Fini);
    auto result = WindowsPeWriter().Write(sections, entry, directories);
    const auto characteristics = Io::ReadU16(result, 0x80 + 22);
    Io::WriteU16(result, 0x80 + 22, static_cast<std::uint16_t>((characteristics | 0x2000) & ~1u));
    return result;
}

}
