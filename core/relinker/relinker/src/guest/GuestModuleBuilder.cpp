#include <relinker/guest/GuestImage.hpp>
#include <elfpatcher/general/GuestModuleWriter.hpp>
#include <codegen/IAmd64OnlyConverter.hpp>
#include <io/FileReader.hpp>
#include <io/BufferUtils.hpp>
#include <algorithm>
#include <fstream>
#include <functional>
#include <iostream>
#include <limits>
#include <set>

namespace Relinker {

namespace {

std::vector<std::string> ReadNeededNames(const Domain::SysVDynamicSection& dynamic) {
    if (dynamic.DynamicSegmentData.size() % 16 != 0) throw Domain::RelinkerException("Invalid executable dependency table");
    std::vector<std::string> names;
    for (std::size_t offset = 0; offset < dynamic.DynamicSegmentData.size(); offset += 16) {
        if (Io::ReadU64(dynamic.DynamicSegmentData, offset) != 1) throw Domain::RelinkerException("Unexpected executable dependency tag");
        const auto nameOffset = Io::ReadU64(dynamic.DynamicSegmentData, offset + 8);
        if (nameOffset >= dynamic.DynStrData.size()) throw Domain::RelinkerException("Invalid dependency string offset");
        const auto start = dynamic.DynStrData.begin() + static_cast<std::ptrdiff_t>(nameOffset);
        const auto end = std::find(start, dynamic.DynStrData.end(), 0);
        if (end == dynamic.DynStrData.end()) throw Domain::RelinkerException("Unterminated dependency string");
        names.emplace_back(start, end);
    }
    return names;
}

}

std::vector<GuestArtifact> GuestModuleBuilder::Build(const std::filesystem::path& inputPath, const std::filesystem::path& outputPath, Domain::SysVDynamicSection& dynamic, const bool windows, const bool toIntel, ISyscallScanner& syscallScanner, const bool lazyBinding, const std::string& runPath, const std::set<std::string>& excludedModules, const std::vector<std::string>& moduleDirectories) const {
    const auto root = std::filesystem::absolute(inputPath).parent_path();
    const auto singular = root / "sce_module";
    const auto plural = root / "sce_modules";
    const auto prx = root / "prx";
    const bool hasSingular = std::filesystem::exists(singular);
    const bool hasPlural = std::filesystem::exists(plural);
    const bool hasPrx = std::filesystem::exists(prx);
    if (hasSingular && hasPlural) throw Domain::RelinkerException("Both sce_module and sce_modules exist beside the input executable");
    if (!hasSingular && !hasPlural && !hasPrx) throw Domain::RelinkerException("sce_module/sce_modules/prx was not found beside the input executable: " + root.string() + ". Use --skip-sce-module only if this game can run without these modules.");
    std::vector<std::filesystem::path> directories;
    if (hasSingular || hasPlural) directories.push_back(hasSingular ? singular : plural);
    if (hasPrx) directories.push_back(prx);
    std::vector<std::filesystem::path> deferredDirectories;
    for (const auto& name : moduleDirectories) {
        const std::filesystem::path relative(name);
        if (relative.empty() || relative.is_absolute() || relative.lexically_normal().generic_string().starts_with(".."))
            throw Domain::RelinkerException("--module-dir must be a directory inside the input directory: " + name);
        const auto directory = (root / relative).lexically_normal();
        if (!std::filesystem::is_directory(directory)) throw Domain::RelinkerException("Guest module path is not a directory: " + directory.string());
        if (std::find(directories.begin(), directories.end(), directory) != directories.end()) throw Domain::RelinkerException("Duplicate guest module directory: " + directory.string());
        directories.push_back(directory);
        deferredDirectories.push_back(directory);
    }
    const auto deferred = [&](const GuestImage& image) {
        return std::find(deferredDirectories.begin(), deferredDirectories.end(), image.SourcePath.parent_path().lexically_normal()) != deferredDirectories.end();
    };
    std::vector<std::filesystem::path> paths;
    std::set<std::string> unmatchedExclusions = excludedModules;
    const auto isElf = [](const std::filesystem::path& path) {
        std::ifstream stream(path, std::ios::binary);
        if (!stream) throw Domain::RelinkerException("Cannot read guest candidate: " + path.string());
        char magic[4]{};
        stream.read(magic, 4);
        if (stream.bad()) throw Domain::RelinkerException("Cannot read guest candidate magic: " + path.string());
        return stream.gcount() == 4 && static_cast<unsigned char>(magic[0]) == 0x7f && magic[1] == 'E' && magic[2] == 'L' && magic[3] == 'F';
    };
    for (const auto& directory : directories) {
        if (!std::filesystem::is_directory(directory)) throw Domain::RelinkerException("Guest module path is not a directory: " + directory.string());
        for (const auto& entry : std::filesystem::directory_iterator(directory)) {
            if (entry.path().filename().string().ends_with(GuestModuleSuffix)) continue;
            if (excludedModules.contains(entry.path().filename().string())) {
                unmatchedExclusions.erase(entry.path().filename().string());
                continue;
            }
            if (!entry.is_regular_file()) continue;
            if (isElf(entry.path())) paths.push_back(entry.path());
        }
    }
    const auto neededNames = ReadNeededNames(dynamic);
    std::set<std::string> missingNeeded;
    for (const auto& name : neededNames) {
        if (excludedModules.contains(name)) continue;
        if (std::none_of(paths.begin(), paths.end(), [&](const auto& path) { return path.filename().string() == name; })) missingNeeded.insert(name);
    }
    if (!missingNeeded.empty()) {
        std::map<std::string, std::filesystem::path> found;
        for (auto it = std::filesystem::recursive_directory_iterator(root); it != std::filesystem::recursive_directory_iterator(); ++it) {
            if (it->is_directory() && std::find(directories.begin(), directories.end(), it->path()) != directories.end()) {
                it.disable_recursion_pending();
                continue;
            }
            const auto name = it->path().filename().string();
            if (!it->is_regular_file() || !missingNeeded.contains(name) || !isElf(it->path())) continue;
            if (!found.emplace(name, it->path()).second) throw Domain::RelinkerException("Ambiguous needed module: " + found.at(name).string() + " and " + it->path().string());
        }
        for (const auto& [name, path] : found) paths.push_back(path);
    }
    if (!unmatchedExclusions.empty()) throw Domain::RelinkerException("Excluded guest module file not found: " + *unmatchedExclusions.begin());
    std::sort(paths.begin(), paths.end());
    if (paths.empty()) return {};
    if (lazyBinding) throw Domain::RelinkerException("Guest modules require eager binding; --lazy-binding is incompatible");
    std::vector<GuestImage> images;
    std::map<std::string, std::vector<std::size_t>> exports;
    std::map<std::string, std::set<std::size_t>> sharedExports;
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
            if (symbol.Section == 0 || symbol.Section == AbsoluteSection || (symbol.Info >> 4) == 0 || symbol.Visibility == 1 || symbol.Visibility == 2) continue;
            auto& providers = exports[symbol.Name];
            const bool repeated = std::find(providers.begin(), providers.end(), images.size()) != providers.end();
            if (!windows && repeated) throw Domain::RelinkerException("Duplicate guest export after stripping #: " + symbol.Name + " in " + path.string() + " and " + path.string());
            if (!repeated) providers.push_back(images.size());
            if (!windows && providers.size() > 1) sharedExports[symbol.Name].insert(providers.begin(), providers.end());
        }
        std::vector<Domain::ProgramHeader> codeHeaders;
        for (const auto& header : image.Headers) if (header.Type == 1 && (header.Flags & 1) != 0) codeHeaders.push_back(header);
        if (toIntel) {
            auto converted = Codegen::MakeAmd64OnlyConverter()->Convert(std::move(image.Bytes), codeHeaders);
            image.Trampolines = std::move(converted.Trampolines);
            image.Bytes = std::move(converted.Bytes);
        }
        for (const auto& header : codeHeaders) {
            const std::vector<std::uint8_t> code(image.Bytes.begin() + header.Offset, image.Bytes.begin() + header.Offset + header.FileSize);
            syscallScanner.ScanCodeSectionForSyscalls(code, header.MappedAddress, header.FileSize);
        }
        images.push_back(std::move(image));
    }
    std::map<std::string, std::size_t> guestNames;
    std::map<std::string, std::size_t> windowsGuestFiles;
    const auto foldFilename = [](std::string name) {
        for (auto& character : name) if (character >= 'A' && character <= 'Z') character = static_cast<char>(character + ('a' - 'A'));
        return name;
    };
    for (std::size_t index = 0; index < images.size(); ++index) {
        for (const auto& name : {images[index].SourcePath.filename().string(), images[index].Soname}) {
            if (name.empty()) continue;
            const auto [found, inserted] = guestNames.emplace(name, index);
            if (!inserted && found->second != index) throw Domain::RelinkerException("Ambiguous guest dependency name: " + name);
        }
        if (windows) windowsGuestFiles.emplace(foldFilename(images[index].SourcePath.filename().string()), index);
    }
    const auto findGuest = [&](const std::string& name) {
        const auto exact = guestNames.find(name);
        if (exact != guestNames.end() || !windows) return exact;
        const auto file = windowsGuestFiles.find(foldFilename(name));
        return file == windowsGuestFiles.end() ? guestNames.end() : guestNames.emplace(name, file->second).first;
    };
    const auto rejectSharedImport = [&](const std::string& name, const std::string& importer) {
        const auto shared = sharedExports.find(name);
        if (shared == sharedExports.end()) return;
        std::string providers;
        for (const auto provider : shared->second) providers += " " + images[provider].SourcePath.string();
        throw Domain::RelinkerException("Ambiguous guest import " + name + " in " + importer + ": exported by" + providers);
    };
    const auto rename = [](std::vector<std::uint8_t>& symbols, std::vector<std::uint8_t>& strings, std::size_t index, const std::string& name) {
        if (strings.size() > std::numeric_limits<std::uint32_t>::max()) throw Domain::RelinkerException("Guest string table too large");
        Io::WriteU32(symbols, index * 24, static_cast<std::uint32_t>(strings.size()));
        Io::AppendString(strings, name + GuestSymbolSuffix);
    };
    if (dynamic.DynSymData.size() % 24 != 0) throw Domain::RelinkerException("Invalid executable symbol table");
    for (std::size_t offset = 0; offset < dynamic.DynSymData.size(); offset += 24) {
        if (Io::ReadU16(dynamic.DynSymData, offset + 6) != 0) continue;
        const auto nameOffset = Io::ReadU32(dynamic.DynSymData, offset);
        if (nameOffset >= dynamic.DynStrData.size()) throw Domain::RelinkerException("Invalid executable symbol name offset");
        const auto start = dynamic.DynStrData.begin() + nameOffset;
        const auto end = std::find(start, dynamic.DynStrData.end(), 0);
        if (end == dynamic.DynStrData.end()) throw Domain::RelinkerException("Unterminated executable symbol name");
        const std::string name(start, end);
        rejectSharedImport(name.substr(0, name.find('#')), inputPath.string());
        if (!windows && exports.contains(name)) rename(dynamic.DynSymData, dynamic.DynStrData, offset / 24, name);
    }
    std::vector<std::set<std::size_t>> dependencies(images.size());
    for (auto& image : images) {
        image.UsePlatformTlsResolver = !windows ? !exports.contains("vNe1w4diLCs") : std::none_of(image.Symbols.begin(), image.Symbols.end(), [](const auto& symbol) {
            return symbol.Name == "vNe1w4diLCs" && symbol.Section != 0 && symbol.Section != AbsoluteSection && (symbol.Info >> 4) != 0 && symbol.Visibility != 1 && symbol.Visibility != 2;
        });
    }
    for (std::size_t index = 0; index < images.size(); ++index) {
        for (const auto& name : images[index].Dependencies) {
            const auto found = findGuest(name);
            if (found != guestNames.end() && found->second != index) dependencies[index].insert(found->second);
        }
        for (const auto& symbol : images[index].Symbols) {
            if (symbol.Section != 0 || symbol.Name.empty()) continue;
            rejectSharedImport(symbol.Name, images[index].SourcePath.string());
            const auto found = exports.find(symbol.Name);
            std::vector<std::size_t> providers;
            if (found != exports.end()) {
                for (const auto provider : found->second) {
                    const auto declared = findGuest(symbol.Library);
                    if (!windows || symbol.Library.empty() || (declared != guestNames.end() && declared->second == provider)) providers.push_back(provider);
                }
            }
            if (providers.size() > 1) throw Domain::RelinkerException("Ambiguous guest import after stripping #: " + symbol.Name);
            if (!providers.empty()) {
                const auto& provider = images[providers.front()];
                const auto exported = std::find_if(provider.Symbols.begin(), provider.Symbols.end(), [&](const auto& candidate) { return candidate.Section != 0 && candidate.Section != AbsoluteSection && candidate.Name == symbol.Name && (candidate.Info >> 4) != 0 && candidate.Visibility != 1 && candidate.Visibility != 2; });
                if (exported == provider.Symbols.end() || ((symbol.Info & 15) != 0 && (symbol.Info & 15) != (exported->Info & 15))) throw Domain::RelinkerException("Guest import/export type mismatch: " + symbol.Name);
                if (providers.front() != index) dependencies[index].insert(providers.front());
            } else if (windows && (symbol.Info & 15) == 6) throw Domain::RelinkerException("Windows guest TLS import requires a guest TLS export: " + symbol.Name);
        }
    }
    if (!windows) {
        for (auto& image : images) {
            for (std::size_t index = 1; index < image.Symbols.size(); ++index) {
                const auto& symbol = image.Symbols[index];
                const bool exported = symbol.Section != 0 && (symbol.Info >> 4) != 0 && symbol.Visibility != 1 && symbol.Visibility != 2;
                if ((symbol.Section == 0 || exported) && exports.contains(symbol.Name)) rename(image.Dynamic.DynSymData, image.Dynamic.DynStrData, index, symbol.Name);
            }
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
        if (findGuest(name) != guestNames.end()) return;
        if (name.empty() || name.find_first_of("/\\:$") != std::string::npos) throw Domain::RelinkerException("Invalid host dependency: " + name);
        if (uniqueHosts.insert(name).second) hostLibraries.push_back(name);
    };
    for (const auto& name : neededNames) addHost(name);
    for (const auto& image : images) for (const auto& dependency : image.Dependencies) addHost(dependency);
    if (uniqueHosts.contains("libSceLibcInternal.prx") && uniqueHosts.insert("libc.prx").second) hostLibraries.push_back("libc.prx");
    dynamic.DynamicSegmentData.clear();
    const auto addNeeded = [&](const std::string& name) {
        Io::AppendU64(dynamic.DynamicSegmentData, 1);
        Io::AppendU64(dynamic.DynamicSegmentData, dynamic.DynStrData.size());
        Io::AppendString(dynamic.DynStrData, name);
    };
    for (const auto index : order) if (!windows && !deferred(images[index])) addNeeded("$ORIGIN/app0/" + images[index].SourcePath.parent_path().lexically_relative(root).generic_string() + "/" + images[index].OutputName);
    for (const auto& name : hostLibraries) addNeeded(name);
    std::string guestRunPath = runPath;
    if (!windows) {
        if (guestRunPath == "$ORIGIN") guestRunPath = "$ORIGIN/../..";
        else if (guestRunPath.starts_with("$ORIGIN/")) guestRunPath.insert(8, "../../");
        else if (!std::filesystem::path(guestRunPath).is_absolute()) throw Domain::RelinkerException("Guest Linux run path must be absolute or begin with $ORIGIN");
    }
    std::vector<GuestArtifact> artifacts;
    for (const auto index : order) {
        const auto& image = images[index];
        const auto relativeDirectory = "app0/" + image.SourcePath.parent_path().lexically_relative(root).generic_string();
        const auto destination = std::filesystem::absolute(outputPath).parent_path() / relativeDirectory;
        const auto target = destination / image.OutputName;
        if (target.lexically_normal() == std::filesystem::absolute(outputPath).lexically_normal()) throw Domain::RelinkerException("Guest output collides with the executable output");
        for (const auto& source : paths) if (std::filesystem::exists(target) && std::filesystem::equivalent(source, target)) throw Domain::RelinkerException("Guest output would overwrite an input module: " + target.string());
        if (std::filesystem::exists(target) && std::filesystem::equivalent(inputPath, target)) throw Domain::RelinkerException("Guest output would overwrite the input executable");
        Domain::GuestRuntime runtime;
        runtime.UsePlatformTlsResolver = image.UsePlatformTlsResolver;
        runtime.Path = relativeDirectory + "/" + image.OutputName;
        runtime.Names = {image.SourcePath.filename().string(), image.Soname};
        runtime.DeferInitialization = deferred(image);
        std::vector<std::uint8_t> output;
        if (windows) output = Elfpatcher::GuestModuleWriter().WriteWindows(image, runtime);
        else {
            std::vector<std::string> needed;
            for (const auto dependency : dependencies[index]) {
                const auto dependencyPath = images[dependency].SourcePath.parent_path() / images[dependency].OutputName;
                needed.push_back("$ORIGIN/" + dependencyPath.lexically_relative(image.SourcePath.parent_path()).generic_string());
            }
            needed.insert(needed.end(), hostLibraries.begin(), hostLibraries.end());
            output = Elfpatcher::GuestModuleWriter().WriteLinux(image, needed, guestRunPath);
        }
        if (windows) {
            for (const auto& [name, provider] : guestNames) {
                if (provider == index && std::find(runtime.Names.begin(), runtime.Names.end(), name) == runtime.Names.end()) runtime.Names.push_back(name);
            }
        }
        dynamic.GuestModules.push_back(std::move(runtime));
        artifacts.push_back({target, std::move(output)});
    }
    return artifacts;
}

}
