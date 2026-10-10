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
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

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

std::string FoldFilename(std::string name) {
    for (auto& character : name) if (character >= 'A' && character <= 'Z') character = static_cast<char>(character + ('a' - 'A'));
    return name;
}

std::string ModuleStem(std::string name, const bool windows) {
    if (windows) name = FoldFilename(std::move(name));
    for (const std::string_view suffix : {".debug_prx", ".sprx", ".prx"}) {
        if (name.size() > suffix.size() && name.ends_with(suffix)) return name.substr(0, name.size() - suffix.size());
    }
    return {};
}

}

std::vector<GuestArtifact> GuestModuleBuilder::Build(const std::filesystem::path& inputPath, const std::filesystem::path& outputPath, Domain::SysVDynamicSection& dynamic, const bool windows, const bool macos, const bool toIntel, ISyscallScanner& syscallScanner, const bool lazyBinding, const std::string& runPath, const std::set<std::string>& excludedModules) const {
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
    std::vector<std::filesystem::path> paths;
    std::set<std::string> unmatchedExclusions = excludedModules;
    const auto isElf = [](const std::filesystem::path& path) {
        std::ifstream stream(path, std::ios::binary);
        if (!stream) throw Domain::RelinkerException("Cannot read guest candidate: " + path.string());
        char magic[4]{};
        stream.read(magic, 4);
        if (stream.bad()) throw Domain::RelinkerException("Cannot read guest candidate magic: " + path.string());
        if (stream.gcount() != 4) return false;
        const auto byte = [&](const std::size_t index) { return static_cast<unsigned char>(magic[index]); };
        const bool self = (byte(0) == 0x4f && byte(1) == 0x15 && byte(2) == 0x3d && byte(3) == 0x1d) || (byte(0) == 0x54 && byte(1) == 0x14 && byte(2) == 0xf5 && byte(3) == 0xee);
        if (self) throw Domain::RelinkerException("Guest module is a SELF container, not an ELF: " + path.string());
        return byte(0) == 0x7f && magic[1] == 'E' && magic[2] == 'L' && magic[3] == 'F';
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
    std::map<std::string, std::filesystem::path> neededAliases;
    for (const auto& name : neededNames) {
        if (excludedModules.contains(name)) continue;
        if (std::any_of(paths.begin(), paths.end(), [&](const auto& path) { return path.filename().string() == name || (windows && FoldFilename(path.filename().string()) == FoldFilename(name)); })) continue;
        const auto stem = ModuleStem(name, windows);
        const auto alias = stem.empty() ? paths.end() : std::find_if(paths.begin(), paths.end(), [&](const auto& path) { return ModuleStem(path.filename().string(), windows) == stem; });
        if (alias != paths.end()) neededAliases.emplace(name, *alias);
        else missingNeeded.insert(name);
    }
    if (!missingNeeded.empty() || !unmatchedExclusions.empty()) {
        std::map<std::string, std::filesystem::path> found;
        std::map<std::string, std::filesystem::path> foundByStem;
        const auto record = [](std::map<std::string, std::filesystem::path>& matches, const std::string& name, const std::filesystem::path& path) {
            if (!matches.emplace(name, path).second) throw Domain::RelinkerException("Ambiguous needed module: " + matches.at(name).string() + " and " + path.string());
        };
        for (auto it = std::filesystem::recursive_directory_iterator(root); it != std::filesystem::recursive_directory_iterator(); ++it) {
            if (!it->is_regular_file()) continue;
            const auto name = it->path().filename().string();
            if (name.ends_with(GuestModuleSuffix)) continue;
            if (excludedModules.contains(name)) {
                for (auto parent = it->path().parent_path(); parent != root && !parent.empty(); parent = parent.parent_path()) {
                    if (std::any_of(directories.begin(), directories.end(), [&](const auto& directory) { return std::filesystem::equivalent(parent, directory); })) unmatchedExclusions.erase(name);
                }
                continue;
            }
            const auto stem = ModuleStem(name, windows);
            std::vector<std::string> stemMatches;
            if (!stem.empty()) {
                for (const auto& needed : missingNeeded) if (needed != name && ModuleStem(needed, windows) == stem) stemMatches.push_back(needed);
            }
            if ((!missingNeeded.contains(name) && stemMatches.empty()) || !isElf(it->path())) continue;
            if (missingNeeded.contains(name)) record(found, name, it->path());
            for (const auto& needed : stemMatches) record(foundByStem, needed, it->path());
        }
        for (const auto& name : missingNeeded) {
            const auto exact = found.find(name);
            const auto byStem = foundByStem.find(name);
            if (exact == found.end() && byStem == foundByStem.end()) continue;
            const auto& path = exact != found.end() ? exact->second : byStem->second;
            if (exact == found.end()) neededAliases.emplace(name, path);
            if (std::find(paths.begin(), paths.end(), path) == paths.end()) paths.push_back(path);
        }
    }
    Io::FileReader reader;
    std::map<std::filesystem::path, GuestImage> discovered;
    const auto matchesIdentity = [](const std::string& name, const std::vector<std::string>& identities) {
        return std::any_of(identities.begin(), identities.end(), [&](const auto& identity) {
            return name == identity || name == identity + ".prx" || name == identity + ".sprx" || name == identity + ".suprx";
        });
    };
    const auto foldFilename = [](std::string name) {
        for (auto& character : name) if (character >= 'A' && character <= 'Z') character = static_cast<char>(character + ('a' - 'A'));
        return name;
    };
    for (const auto& path : paths) discovered.emplace(path, GuestImageReader().Read(path, reader.Read(path.string())));
    std::set<std::string> unresolved;
    for (const auto& name : missingNeeded) {
        if (std::none_of(discovered.begin(), discovered.end(), [&](const auto& entry) {
            const auto& [path, image] = entry;
            return path.filename().string() == name || image.Soname == name ||
                (windows && foldFilename(path.filename().string()) == foldFilename(name)) || matchesIdentity(name, image.ModuleNames);
        })) unresolved.insert(name);
    }
    if (!unresolved.empty()) {
        std::map<std::string, std::filesystem::path> identities;
        for (auto it = std::filesystem::recursive_directory_iterator(root); it != std::filesystem::recursive_directory_iterator(); ++it) {
            const auto& path = it->path();
            const auto extension = path.extension().string();
            if (!it->is_regular_file() || (extension != ".prx" && extension != ".sprx" && extension != ".suprx") ||
                path.filename().string().ends_with(GuestModuleSuffix) || excludedModules.contains(path.filename().string()) || discovered.contains(path) || !isElf(path)) continue;
            const auto names = GuestImageReader().ReadModuleNames(reader.Read(path.string()));
            for (const auto& name : unresolved) {
                if (!matchesIdentity(name, names)) continue;
                if (!identities.emplace(name, path).second) throw Domain::RelinkerException("Ambiguous needed module identity: " + name);
            }
        }
        for (const auto& [name, path] : identities) {
            if (discovered.contains(path)) continue;
            discovered.emplace(path, GuestImageReader().Read(path, reader.Read(path.string())));
            paths.push_back(path);
        }
    }
    if (!unmatchedExclusions.empty()) throw Domain::RelinkerException("Excluded guest module file not found: " + *unmatchedExclusions.begin());
    std::sort(paths.begin(), paths.end());
    paths.erase(std::unique(paths.begin(), paths.end()), paths.end());
    if (paths.empty()) return {};
    if (lazyBinding) throw Domain::RelinkerException("Guest modules require eager binding; --lazy-binding is incompatible");
    std::vector<GuestImage> images;
    if (std::filesystem::exists(outputPath)) {
        for (const auto& path : paths)
            if (std::filesystem::equivalent(path, outputPath))
                throw Domain::RelinkerException("Executable output would overwrite an input module: " + outputPath.string());
    }
    std::map<std::string, std::vector<std::size_t>> exports;
    std::map<std::string, std::set<std::size_t>> sharedExports;
    std::set<std::string> outputNames;
    for (const auto& path : paths) {
        auto image = std::move(discovered.at(path));
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
    for (std::size_t index = 0; index < images.size(); ++index) {
        for (const auto& name : {images[index].SourcePath.filename().string(), images[index].Soname}) {
            if (name.empty()) continue;
            const auto [found, inserted] = guestNames.emplace(name, index);
            if (!inserted && found->second != index) throw Domain::RelinkerException("Ambiguous guest dependency name: " + name);
        }
        if (windows) windowsGuestFiles.emplace(FoldFilename(images[index].SourcePath.filename().string()), index);
    }
    for (const auto& [name, path] : neededAliases) {
        if (guestNames.contains(name)) continue;
        const auto image = std::find_if(images.begin(), images.end(), [&](const auto& candidate) { return candidate.SourcePath == path; });
        if (image == images.end()) throw Domain::RelinkerException("Needed module alias has no guest image: " + name);
        const auto index = static_cast<std::size_t>(image - images.begin());
        const auto [found, inserted] = guestNames.emplace(name, index);
        if (!inserted && found->second != index) throw Domain::RelinkerException("Ambiguous guest dependency name: " + name);
    }
    const auto findGuest = [&](const std::string& name) {
        const auto exact = guestNames.find(name);
        if (exact != guestNames.end() || !windows) return exact;
        const auto file = windowsGuestFiles.find(FoldFilename(name));
        return file == windowsGuestFiles.end() ? guestNames.end() : guestNames.emplace(name, file->second).first;
    };
    const auto resolveIdentity = [&](const std::string& name) {
        if (findGuest(name) != guestNames.end()) return;
        std::size_t match = images.size();
        for (std::size_t index = 0; index < images.size(); ++index) {
            const auto& identities = images[index].ModuleNames;
            if (!matchesIdentity(name, identities)) continue;
            if (match != images.size()) throw Domain::RelinkerException("Ambiguous guest module identity: " + name);
            match = index;
        }
        if (match != images.size()) guestNames.emplace(name, match);
    };
    for (const auto& name : missingNeeded) resolveIdentity(name);
    for (const auto& image : images) for (const auto& name : image.Dependencies) resolveIdentity(name);
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
    std::vector<std::set<std::size_t>> systemImports(images.size());
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
        for (std::size_t symbolIndex = 0; symbolIndex < images[index].Symbols.size(); ++symbolIndex) {
            const auto& symbol = images[index].Symbols[symbolIndex];
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
            if (providers.empty()) systemImports[index].insert(symbolIndex);
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
    for (const auto index : order) if (!windows) addNeeded("$ORIGIN/app0/" + images[index].SourcePath.parent_path().lexically_relative(root).generic_string() + "/" + images[index].OutputName);
    for (const auto& name : hostLibraries) addNeeded(name);
    if (!windows && runPath != "$ORIGIN" && !runPath.starts_with("$ORIGIN/") && !std::filesystem::path(runPath).is_absolute()) throw Domain::RelinkerException("Guest Linux run path must be absolute or begin with $ORIGIN");
    const auto outputDirectory = std::filesystem::absolute(outputPath).parent_path().lexically_normal();
    std::vector<GuestArtifact> artifacts;
    for (const auto index : order) {
        const auto& image = images[index];
        const auto relativeDirectory = "app0/" + image.SourcePath.parent_path().lexically_relative(root).generic_string();
        const auto destination = outputDirectory / relativeDirectory;
        const auto target = destination / image.OutputName;
        if (target.lexically_normal() == std::filesystem::absolute(outputPath).lexically_normal()) throw Domain::RelinkerException("Guest output collides with the executable output");
        for (const auto& source : paths) if (std::filesystem::exists(target) && std::filesystem::equivalent(source, target)) throw Domain::RelinkerException("Guest output would overwrite an input module: " + target.string());
        if (std::filesystem::exists(target) && std::filesystem::equivalent(inputPath, target)) throw Domain::RelinkerException("Guest output would overwrite the input executable");
        Domain::GuestRuntime runtime;
        runtime.UsePlatformTlsResolver = image.UsePlatformTlsResolver;
        runtime.Path = relativeDirectory + "/" + image.OutputName;
        runtime.Names = {image.SourcePath.filename().string(), image.Soname};
        std::vector<std::uint8_t> output;
        if (windows) output = Elfpatcher::GuestModuleWriter().WriteWindows(image, runtime);
        else {
            std::string guestRunPath = runPath;
            if (runPath == "$ORIGIN" || runPath.starts_with("$ORIGIN/")) {
                const auto relativeRoot = outputDirectory.lexically_relative(destination.lexically_normal()).generic_string();
                guestRunPath = "$ORIGIN/" + relativeRoot + runPath.substr(7);
            }
            std::vector<std::string> needed;
            for (const auto dependency : dependencies[index]) {
                const auto dependencyPath = images[dependency].SourcePath.parent_path() / images[dependency].OutputName;
                needed.push_back("$ORIGIN/" + dependencyPath.lexically_relative(image.SourcePath.parent_path()).generic_string());
            }
            needed.insert(needed.end(), hostLibraries.begin(), hostLibraries.end());
            output = macos ? Elfpatcher::GuestModuleWriter().WriteMacOs(image, needed, guestRunPath) : Elfpatcher::GuestModuleWriter().WriteLinux(image, needed, guestRunPath);
        }
        if (windows) {
            for (const auto& [name, provider] : guestNames) {
                if (provider == index && std::find(runtime.Names.begin(), runtime.Names.end(), name) == runtime.Names.end()) runtime.Names.push_back(name);
            }
        }
        std::vector<Domain::CallRegistryEntry> imports;
        for (const auto* table : {&image.Dynamic.RelaData, &image.Dynamic.RelaPltData}) {
            for (std::size_t position = 0; position < table->size(); position += 24) {
                const auto symbolIndex = static_cast<std::size_t>(Io::ReadU64(*table, position + 8) >> 32);
                if (!systemImports[index].contains(symbolIndex)) continue;
                const auto& symbol = image.Symbols[symbolIndex];
                imports.push_back({symbol.Name, symbol.Library, {}, 0, {}, Io::ReadU64(*table, position), {}, false});
            }
        }
        dynamic.GuestModules.push_back(std::move(runtime));
        artifacts.push_back({target, std::move(output), std::move(imports)});
    }
    return artifacts;
}

}
