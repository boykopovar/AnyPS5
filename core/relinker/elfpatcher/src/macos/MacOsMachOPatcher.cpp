#include <elfpatcher/macos/MacOsMachOPatcher.hpp>
#include <elfpatcher/macos/MacOsImage.hpp>
#include <elfpatcher/general/ElfConstants.hpp>
#include <elfpatcher/windows/WindowsLoadImage.hpp>
#include <elfpatcher/windows/WindowsRelocationBuilder.hpp>
#include <io/BufferUtils.hpp>
#include <algorithm>
#include <set>
#include <string>

namespace Elfpatcher::MacOs {

std::string MachOLoadPath(const std::string& path, const std::string& origin) {
    if (path == "$ORIGIN") return origin;
    if (path.starts_with("$ORIGIN/")) return origin + path.substr(7);
    return path;
}

std::vector<std::string> MachORunPaths(const std::string& paths, const std::string& origin) {
    std::vector<std::string> result;
    for (std::size_t begin = 0;;) {
        const auto end = paths.find(':', begin);
        const auto path = paths.substr(begin, end == std::string::npos ? end : end - begin);
        if (path.empty()) throw Domain::RelinkerException("Empty macOS run path component is not supported");
        result.push_back(MachOLoadPath(path, origin));
        if (end == std::string::npos) break;
        begin = end + 1;
    }
    return result;
}

std::vector<std::string> ReadNeededLibraries(const Domain::SysVDynamicSection& dynamicSection) {
    const auto& bytes = dynamicSection.DynamicSegmentData;
    if (bytes.size() % 16 != 0) throw Domain::RelinkerException("Invalid dynamic segment size");
    std::vector<std::string> result;
    std::set<std::string> unique;
    for (std::size_t offset = 0; offset < bytes.size(); offset += 16) {
        if (Io::ReadU64(bytes, offset) != 1) throw Domain::RelinkerException("Unexpected tag in rebuilt ELF dependency table", offset);
        const auto nameOffset = Io::ReadU64(bytes, offset + 8);
        if (nameOffset >= dynamicSection.DynStrData.size()) throw Domain::RelinkerException("DT_NEEDED string offset is out of bounds", nameOffset);
        auto name = Windows::ReadString(dynamicSection.DynStrData, static_cast<std::size_t>(nameOffset));
        if (name.empty()) throw Domain::RelinkerException("Invalid DT_NEEDED library: " + name);
        if (unique.insert(name).second) result.push_back(std::move(name));
    }
    return result;
}

std::string MachODependency(const std::string& name, const std::string& origin) {
    if (name.starts_with("$ORIGIN")) return MachOLoadPath(name, origin);
    if (name.find('/') != std::string::npos) throw Domain::RelinkerException("Unsupported dependency path: " + name);
    return "@rpath/" + name;
}

MacOsMachOPatcher::MacOsMachOPatcher(const std::optional<std::uint64_t> fixedBase, std::function<void(std::string_view)> message, const bool traceImports) : fixedBase(fixedBase), message(std::move(message)), traceImports(traceImports) {
    if (fixedBase && (*fixedBase < 0x20000 || *fixedBase > 0x7fffffff0000 || (*fixedBase & 0xffff) != 0))
        throw Domain::RelinkerException("Invalid fixed macOS guest image base");
}

std::vector<std::uint8_t> MacOsMachOPatcher::Patch(std::vector<std::uint8_t> sourceElf, const std::vector<Domain::ProgramHeader>& originalHeaders, const Domain::SysVDynamicSection& dynamicSection, const std::uint64_t originalPltGotVaddr, const std::string& runPath, const bool lazyBinding, const bool dependencyDiagnostics, const std::vector<Codegen::TrampolineSite>& trampolines) {
    if (dependencyDiagnostics)
        throw Domain::RelinkerException("macOS target does not support --windows-diagnostics");
    if (lazyBinding)
        throw Domain::RelinkerException("macOS target does not support --lazy-binding");

    Windows::WindowsLoadImage image(sourceElf, originalHeaders, true, fixedBase ? *fixedBase - Windows::LoadRva : Windows::ImageBase);
    if (originalPltGotVaddr != 0)
        image.GetRva(originalPltGotVaddr, 8);
    const auto relocations = Windows::WindowsRelocationBuilder().Apply(image, dynamicSection, originalHeaders);
    const auto needed = ReadNeededLibraries(dynamicSection);
    if (traceImports && std::find(needed.begin(), needed.end(), "libc.prx") == needed.end())
        throw Domain::RelinkerException("--trace-imports requires libc.prx among the libraries");

    MacOsImageInput input;
    input.Executable = true;
    input.Source = &sourceElf;
    input.Headers = &originalHeaders;
    input.Image = &image;
    input.Message = message;
    for (std::size_t offset = 24; offset < dynamicSection.DynSymData.size(); offset += 24) {
        const auto& symbols = dynamicSection.DynSymData;
        const auto section = Io::ReadU16(symbols, offset + 6);
        const auto info = symbols[offset + 4];
        const auto visibility = symbols[offset + 5] & 3;
        if (section == SHN_UNDEF || (info >> 4) == 0 || visibility == 1 || visibility == 2) continue;
        const auto name = "_" + Windows::ReadString(dynamicSection.DynStrData, Io::ReadU32(symbols, offset));
        const auto value = Io::ReadU64(symbols, offset + 8);
        const auto flags = (info >> 4) == STB_WEAK ? ExportWeakDefinition : std::uint64_t{0};
        if ((info & 15) == STT_TLS) {
            if (section >= SHN_LORESERVE) throw Domain::RelinkerException("Unsupported macOS executable TLS export section: " + name);
            const auto tls = std::find_if(originalHeaders.begin(), originalHeaders.end(), [](const auto& header) { return header.Type == 7; });
            if (tls != originalHeaders.end() && (value > tls->MemorySize || Io::ReadU64(symbols, offset + 16) > tls->MemorySize - value))
                throw Domain::RelinkerException("macOS executable TLS export exceeds PT_TLS: " + name);
            input.TlsExports.push_back({name, value, flags});
        } else if (section == SHN_ABS) input.Exports.push_back({name, value, flags | ExportKindAbsolute});
        else {
            if (section >= SHN_LORESERVE) throw Domain::RelinkerException("Unsupported macOS executable export section: " + name);
            input.Exports.push_back({name, image.GetRva(value, std::max<std::uint64_t>(Io::ReadU64(symbols, offset + 16), 1)), flags});
        }
    }
    input.Rebases = relocations.BaseRelocations;
    for (const auto& import : relocations.Imports) {
        input.Binds.push_back({"_" + import.Name, import.TargetRva, static_cast<std::int64_t>(import.Addend), import.Weak});
        if (traceImports && import.Function && !import.Weak && import.Addend == 0 && !import.Name.empty())
            input.Traced.push_back({import.TargetRva, import.Name, import.Library});
    }
    input.TraceImports = traceImports;
    for (const auto& library : needed)
        input.Dylibs.push_back(MachODependency(library, "@executable_path"));
    for (const auto* library : {"@rpath/libkernel.prx", "@rpath/libc.prx"})
        if (std::find(input.Dylibs.begin(), input.Dylibs.end(), library) == input.Dylibs.end())
            input.Dylibs.push_back(library);
    input.RunPaths = MachORunPaths(runPath, "@executable_path");
    input.EntryRva = image.GetEntryRva();
    input.Trampolines = &trampolines;
    return WriteMacOsImage(input);
}

}
