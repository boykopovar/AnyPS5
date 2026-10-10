#include <elfpatcher/macos/MacOsMachOPatcher.hpp>
#include <elfpatcher/macos/MacOsImage.hpp>
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
        if (name.empty() || !unique.insert(name).second) throw Domain::RelinkerException("Invalid or duplicate DT_NEEDED library: " + name);
        result.push_back(std::move(name));
    }
    if (unique.contains("libSceLibcInternal.prx") && !unique.contains("libc.prx")) result.push_back("libc.prx");
    return result;
}

std::string MachODependency(const std::string& name, const std::string& origin) {
    if (name.starts_with("$ORIGIN")) return MachOLoadPath(name, origin);
    if (name.find('/') != std::string::npos) throw Domain::RelinkerException("Unsupported dependency path: " + name);
    return "@rpath/" + name;
}

std::vector<std::uint8_t> MacOsMachOPatcher::Patch(const std::vector<std::uint8_t>& sourceElf, const std::vector<Domain::ProgramHeader>& originalHeaders, const Domain::SysVDynamicSection& dynamicSection, const std::uint64_t originalPltGotVaddr, const std::string& runPath, const bool lazyBinding, const bool dependencyDiagnostics, const std::vector<Codegen::TrampolineSite>& trampolines) {
    if (dependencyDiagnostics)
        throw Domain::RelinkerException("macOS target does not support --windows-diagnostics");
    static_cast<void>(lazyBinding);

    Windows::WindowsLoadImage image(sourceElf, originalHeaders);
    if (originalPltGotVaddr != 0)
        image.GetRva(originalPltGotVaddr, 8);
    const auto relocations = Windows::WindowsRelocationBuilder().Apply(image, dynamicSection);

    MacOsImageInput input;
    input.Executable = true;
    input.Source = &sourceElf;
    input.Headers = &originalHeaders;
    input.Image = &image;
    input.Rebases = relocations.BaseRelocations;
    for (const auto& import : relocations.Imports)
        input.Binds.push_back({"_" + import.Name, import.TargetRva, static_cast<std::int64_t>(import.Addend)});
    for (const auto& library : ReadNeededLibraries(dynamicSection))
        input.Dylibs.push_back(MachODependency(library, "@executable_path"));
    if (std::find(input.Dylibs.begin(), input.Dylibs.end(), "@rpath/libkernel.prx") == input.Dylibs.end())
        input.Dylibs.push_back("@rpath/libkernel.prx");
    input.RunPaths.push_back(MachOLoadPath(runPath, "@executable_path"));
    input.EntryRva = image.GetEntryRva();
    input.Trampolines = &trampolines;
    return WriteMacOsImage(input);
}

}
