#include <relinker/guest/GuestImage.hpp>
#include <relinker/analysis/SceCodeExtent.hpp>
#include <relinker/analysis/UnusedNidFilter/EhFrameReader.hpp>
#include <elfpatcher/general/GuestModuleWriter.hpp>
#include <codegen/IAmd64OnlyConverter.hpp>
#include <domain/ImportModule.hpp>
#include <io/NativePath.hpp>
#include <io/BufferUtils.hpp>
#include <nid/NidCompute.hpp>
#include <algorithm>
#include <functional>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Relinker {

namespace {

struct GuestExport {
    std::size_t Provider;
    std::uint8_t Type;
};

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

std::string SymbolName(const Domain::SysVDynamicSection& dynamic, const std::size_t index) {
    if (dynamic.DynSymData.size() % 24 != 0 || index >= dynamic.DynSymData.size() / 24) throw Domain::RelinkerException("Invalid symbol index");
    const auto nameOffset = Io::ReadU32(dynamic.DynSymData, index * 24);
    if (nameOffset >= dynamic.DynStrData.size()) throw Domain::RelinkerException("Invalid symbol name offset");
    const auto start = dynamic.DynStrData.begin() + nameOffset;
    const auto end = std::find(start, dynamic.DynStrData.end(), 0);
    if (end == dynamic.DynStrData.end()) throw Domain::RelinkerException("Unterminated symbol name");
    return std::string(start, end);
}

std::set<std::string> ExportedNames(const Domain::SysVDynamicSection& dynamic) {
    std::set<std::string> names;
    for (std::size_t index = 1; index < dynamic.DynSymData.size() / 24; ++index) {
        const auto info = dynamic.DynSymData[index * 24 + 4];
        const auto visibility = dynamic.DynSymData[index * 24 + 5] & 3;
        if (Io::ReadU16(dynamic.DynSymData, index * 24 + 6) != 0 && (info >> 4) != 0 && visibility != 1 && visibility != 2)
            names.insert(SymbolName(dynamic, index));
    }
    return names;
}

void BindNamedImportsByNid(GuestImage& image, const std::set<std::string>& kept) {
    for (const auto index : image.NamedImports) {
        const auto name = SymbolName(image.Dynamic, index);
        if (name.ends_with(GuestSymbolSuffix) || kept.contains(name)) continue;
        if (image.Dynamic.DynStrData.size() > std::numeric_limits<std::uint32_t>::max()) throw Domain::RelinkerException("Guest string table too large");
        Io::WriteU32(image.Dynamic.DynSymData, index * 24, static_cast<std::uint32_t>(image.Dynamic.DynStrData.size()));
        Io::AppendString(image.Dynamic.DynStrData, Nid::ComputeNid(name, ""));
    }
}

std::string FoldFilename(std::string name) {
    for (auto& character : name) if (character >= 'A' && character <= 'Z') character = static_cast<char>(character + ('a' - 'A'));
    return name;
}

std::string ModuleStem(std::string name, const bool windows) {
    if (windows) name = FoldFilename(std::move(name));
    for (const std::string_view suffix : {".debug_prx", ".suprx", ".sprx", ".prx"}) {
        if (name.size() > suffix.size() && name.ends_with(suffix)) return name.substr(0, name.size() - suffix.size());
    }
    return {};
}

}

std::vector<GuestArtifact> GuestModuleBuilder::Build(const std::filesystem::path& inputPath, const std::filesystem::path& outputPath, Domain::SysVDynamicSection& dynamic, const bool windows, const bool macos, const bool toIntel, const bool toRosetta, ISyscallScanner& syscallScanner, const bool lazyBinding, const std::string& runPath, const std::set<std::string>& excludedModules, const std::filesystem::path& root, const std::vector<std::filesystem::path>& runtimeDirectories, std::vector<GuestModuleInput> modules, Domain::IProgress& progress) const {
    std::vector<std::filesystem::path> paths;
    std::map<std::filesystem::path, GuestImage> discovered;
    progress.Report({Domain::ProgressStage::Modules, {}, 0, modules.size()});
    for (auto& module : modules) {
        const auto path = module.Path.lexically_normal();
        if (path == inputPath.lexically_normal()) throw Domain::RelinkerException("Module path collides with the input executable: " + Io::Utf8Path(path));
        if (path == outputPath.lexically_normal()) throw Domain::RelinkerException("Executable output would overwrite an input module: " + Io::Utf8Path(path));
        const auto relative = path.lexically_relative(root);
        if (relative.empty() || relative.has_root_path() || *relative.begin() == "..") throw Domain::RelinkerException("Module is outside module root: " + Io::Utf8Path(path));
        if (excludedModules.contains(Io::Utf8Path(path.filename()))) throw Domain::RelinkerException("Excluded module supplied to converter: " + Io::Utf8Path(path));
        if (discovered.contains(path)) throw Domain::RelinkerException("Duplicate module path: " + Io::Utf8Path(path));
        paths.push_back(path);
        auto image = GuestImageReader().Read(path, std::move(module.Bytes), windows);
        image.PatchedRanges = std::move(module.PatchedRanges);
        discovered.emplace(path, std::move(image));
    }
    const auto neededNames = ReadNeededNames(dynamic);
    std::set<std::string> missingNeeded;
    std::map<std::string, std::filesystem::path> neededAliases;
    for (const auto& name : neededNames) {
        if (excludedModules.contains(name)) continue;
        if (std::any_of(paths.begin(), paths.end(), [&](const auto& path) { return Io::Utf8Path(path.filename()) == name || (windows && FoldFilename(Io::Utf8Path(path.filename())) == FoldFilename(name)); })) continue;
        const auto stem = ModuleStem(name, windows);
        const auto alias = stem.empty() ? paths.end() : std::find_if(paths.begin(), paths.end(), [&](const auto& path) { return ModuleStem(Io::Utf8Path(path.filename()), windows) == stem; });
        if (alias != paths.end()) neededAliases.emplace(name, *alias);
        else missingNeeded.insert(name);
    }
    const auto matchesIdentity = [](const std::string& name, const std::vector<std::string>& identities) {
        const auto stem = std::string_view(name).substr(0, name.size() - Domain::ModuleSuffix(name).size());
        return std::any_of(identities.begin(), identities.end(), [&](const auto& identity) { return name == identity || stem == identity; });
    };
    std::sort(paths.begin(), paths.end());
    paths.erase(std::unique(paths.begin(), paths.end()), paths.end());
    if (paths.empty()) return {};
    if (lazyBinding) throw Domain::RelinkerException("Guest modules require eager binding; --lazy-binding is incompatible");
    std::vector<GuestImage> images;
    std::unordered_map<std::string, std::vector<GuestExport>> exports;
    std::map<std::string, std::set<std::size_t>> sharedExports;
    std::set<std::string> outputNames;
    for (const auto& path : paths) {
        auto image = std::move(discovered.at(path));
        const auto fail = [&](const std::string& message) {
            Domain::RelinkerException error(message);
            error.InputPath = Io::Utf8Path(path);
            throw error;
        };
        if (image.OutputName.find_first_of("$\r\n") != std::string::npos) fail("Unsupported guest filename: " + image.OutputName);
        std::string folded = image.OutputName;
        if (windows) {
            for (auto& value : folded) {
                if (static_cast<unsigned char>(value) >= 128 || value == ':' || value == '$') fail("Unsupported Windows guest filename: " + image.OutputName);
                if (value >= 'A' && value <= 'Z') value = static_cast<char>(value + ('a' - 'A'));
            }
        }
        if (!outputNames.insert(folded).second) fail("Conflicting guest output filename: " + image.OutputName);
        for (const auto& symbol : image.Symbols) {
            if (symbol.Section == 0 || symbol.Section == AbsoluteSection || (symbol.Info >> 4) == 0 || symbol.Visibility == 1 || symbol.Visibility == 2) continue;
            auto& providers = exports[symbol.Name];
            const bool repeated = !providers.empty() && providers.back().Provider == images.size();
            if (!windows && repeated) fail("Duplicate guest export after stripping #: " + symbol.Name + " in " + Io::Utf8Path(path) + " and " + Io::Utf8Path(images[providers.front().Provider].SourcePath));
            if (!repeated) providers.push_back({images.size(), static_cast<std::uint8_t>(symbol.Info & 15)});
            if (!windows && providers.size() > 1)
                for (const auto& provider : providers) sharedExports[symbol.Name].insert(provider.Provider);
        }
        images.push_back(std::move(image));
    }
    std::map<std::string, std::size_t> guestNames;
    std::map<std::string, std::size_t> windowsGuestFiles;
    for (std::size_t index = 0; index < images.size(); ++index) {
        for (const auto& name : {Io::Utf8Path(images[index].SourcePath.filename()), images[index].Soname}) {
            if (name.empty()) continue;
            const auto [found, inserted] = guestNames.emplace(name, index);
            if (!inserted && found->second != index) throw Domain::RelinkerException("Ambiguous guest dependency name: " + name);
        }
        if (windows) windowsGuestFiles.emplace(FoldFilename(Io::Utf8Path(images[index].SourcePath.filename())), index);
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
        for (const auto provider : shared->second) providers += " " + Io::Utf8Path(images[provider].SourcePath);
        throw Domain::RelinkerException("Ambiguous guest import " + name + " in " + importer + ": exported by" + providers);
    };
    const auto rename = [](std::vector<std::uint8_t>& symbols, std::vector<std::uint8_t>& strings, std::size_t index, const std::string& name) {
        if (strings.size() > std::numeric_limits<std::uint32_t>::max()) throw Domain::RelinkerException("Guest string table too large");
        Io::WriteU32(symbols, index * 24, static_cast<std::uint32_t>(strings.size()));
        Io::AppendString(strings, name + GuestSymbolSuffix);
    };
    const auto declaresHost = [&](const std::string& library) { return !windows && !library.empty() && findGuest(library) == guestNames.end(); };
    if (dynamic.DynSymData.size() % 24 != 0) throw Domain::RelinkerException("Invalid executable symbol table");
    std::vector<std::string> executableModules(dynamic.DynSymData.size() / 24);
    std::vector<bool> executableReferences(executableModules.size());
    std::set<std::size_t> executableProviders;
    for (const auto* table : {&dynamic.RelaData, &dynamic.RelaPltData}) {
        if (table->size() % 24 != 0) throw Domain::RelinkerException("Invalid executable relocation table");
        for (std::size_t offset = 0; offset < table->size(); offset += 24) {
            const auto info = Io::ReadU64(*table, offset + 8);
            const auto symbol = info >> 32;
            if (symbol != 0 && symbol < executableReferences.size() && (info & 0xffffffffu) != 0) executableReferences[symbol] = true;
            const auto module = dynamic.ImportModules.find(Io::ReadU64(*table, offset));
            if (symbol != 0 && symbol < executableModules.size() && module != dynamic.ImportModules.end()) executableModules[symbol] = module->second;
        }
    }
    for (std::size_t offset = 0; offset < dynamic.DynSymData.size(); offset += 24) {
        if (Io::ReadU16(dynamic.DynSymData, offset + 6) != 0) continue;
        const auto nameOffset = Io::ReadU32(dynamic.DynSymData, offset);
        if (nameOffset >= dynamic.DynStrData.size()) throw Domain::RelinkerException("Invalid executable symbol name offset");
        const auto start = dynamic.DynStrData.begin() + nameOffset;
        const auto end = std::find(start, dynamic.DynStrData.end(), 0);
        if (end == dynamic.DynStrData.end()) throw Domain::RelinkerException("Unterminated executable symbol name");
        const std::string name(start, end);
        if (declaresHost(executableModules[offset / 24])) continue;
        const std::string base = name.substr(0, name.find('#'));
        const auto shared = sharedExports.find(base);
        std::size_t needed = 0;
        if (shared != sharedExports.end()) {
            for (const auto provider : shared->second) {
                const auto& file = images[provider].SourcePath.filename().string();
                const auto& soname = images[provider].Soname;
                if (std::find(neededNames.begin(), neededNames.end(), file) == neededNames.end() &&
                    (soname.empty() || std::find(neededNames.begin(), neededNames.end(), soname) == neededNames.end())) continue;
                if (++needed > 1) break;
            }
        }
        if (needed != 1) rejectSharedImport(base, Io::Utf8Path(inputPath));
        const auto found = exports.find(name);
        if (!windows && found != exports.end()) {
            rename(dynamic.DynSymData, dynamic.DynStrData, offset / 24, name);
            if (executableReferences[offset / 24] && found->second.size() == 1) executableProviders.insert(found->second.front().Provider);
        }
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
        std::set<std::size_t> referencedSymbols;
        for (const auto* table : {&images[index].Dynamic.RelaData, &images[index].Dynamic.RelaPltData}) {
            for (std::size_t offset = 0; offset < table->size(); offset += 24) {
                const auto info = Io::ReadU64(*table, offset + 8);
                if ((info & 0xffffffffu) != 0) referencedSymbols.insert(info >> 32);
            }
        }
        for (std::size_t symbolIndex = 0; symbolIndex < images[index].Symbols.size(); ++symbolIndex) {
            const auto& symbol = images[index].Symbols[symbolIndex];
            if (symbol.Section != 0 || symbol.Name.empty()) continue;
            if (declaresHost(symbol.Library)) {
                systemImports[index].insert(symbolIndex);
                continue;
            }
            const auto declared = symbol.Library.empty() ? guestNames.end() : findGuest(symbol.Library);
            const auto shared = sharedExports.find(symbol.Name);
            if (shared == sharedExports.end() || declared == guestNames.end() || !shared->second.contains(declared->second))
                rejectSharedImport(symbol.Name, Io::Utf8Path(images[index].SourcePath));
            const auto found = exports.find(symbol.Name);
            const GuestExport* provider = nullptr;
            if (found != exports.end()) {
                for (const auto& candidate : found->second) {
                    if (!symbol.Library.empty() && (declared == guestNames.end() || declared->second != candidate.Provider)) continue;
                    if (provider != nullptr) throw Domain::RelinkerException("Ambiguous guest import after stripping #: " + symbol.Name);
                    provider = &candidate;
                }
            }
            if (provider != nullptr) {
                if ((symbol.Info & 15) != 0 && (symbol.Info & 15) != provider->Type) throw Domain::RelinkerException("Guest import/export type mismatch: " + symbol.Name);
                if (provider->Type == 6 && (symbol.Info & 15) == 0 && referencedSymbols.contains(symbolIndex)) throw Domain::RelinkerException("Non-TLS relocation against guest TLS export: " + symbol.Name);
                if (provider->Provider != index && referencedSymbols.contains(symbolIndex)) dependencies[index].insert(provider->Provider);
            } else if (windows && (symbol.Info & 15) == 6) throw Domain::RelinkerException("Windows guest TLS import requires a guest TLS export: " + symbol.Name);
            if (provider == nullptr) systemImports[index].insert(symbolIndex);
        }
    }
    std::vector<bool> required(images.size());
    const std::function<void(std::size_t)> require = [&](const std::size_t index) {
        if (required[index]) return;
        required[index] = true;
        for (const auto dependency : dependencies[index]) require(dependency);
    };
    const auto isRuntime = [&](const std::size_t index) {
        return std::find(runtimeDirectories.begin(), runtimeDirectories.end(), images[index].SourcePath.parent_path().lexically_normal()) != runtimeDirectories.end();
    };
    const auto standardDirectory = [&](const std::size_t index) {
        const auto directory = images[index].SourcePath.parent_path().lexically_normal();
        return directory == (root / "sce_module").lexically_normal() || directory == (root / "sce_modules").lexically_normal() || directory == (root / "prx").lexically_normal();
    };
    for (const auto& name : neededNames) {
        const auto found = findGuest(name);
        if (found != guestNames.end() && (!windows || standardDirectory(found->second))) require(found->second);
    }
    for (const auto provider : executableProviders) require(provider);
    for (std::size_t index = 0; index < images.size(); ++index) {
        if (!isRuntime(index)) require(index);
        if (windows) for (const auto dependency : dependencies[index]) require(dependency);
    }
    const auto deferred = [&](const std::size_t index) {
        return !required[index] && isRuntime(index);
    };
    const auto appDirectory = [&](const GuestImage& image) {
        const auto directory = image.SourcePath.parent_path().lexically_relative(root).generic_string();
        return directory == "." ? std::string("app0") : "app0/" + directory;
    };
    if (!windows) {
        const auto executableExports = macos ? std::set<std::string>{} : ExportedNames(dynamic);
        for (auto& image : images) {
            for (std::size_t index = 1; index < image.Symbols.size(); ++index) {
                const auto& symbol = image.Symbols[index];
                const bool exported = symbol.Section != 0 && (symbol.Info >> 4) != 0 && symbol.Visibility != 1 && symbol.Visibility != 2;
                if (((symbol.Section == 0 && !declaresHost(symbol.Library)) || exported) && exports.contains(symbol.Name)) rename(image.Dynamic.DynSymData, image.Dynamic.DynStrData, index, symbol.Name);
            }
            if (!macos) BindNamedImportsByNid(image, executableExports);
        }
    }
    std::vector<std::size_t> order;
    std::vector<unsigned char> states(images.size());
    const std::function<void(std::size_t)> visit = [&](std::size_t index) {
        if (states[index] == 1) throw Domain::RelinkerException("Cyclic guest initialization dependency: " + Io::Utf8Path(images[index].SourcePath));
        if (states[index] == 2) return;
        states[index] = 1;
        for (const auto dependency : dependencies[index]) visit(dependency);
        states[index] = 2;
        order.push_back(index);
    };
    for (std::size_t index = 0; index < images.size(); ++index) visit(index);
    std::vector<std::string> hostLibraries;
    std::vector<std::vector<std::string>> moduleHostLibraries(images.size());
    std::set<std::string> uniqueHosts;
    const auto hostLibrary = [&](const std::string& name) { return windows || macos ? name : Domain::NeededLibraryName(name); };
    const auto addHost = [&](const std::string& name, const bool startup) {
        if (findGuest(name) != guestNames.end()) return;
        if (name.empty() || name.find_first_of("/\\:$") != std::string::npos) throw Domain::RelinkerException("Invalid host dependency: " + name);
        if (const auto host = hostLibrary(name); startup && uniqueHosts.insert(host).second) hostLibraries.push_back(host);
    };
    for (const auto& name : neededNames) addHost(name, true);
    for (std::size_t index = 0; index < images.size(); ++index) {
        auto& hosts = moduleHostLibraries[index];
        for (const auto& dependency : images[index].Dependencies) {
            addHost(dependency, windows || !deferred(index));
            if (findGuest(dependency) == guestNames.end()) hosts.push_back(hostLibrary(dependency));
        }
        if (!windows && !macos && images[index].HasInit) {
            if (std::find(hosts.begin(), hosts.end(), "libkernel.prx") == hosts.end()) hosts.push_back("libkernel.prx");
            if (!deferred(index) && uniqueHosts.insert("libkernel.prx").second) hostLibraries.push_back("libkernel.prx");
        }
        if (std::find(hosts.begin(), hosts.end(), "libSceLibcInternal.prx") != hosts.end() &&
            std::find(hosts.begin(), hosts.end(), "libc.prx") == hosts.end()) hosts.push_back("libc.prx");
    }
    if (uniqueHosts.contains("libSceLibcInternal.prx") && uniqueHosts.insert("libc.prx").second) hostLibraries.push_back("libc.prx");
    dynamic.NeededLibraries = hostLibraries;
    dynamic.DynamicSegmentData.clear();
    const auto addNeeded = [&](const std::string& name) {
        Io::AppendU64(dynamic.DynamicSegmentData, 1);
        Io::AppendU64(dynamic.DynamicSegmentData, dynamic.DynStrData.size());
        Io::AppendString(dynamic.DynStrData, name);
    };
    for (const auto index : order) if (!windows && !deferred(index)) addNeeded("$ORIGIN/" + appDirectory(images[index]) + "/" + images[index].OutputName);
    for (const auto& name : hostLibraries) addNeeded(name);
    if (!windows && runPath != "$ORIGIN" && !runPath.starts_with("$ORIGIN/") && !runPath.starts_with("$ORIGIN:") && !runPath.starts_with('/')) throw Domain::RelinkerException("Guest Linux run path must be absolute or begin with $ORIGIN");
    const auto outputDirectory = outputPath.parent_path().lexically_normal();
    std::vector<GuestArtifact> artifacts;
    for (const auto index : order) {
        auto& image = images[index];
        progress.Report({Domain::ProgressStage::Modules, Io::Utf8Path(image.SourcePath), artifacts.size(), order.size()});
        std::vector<Domain::ProgramHeader> codeHeaders;
        for (const auto& header : image.Headers) if (header.Type == 1 && (header.Flags & 1) != 0) codeHeaders.push_back(header);
        if (toIntel) {
            auto converted = Codegen::MakeAmd64OnlyConverter(toRosetta ? Codegen::Amd64OnlyTarget::Rosetta : Codegen::Amd64OnlyTarget::Intel, [&](auto completed, auto total) { progress.Report({Domain::ProgressStage::Lowering, Io::Utf8Path(image.SourcePath), completed, total}); })->Convert(std::move(image.Bytes), image.Headers);
            image.Trampolines = std::move(converted.Trampolines);
            image.Bytes = std::move(converted.Bytes);
            if (!image.PatchedRanges.empty()) GamePatch::CheckOverlaps(Io::Utf8Path(image.SourcePath), image.PatchedRanges, {.Reports = converted.Reports, .Trampolines = image.Trampolines});
        }
        if (!image.PatchedRanges.empty()) GamePatch::CheckOverlaps(Io::Utf8Path(image.SourcePath), image.PatchedRanges, {.RelaData = image.Dynamic.RelaData, .RelaPltData = image.Dynamic.RelaPltData});
        const bool sceImage = std::any_of(image.Headers.begin(), image.Headers.end(), [](const Domain::ProgramHeader& header) { return header.Type == 0x61000000; });
        const auto unwindFunctions = sceImage ? UnusedNidFilter::ReadExceptionFunctions(image.Bytes, image.Headers, {}, {}) : std::vector<UnusedNidFilter::StrictCodeRegion>{};
        for (const auto& header : codeHeaders) {
            std::vector<std::uint8_t> code(image.Bytes.begin() + header.Offset, image.Bytes.begin() + header.Offset + header.FileSize);
            if (sceImage)
                if (const auto end = FindSceCodeEnd(code, header.MappedAddress, unwindFunctions, image.Got, image.Dynamic.RelaPltData.size()))
                    code.resize(*end - header.MappedAddress);
            const auto unwindRanges = sceImage ? SceUnwindRanges(code, header.MappedAddress, unwindFunctions) : std::vector<CodeRange>{};
            for (const auto offset : syscallScanner.ScanCodeSectionForSyscalls(code, header.MappedAddress, code.size(), Io::Utf8Path(image.SourcePath), unwindRanges)) {
                if (offset > code.size() || code.size() - offset < 2u)
                    throw Domain::RelinkerException("Syscall trap patch exceeds guest executable segment", header.MappedAddress + offset);
                if (!image.PatchedRanges.empty()) {
                    const GamePatch::WriterRange trap{header.Offset + offset, 2, "syscall trap"};
                    GamePatch::CheckOverlaps(Io::Utf8Path(image.SourcePath), image.PatchedRanges, {.Traps = std::span<const GamePatch::WriterRange>(&trap, 1)});
                }
                image.Bytes[header.Offset + offset] = 0x0F;
                image.Bytes[header.Offset + offset + 1] = 0x0B;
            }
        }
        const auto relativeDirectory = appDirectory(image);
        const auto destination = outputDirectory / relativeDirectory;
        const auto target = destination / image.OutputName;
        if (target.lexically_normal() == outputPath.lexically_normal()) throw Domain::RelinkerException("Guest output collides with the executable output");
        Domain::GuestRuntime runtime;
        runtime.UsePlatformTlsResolver = image.UsePlatformTlsResolver;
        runtime.Path = relativeDirectory + "/" + image.OutputName;
        runtime.Names = {Io::Utf8Path(image.SourcePath.filename()), image.Soname};
        runtime.DeferInitialization = deferred(index);
        std::vector<std::uint8_t> output;
        const auto message = [&](std::string_view text) { progress.Message(text); };
        if (windows) output = Elfpatcher::GuestModuleWriter().WriteWindows(image, runtime, message);
        else {
            std::string guestRunPath = runPath;
            const auto relativeRoot = outputDirectory.lexically_relative(destination.lexically_normal()).generic_string();
            for (std::size_t position = 0; (position = guestRunPath.find("$ORIGIN", position)) != std::string::npos;) {
                position += 7;
                if (position != guestRunPath.size() && guestRunPath[position] != '/' && guestRunPath[position] != ':') continue;
                guestRunPath.insert(position, "/" + relativeRoot);
                position += relativeRoot.size() + 1;
            }
            std::vector<std::string> needed;
            for (const auto dependency : dependencies[index]) {
                const auto dependencyPath = images[dependency].SourcePath.parent_path() / images[dependency].OutputName;
                needed.push_back("$ORIGIN/" + dependencyPath.lexically_relative(image.SourcePath.parent_path()).generic_string());
            }
            needed.insert(needed.end(), moduleHostLibraries[index].begin(), moduleHostLibraries[index].end());
            const auto directory = image.SourcePath.parent_path();
            const bool deferLinuxStart = isRuntime(index) && directory != root / "sce_module" &&
                directory != root / "sce_modules" && directory != root / "prx";
            output = macos ? Elfpatcher::GuestModuleWriter().WriteMacOs(image, needed, guestRunPath, message) : Elfpatcher::GuestModuleWriter().WriteLinux(image, needed, guestRunPath, deferLinuxStart);
        }
        if (windows) {
            for (const auto& [name, provider] : guestNames) {
                if (provider == index && std::find(runtime.Names.begin(), runtime.Names.end(), name) == runtime.Names.end()) runtime.Names.push_back(name);
            }
        }
        std::vector<Domain::CallRegistryEntry> imports;
        for (const auto* table : {&image.Dynamic.RelaData, &image.Dynamic.RelaPltData}) {
            for (std::size_t position = 0; position < table->size(); position += 24) {
                const auto info = Io::ReadU64(*table, position + 8);
                if ((info & 0xffffffffu) == 0) continue;
                const auto symbolIndex = static_cast<std::size_t>(info >> 32);
                if (!systemImports[index].contains(symbolIndex)) continue;
                const auto& symbol = image.Symbols[symbolIndex];
                imports.push_back({symbol.Name, symbol.Library, {}, 0, {}, Io::ReadU64(*table, position), {}, false, (symbol.Info >> 4) == 2});
            }
        }
        dynamic.GuestModules.push_back(std::move(runtime));
        std::vector<std::string> neededLibraries;
        for (const auto& name : image.Dependencies)
            if (findGuest(name) == guestNames.end()) neededLibraries.push_back(hostLibrary(name));
        artifacts.push_back({target, std::move(output), std::move(imports), image.SourcePath, std::move(neededLibraries)});
        progress.Report({Domain::ProgressStage::Modules, Io::Utf8Path(image.SourcePath), artifacts.size(), order.size()});
    }
    return artifacts;
}

}
