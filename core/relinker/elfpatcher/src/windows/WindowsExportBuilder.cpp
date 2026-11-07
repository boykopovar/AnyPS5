#include <elfpatcher/windows/WindowsExportBuilder.hpp>
#include <relinker/guest/GuestImage.hpp>
#include <io/BufferUtils.hpp>
#include <algorithm>

namespace Elfpatcher::Windows {

PeDirectory WindowsExportBuilder::Build(const std::map<std::string, std::uint32_t>& exports, const std::string& name, std::vector<PeSection>& sections, std::uint32_t& nextRva) const {
    if (exports.size() > 65535) throw Domain::RelinkerException("Too many PE exports");
    PeSection section{".edata", nextRva, SectionRead | 0x40u, std::vector<std::uint8_t>(40)};
    auto& data = section.Data;
    const auto count = CheckedRva(exports.size());
    const auto functions = data.size();
    data.resize(data.size() + count * 4);
    const auto names = data.size();
    data.resize(data.size() + count * 4);
    const auto ordinals = data.size();
    data.resize(data.size() + count * 2);
    Io::WriteU32(data, 12, CheckedRva(nextRva + data.size()));
    Io::AppendString(data, name);
    Io::WriteU32(data, 16, 1);
    Io::WriteU32(data, 20, count);
    Io::WriteU32(data, 24, count);
    Io::WriteU32(data, 28, CheckedRva(nextRva + functions));
    Io::WriteU32(data, 32, CheckedRva(nextRva + names));
    Io::WriteU32(data, 36, CheckedRva(nextRva + ordinals));
    std::size_t index = 0;
    for (const auto& [symbol, rva] : exports) {
        Io::WriteU32(data, functions + index * 4, rva);
        Io::WriteU32(data, names + index * 4, CheckedRva(nextRva + data.size()));
        Io::WriteU16(data, ordinals + index * 2, static_cast<std::uint16_t>(index));
        Io::AppendString(data, symbol);
        ++index;
    }
    const PeDirectory directory{nextRva, CheckedRva(data.size())};
    nextRva = AlignRva(nextRva + data.size());
    sections.push_back(std::move(section));
    return directory;
}

PeDirectory WindowsExportBuilder::BuildExecutable(const Domain::SysVDynamicSection& dynamic, const WindowsLoadImage& image, const std::vector<Domain::ProgramHeader>& headers, const std::uint32_t tlsIndex, std::vector<std::uint32_t>& relocations, const std::string& name, std::vector<PeSection>& sections, std::uint32_t& nextRva) const {
    if (dynamic.DynSymData.size() % 24 != 0) throw Domain::RelinkerException("Invalid executable symbol table");
    std::map<std::string, Relinker::GuestSymbol> symbols;
    std::map<std::string, std::uint32_t> exports;
    PeSection tlsExports{".tlsrefs", nextRva, SectionRead | 0x40u, {}};
    for (std::size_t offset = 24; offset < dynamic.DynSymData.size(); offset += 24) {
        const auto info = dynamic.DynSymData[offset + 4];
        const auto visibility = dynamic.DynSymData[offset + 5] & 3;
        const auto section = Io::ReadU16(dynamic.DynSymData, offset + 6);
        if (section == 0 || (info >> 4) == 0 || visibility == 1 || visibility == 2) continue;
        const auto symbolName = ReadString(dynamic.DynStrData, Io::ReadU32(dynamic.DynSymData, offset));
        if (symbolName.empty()) throw Domain::RelinkerException("Empty executable export");
        Relinker::GuestSymbol symbol{symbolName, info, static_cast<std::uint8_t>(visibility), section, Io::ReadU64(dynamic.DynSymData, offset + 8), Io::ReadU64(dynamic.DynSymData, offset + 16), {}, {}};
        const auto [previous, inserted] = symbols.emplace(symbolName, symbol);
        if (!inserted) {
            const auto& first = previous->second;
            if (first.Info != symbol.Info || first.Visibility != symbol.Visibility || first.Section != symbol.Section || first.Value != symbol.Value || first.Size != symbol.Size) throw Domain::RelinkerException("Duplicate executable export: " + symbolName);
            continue;
        }
        if (section == Relinker::AbsoluteSection) continue;
        if (section >= 0xff00) throw Domain::RelinkerException("Unsupported executable export section: " + symbolName);
        const auto type = info & 15;
        std::uint32_t rva = 0;
        if (type == 6) {
            const auto tls = std::find_if(headers.begin(), headers.end(), [](const auto& header) { return header.Type == 7; });
            if (tlsIndex == 0 || tls == headers.end()) throw Domain::RelinkerException("Executable TLS export has no TLS block: " + symbolName);
            if (symbol.Value >= tls->MemorySize || std::max<std::uint64_t>(symbol.Size, 1) > tls->MemorySize - symbol.Value) throw Domain::RelinkerException("Executable TLS export is outside its TLS block: " + symbolName);
            rva = CheckedRva(tlsExports.Rva + tlsExports.Data.size());
            Io::AppendU64(tlsExports.Data, image.GetImageBase() + tlsIndex);
            Io::AppendU64(tlsExports.Data, symbol.Value);
            relocations.push_back(rva);
        } else {
            if (type != 0 && type != 1 && type != 2) throw Domain::RelinkerException("Unsupported executable export type: " + symbolName);
            rva = image.GetRva(symbol.Value, std::max<std::uint64_t>(symbol.Size, 1));
        }
        exports.emplace(symbolName, rva);
    }
    if (!tlsExports.Data.empty()) {
        nextRva = AlignRva(nextRva + tlsExports.Data.size());
        sections.push_back(std::move(tlsExports));
    }
    return exports.empty() ? PeDirectory{} : Build(exports, name, sections, nextRva);
}

}
