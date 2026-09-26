#include <relinker/guest/GuestImage.hpp>
#include <relinker/analysis/SyscallScanner.hpp>
#include <elfpatcher/general/GuestModuleWriter.hpp>
#include <codegen/IAmd64OnlyConverter.hpp>
#include <io/FileReader.hpp>
#include <io/BufferUtils.hpp>
#include <algorithm>
#include <fstream>
#include <functional>
#include <iostream>
#include <set>

namespace Relinker {

std::vector<GuestArtifact> GuestModuleBuilder::Build(const std::filesystem::path& inputPath, const std::filesystem::path& outputPath, Domain::SysVDynamicSection& dynamic, const bool windows, const bool toIntel, const bool skipSyscallCheck, const bool lazyBinding, const std::string& runPath) const {
    const auto root = std::filesystem::absolute(inputPath).parent_path();
    const auto singular = root / "sce_module";
    const auto plural = root / "sce_modules";
    const bool hasSingular = std::filesystem::exists(singular);
    const bool hasPlural = std::filesystem::exists(plural);
    if (hasSingular && hasPlural) throw Domain::RelinkerException("Both sce_module and sce_modules exist beside the input executable");
    if (!hasSingular && !hasPlural) {
        std::cout << "WARNING: sce_module not found beside the input executable; it may be unnecessary or missing.\n";
        return {};
    }
    const auto directory = hasSingular ? singular : plural;
    if (!std::filesystem::is_directory(directory)) throw Domain::RelinkerException("Guest module path is not a directory: " + directory.string());
    std::vector<std::filesystem::path> paths;
    for (const auto& entry : std::filesystem::directory_iterator(directory)) {
        if (!entry.is_regular_file()) continue;
        std::ifstream stream(entry.path(), std::ios::binary);
        if (!stream) throw Domain::RelinkerException("Cannot read guest candidate: " + entry.path().string());
        char magic[4]{};
        stream.read(magic, 4);
        if (stream.bad()) throw Domain::RelinkerException("Cannot read guest candidate magic: " + entry.path().string());
        if (stream.gcount() == 4 && static_cast<unsigned char>(magic[0]) == 0x7f && magic[1] == 'E' && magic[2] == 'L' && magic[3] == 'F') paths.push_back(entry.path());
    }
    std::sort(paths.begin(), paths.end());
    if (paths.empty()) return {};
    if (lazyBinding) throw Domain::RelinkerException("Guest modules require eager binding; --lazy-binding is incompatible");
    std::vector<GuestImage> images;
    std::map<std::string, std::size_t> exports;
    std::set<std::string> outputNames;
    Io::FileReader reader;
    for (const auto& path : paths) {
        auto image = GuestImageReader().Read(path, reader.Read(path.string()));
        if (image.OutputName.find_first_of("$\r\n") != std::string::npos) throw Domain::RelinkerException("Unsupported guest filename: " + image.OutputName);
        std::string folded = image.OutputName;
        if (windows) {
            for (auto& value : folded) {
                if (static_cast<unsigned char>(value) >= 128 || value == ':' || value == '$') throw Domain::RelinkerException("Unsupported Windows guest filename: " + image.OutputName);
                if (value >= 'A' && value <= 'Z') value = static_cast<char>(value + ('a' - 'A'));
            }
        }
        if (!outputNames.insert(folded).second) throw Domain::RelinkerException("Conflicting guest output filename: " + image.OutputName);
        for (const auto& symbol : image.Symbols) {
            if (symbol.Section == 0 || (symbol.Info >> 4) == 0 || symbol.Visibility == 1 || symbol.Visibility == 2) continue;
            const auto [existing, inserted] = exports.emplace(symbol.Name, images.size());
            if (!inserted) throw Domain::RelinkerException("Duplicate guest export after stripping #: " + symbol.Name + " in " + images.at(existing->second).SourcePath.string() + " and " + path.string());
        }
        std::vector<Domain::ProgramHeader> codeHeaders;
        for (const auto& header : image.Headers) if (header.Type == 1 && (header.Flags & 1) != 0) codeHeaders.push_back(header);
        if (toIntel) image.Bytes = Codegen::MakeAmd64OnlyConverter()->Convert(std::move(image.Bytes), codeHeaders).Bytes;
        if (!skipSyscallCheck) {
            const auto scanner = MakeSyscallScanner();
            for (const auto& header : codeHeaders) {
                const std::vector<std::uint8_t> code(image.Bytes.begin() + header.Offset, image.Bytes.begin() + header.Offset + header.FileSize);
                scanner->ScanCodeSectionForSyscalls(code, header.MappedAddress, header.FileSize);
            }
        }
        images.push_back(std::move(image));
    }
    std::vector<std::set<std::size_t>> dependencies(images.size());
    for (auto& image : images) image.UsePlatformTlsResolver = !exports.contains("vNe1w4diLCs");
    for (std::size_t index = 0; index < images.size(); ++index) {
        for (const auto& symbol : images[index].Symbols) {
            if (symbol.Section != 0 || symbol.Name.empty()) continue;
            const auto found = exports.find(symbol.Name);
            if (found != exports.end()) {
                const auto& provider = images[found->second];
                const auto exported = std::find_if(provider.Symbols.begin(), provider.Symbols.end(), [&](const auto& candidate) { return candidate.Section != 0 && candidate.Name == symbol.Name && (candidate.Info >> 4) != 0 && candidate.Visibility != 1 && candidate.Visibility != 2; });
                if (exported == provider.Symbols.end() || ((symbol.Info & 15) != 0 && (symbol.Info & 15) != (exported->Info & 15))) throw Domain::RelinkerException("Guest import/export type mismatch: " + symbol.Name);
                if (found->second != index) dependencies[index].insert(found->second);
            } else if (windows && (symbol.Info & 15) == 6) throw Domain::RelinkerException("Windows guest TLS import requires a guest TLS export: " + symbol.Name);
        }
    }
    std::vector<std::size_t> order;
    std::vector<unsigned char> states(images.size());
    const std::function<void(std::size_t)> visit = [&](std::size_t index) {
        if (states[index] == 1) throw Domain::RelinkerException("Cyclic guest initialization dependency: " + images[index].SourcePath.string());
        if (states[index] == 2) return;
        states[index] = 1;
        for (const auto dependency : dependencies[index]) visit(dependency);
        states[index] = 2;
        order.push_back(index);
    };
    for (std::size_t index = 0; index < images.size(); ++index) visit(index);
    std::vector<std::string> hostLibraries;
    std::set<std::string> uniqueHosts;
    const auto addHost = [&](const std::string& name) {
        if (name.empty() || name.find_first_of("/\\:$") != std::string::npos) throw Domain::RelinkerException("Invalid host dependency: " + name);
        if (uniqueHosts.insert(name).second) hostLibraries.push_back(name);
    };
    if (dynamic.DynamicSegmentData.size() % 16 != 0) throw Domain::RelinkerException("Invalid executable dependency table");
    for (std::size_t offset = 0; offset < dynamic.DynamicSegmentData.size(); offset += 16) {
        if (Io::ReadU64(dynamic.DynamicSegmentData, offset) != 1) throw Domain::RelinkerException("Unexpected executable dependency tag");
        const auto nameOffset = Io::ReadU64(dynamic.DynamicSegmentData, offset + 8);
        if (nameOffset >= dynamic.DynStrData.size()) throw Domain::RelinkerException("Invalid dependency string offset");
        const auto start = dynamic.DynStrData.begin() + nameOffset;
        const auto end = std::find(start, dynamic.DynStrData.end(), 0);
        if (end == dynamic.DynStrData.end()) throw Domain::RelinkerException("Unterminated dependency string");
        addHost(std::string(start, end));
    }
    for (const auto& image : images) for (const auto& dependency : image.Dependencies) addHost(dependency);
    dynamic.DynamicSegmentData.clear();
    const auto addNeeded = [&](const std::string& name) {
        Io::AppendU64(dynamic.DynamicSegmentData, 1);
        Io::AppendU64(dynamic.DynamicSegmentData, dynamic.DynStrData.size());
        Io::AppendString(dynamic.DynStrData, name);
    };
    const auto relativeDirectory = directory.filename().generic_string();
    for (const auto index : order) if (!windows) addNeeded("$ORIGIN/" + relativeDirectory + "/" + images[index].OutputName);
    for (const auto& name : hostLibraries) addNeeded(name);
    std::string guestRunPath = runPath;
    if (!windows) {
        if (guestRunPath == "$ORIGIN") guestRunPath = "$ORIGIN/..";
        else if (guestRunPath.starts_with("$ORIGIN/")) guestRunPath.insert(8, "../");
        else if (!std::filesystem::path(guestRunPath).is_absolute()) throw Domain::RelinkerException("Guest Linux run path must be absolute or begin with $ORIGIN");
    }
    std::vector<GuestArtifact> artifacts;
    const auto destination = std::filesystem::absolute(outputPath).parent_path() / directory.filename();
    if (std::filesystem::exists(destination) && std::filesystem::equivalent(destination, directory)) throw Domain::RelinkerException("Guest output directory must differ from the source module directory");
    for (const auto index : order) {
        const auto& image = images[index];
        const auto target = destination / image.OutputName;
        if (target.lexically_normal() == std::filesystem::absolute(outputPath).lexically_normal()) throw Domain::RelinkerException("Guest output collides with the executable output");
        for (const auto& source : paths) if (std::filesystem::exists(target) && std::filesystem::equivalent(source, target)) throw Domain::RelinkerException("Guest output would overwrite an input module: " + target.string());
        if (std::filesystem::exists(target) && std::filesystem::equivalent(inputPath, target)) throw Domain::RelinkerException("Guest output would overwrite the input executable");
        Domain::GuestRuntime runtime;
        runtime.UsePlatformTlsResolver = image.UsePlatformTlsResolver;
        runtime.Path = relativeDirectory + "/" + image.OutputName;
        std::vector<std::uint8_t> output;
        if (windows) output = Elfpatcher::GuestModuleWriter().WriteWindows(image, runtime);
        else {
            std::vector<std::string> needed;
            for (const auto dependency : dependencies[index]) needed.push_back("$ORIGIN/" + images[dependency].OutputName);
            needed.insert(needed.end(), hostLibraries.begin(), hostLibraries.end());
            output = Elfpatcher::GuestModuleWriter().WriteLinux(image, needed, guestRunPath);
        }
        dynamic.GuestModules.push_back(std::move(runtime));
        artifacts.push_back({target, std::move(output)});
    }
    return artifacts;
}

}
