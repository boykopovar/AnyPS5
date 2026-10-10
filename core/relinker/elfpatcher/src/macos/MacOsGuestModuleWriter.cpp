#include <elfpatcher/general/GuestModuleWriter.hpp>
#include <elfpatcher/macos/MacOsImage.hpp>
#include <elfpatcher/windows/WindowsLoadImage.hpp>
#include <elfpatcher/windows/WindowsPeFormat.hpp>
#include <io/BufferUtils.hpp>
#include <algorithm>
#include <map>

namespace Elfpatcher {

namespace {

std::string DynamicName(const Relinker::GuestImage& guest, const std::size_t index) {
    const auto& symbols = guest.Dynamic.DynSymData;
    if ((index + 1) * 24 > symbols.size()) throw Domain::RelinkerException("Guest symbol index outside the dynamic symbol table");
    const auto offset = Io::ReadU32(symbols, index * 24);
    if (offset >= guest.Dynamic.DynStrData.size()) throw Domain::RelinkerException("Guest symbol name offset is out of bounds", offset);
    return Windows::ReadString(guest.Dynamic.DynStrData, offset);
}

}

std::vector<std::uint8_t> GuestModuleWriter::WriteMacOs(const Relinker::GuestImage& guest, const std::vector<std::string>& dependencies, const std::string& runPath) const {
    using namespace MacOs;
    Windows::WindowsLoadImage image(guest.Bytes, guest.Headers, false);
    MacOsImageInput input;
    input.Executable = false;
    input.InstallName = "@rpath/" + guest.OutputName;
    input.Source = &guest.Bytes;
    input.Headers = &guest.Headers;
    input.Image = &image;

    const auto tls = std::find_if(guest.Headers.begin(), guest.Headers.end(), [](const auto& header) { return header.Type == 7; });
    std::map<std::uint64_t, std::uint64_t> targets;
    const auto apply = [&](const std::vector<std::uint8_t>& table) {
        for (std::size_t offset = 0; offset < table.size(); offset += 24) {
            const auto target = Io::ReadU64(table, offset);
            const auto info = Io::ReadU64(table, offset + 8);
            const auto addend = Io::ReadU64(table, offset + 16);
            const auto type = static_cast<std::uint32_t>(info);
            const auto symbolIndex = info >> 32;
            const auto& symbol = guest.Symbols.at(symbolIndex);
            const bool imported = symbolIndex != 0 && symbol.Section == 0;
            const bool tlsSymbol = (symbol.Info & 15) == 6;
            const auto rva = image.GetRva(target, 8);
            for (const auto& header : guest.Headers) {
                if (header.Type != 7 || header.FileSize == 0) continue;
                const auto templateRva = image.GetRva(header.MappedAddress, header.FileSize);
                const auto templateEnd = static_cast<std::uint64_t>(templateRva) + header.FileSize;
                if (rva >= templateEnd || static_cast<std::uint64_t>(rva) + 8 <= templateRva) continue;
                if (rva < templateRva || templateEnd - rva < 8) throw Domain::RelinkerException("Guest relocation crosses the TLS template boundary", target);
                if (type == 16 || imported) throw Domain::RelinkerException("Runtime-bound guest TLS template relocation is not supported", target);
            }
            const auto next = targets.lower_bound(target);
            if ((next != targets.end() && next->first < target + 8) || (next != targets.begin() && std::prev(next)->second > target)) throw Domain::RelinkerException("Overlapping guest relocations", target);
            targets.emplace(target, target + 8);
            image.RequireWritable(target, 8);
            if (type == 8) {
                if (symbolIndex != 0) throw Domain::RelinkerException("RELATIVE guest relocation has a symbol", target);
                image.WritePointer(target, image.GetRelocatedAddress(addend));
                input.Rebases.push_back(rva);
            } else if (type == 16 || type == 17) {
                if ((type == 16 && addend != 0) || (symbolIndex != 0 && !tlsSymbol)) throw Domain::RelinkerException("Invalid guest dynamic TLS relocation", target);
                if (imported) {
                    image.WritePointer(target, 0);
                    input.TlsImports.push_back({"_" + DynamicName(guest, symbolIndex), rva, type == 16, static_cast<std::int64_t>(addend)});
                } else if (type == 16) {
                    image.WritePointer(target, 0);
                    input.TlsModuleSlots.push_back(rva);
                } else image.WritePointer(target, symbol.Value + addend);
            } else if (type == 18) {
                if (tls == guest.Headers.end() || (symbolIndex != 0 && (imported || !tlsSymbol))) throw Domain::RelinkerException("Unsupported external static TLS relocation", target);
                image.WritePointer(target, symbol.Value + addend - GuestTlsBlockSize(*tls));
            } else if (type == 1 || type == 6 || type == 7) {
                if (symbolIndex == 0 || (type != 1 && addend != 0) || tlsSymbol) throw Domain::RelinkerException("Invalid guest symbol relocation", target);
                if (!imported) {
                    if (symbol.Section >= 0xff00) throw Domain::RelinkerException("Unsupported special guest symbol section", target);
                    image.WritePointer(target, image.GetRelocatedAddress(symbol.Value) + addend);
                    input.Rebases.push_back(rva);
                } else {
                    if (symbol.Name.empty()) throw Domain::RelinkerException("Empty guest import", target);
                    image.WritePointer(target, 0);
                    const auto name = guest.UsePlatformTlsResolver && symbol.Name == "vNe1w4diLCs" ? std::string("Aps5GuestTlsGetAddr_nid_no_patch") : DynamicName(guest, symbolIndex);
                    input.Binds.push_back({"_" + name, rva, static_cast<std::int64_t>(addend)});
                }
            } else throw Domain::RelinkerException("Unsupported macOS guest relocation " + std::to_string(type), target);
        }
    };
    apply(guest.Dynamic.RelaData);
    apply(guest.Dynamic.RelaPltData);

    for (std::size_t index = 1; index < guest.Symbols.size(); ++index) {
        const auto& symbol = guest.Symbols[index];
        if (symbol.Section == 0 || (symbol.Info >> 4) == 0 || symbol.Visibility == 1 || symbol.Visibility == 2) continue;
        if (symbol.Section >= 0xff00) throw Domain::RelinkerException("Unsupported guest export section: " + symbol.Name);
        const auto name = "_" + DynamicName(guest, index);
        if ((symbol.Info & 15) == 6) {
            input.TlsExports.push_back({name, symbol.Value});
            continue;
        }
        input.Exports.push_back({name, image.GetRva(symbol.Value, std::max<std::uint64_t>(symbol.Size, 1))});
    }

    for (const auto& dependency : dependencies) input.Dylibs.push_back(MachODependency(dependency, "@loader_path"));
    if (std::find(input.Dylibs.begin(), input.Dylibs.end(), "@rpath/libkernel.prx") == input.Dylibs.end())
        input.Dylibs.push_back("@rpath/libkernel.prx");
    input.RunPaths.push_back(MachOLoadPath(runPath, "@loader_path"));
    for (const auto slot : guest.InitArray) input.InitArrayRvas.push_back(image.GetRva(slot, 8));
    for (const auto slot : guest.FiniArray) input.FiniArrayRvas.push_back(image.GetRva(slot, 8));
    input.InitRva = guest.Init == 0 ? 0 : image.GetRva(guest.Init);
    input.FiniRva = guest.Fini == 0 ? 0 : image.GetRva(guest.Fini);
    input.Trampolines = &guest.Trampolines;
    try {
        return WriteMacOsImage(input);
    } catch (Domain::RelinkerException& error) {
        error.InputPath = guest.SourcePath.string();
        throw;
    }
}

}
