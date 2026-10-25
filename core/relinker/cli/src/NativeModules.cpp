#include <NativeModules.hpp>
#include <io/FileReader.hpp>
#include <io/NativePath.hpp>
#include <io/FileIdentity.hpp>
#include <domain/ImportModule.hpp>
#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <string_view>
#include <system_error>
#include <tuple>

namespace Cli {

namespace {

struct CandidateHeader {
    bool Elf = false;
    bool Self = false;
    std::uint16_t Type = 0;
};

std::optional<CandidateHeader> TryReadCandidateHeader(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) return std::nullopt;
    char header[18]{};
    stream.read(header, sizeof header);
    if (stream.bad()) return std::nullopt;
    CandidateHeader result;
    if (stream.gcount() < 4) return result;
    const auto byte = [&](const std::size_t index) { return static_cast<unsigned char>(header[index]); };
    result.Self = (byte(0) == 0x4f && byte(1) == 0x15 && byte(2) == 0x3d && byte(3) == 0x1d) || (byte(0) == 0x54 && byte(1) == 0x14 && byte(2) == 0xf5 && byte(3) == 0xee);
    result.Elf = byte(0) == 0x7f && header[1] == 'E' && header[2] == 'L' && header[3] == 'F';
    if (result.Elf && stream.gcount() == sizeof header) result.Type = static_cast<std::uint16_t>(byte(16) | (byte(17) << 8));
    return result;
}

CandidateHeader ReadCandidateHeader(const std::filesystem::path& path) {
    const auto header = TryReadCandidateHeader(path);
    if (!header) throw Domain::RelinkerException("Cannot read guest candidate: " + Io::Utf8Path(path));
    return *header;
}

std::string FoldedName(std::string name, const bool foldCase) {
    if (foldCase) for (auto& character : name) if (character >= 'A' && character <= 'Z') character = static_cast<char>(character + ('a' - 'A'));
    return name;
}

std::string_view ModuleStem(const std::string_view name) {
    return name.substr(0, name.size() - Domain::ModuleSuffix(name).size());
}

}

bool SameDirectory(const std::filesystem::path& first, const std::filesystem::path& second) {
    if (std::filesystem::exists(first) && std::filesystem::exists(second)) return std::filesystem::equivalent(first, second);
    return std::filesystem::weakly_canonical(first) == std::filesystem::weakly_canonical(second);
}

NativeModules ReadModules(const std::filesystem::path& inputPath, const std::filesystem::path& sceModulePath, const std::set<std::string>& excludedModules, const std::vector<std::string>& moduleDirectories) {
    using namespace Relinker;
    const auto root = std::filesystem::absolute(sceModulePath).lexically_normal();
    if (!std::filesystem::exists(root)) throw Domain::RelinkerException("Guest module parent directory does not exist: " + Io::Utf8Path(root));
    if (!std::filesystem::is_directory(root)) throw Domain::RelinkerException("Guest module parent path is not a directory: " + Io::Utf8Path(root));
    const auto missingModuleMessage = std::string(GuestModulePattern) + " was not found in: " + Io::Utf8Path(root) + ". Use --skip-sce-module to disable guest module processing.";
    const auto singular = root / "sce_module";
    const auto plural = root / "sce_modules";
    const auto prx = root / "prx";
    const bool hasSingular = std::filesystem::exists(singular);
    const bool hasPlural = std::filesystem::exists(plural);
    const bool hasPrx = std::filesystem::exists(prx);
    if (hasSingular && hasPlural) throw Domain::RelinkerException("Both sce_module and sce_modules exist in the guest module parent directory");
    if (!hasSingular && !hasPlural && !hasPrx && moduleDirectories.empty()) throw Domain::RelinkerException(missingModuleMessage);
    std::vector<std::filesystem::path> directories;
    if (hasSingular || hasPlural) directories.push_back(hasSingular ? singular : plural);
    if (hasPrx) directories.push_back(prx);
    std::vector<std::filesystem::path> runtimeDirectories;
    for (const auto& name : moduleDirectories) {
        const auto relative = Io::NativePath(name).lexically_normal();
        if (relative.empty() || relative.has_root_path() || *relative.begin() == "..")
            throw Domain::RelinkerException("--module-dir must be a directory inside the guest module parent directory: " + name);
        auto directory = (root / relative).lexically_normal();
        if (!directory.has_filename()) directory = directory.parent_path();
        if (!std::filesystem::is_directory(directory)) throw Domain::RelinkerException("Guest module path is not a directory: " + Io::Utf8Path(directory));
        if (std::any_of(directories.begin(), directories.end(), [&](const auto& existing) { return SameDirectory(existing, directory); })) throw Domain::RelinkerException("Duplicate guest module directory: " + Io::Utf8Path(directory));
        directories.push_back(directory);
        runtimeDirectories.push_back(directory);
    }
    std::vector<std::filesystem::path> paths;
    std::set<std::string> unmatchedExclusions = excludedModules;
    for (const auto& directory : directories) {
        if (!std::filesystem::is_directory(directory)) throw Domain::RelinkerException("Guest module path is not a directory: " + Io::Utf8Path(directory));
        for (const auto& entry : std::filesystem::directory_iterator(directory)) {
            if (Io::Utf8Path(entry.path().filename()).ends_with(GuestModuleSuffix)) continue;
            if (excludedModules.contains(Io::Utf8Path(entry.path().filename()))) {
                unmatchedExclusions.erase(Io::Utf8Path(entry.path().filename()));
                continue;
            }
            if (!entry.is_regular_file()) continue;
            if (Io::SameFile(entry.path(), inputPath)) continue;
            const auto header = ReadCandidateHeader(entry.path());
            if (header.Elf) paths.push_back(entry.path());
            else if (header.Self)
                std::cout << "Skipping SELF container: " << entry.path().lexically_relative(root).generic_string()
                          << " (unwrap it before relinking)\n";
        }
    }
    if (!unmatchedExclusions.empty()) throw Domain::RelinkerException("Excluded guest module file not found: " + *unmatchedExclusions.begin());
    std::sort(paths.begin(), paths.end());
    NativeModules result;
    result.Root = root;
    result.Directories = std::move(directories);
    result.RuntimeDirectories = std::move(runtimeDirectories);
    for (const auto& path : paths) result.Modules.push_back({path, Io::FileReader().Read(Io::Utf8Path(path))});
    std::map<std::size_t, std::string> sonames;
    const auto soname = [&](std::size_t index) -> const std::string& {
        const auto [found, inserted] = sonames.try_emplace(index);
        if (inserted) found->second = GuestImageReader().Read(result.Modules[index].Path, result.Modules[index].Bytes, true).Soname;
        return found->second;
    };
    std::vector<bool> copies(result.Modules.size());
    for (std::size_t copy = 0; copy < result.Modules.size(); ++copy) {
        const auto& path = result.Modules[copy].Path;
        const auto name = Io::Utf8Path(path.filename());
        for (std::size_t original = 0; original < result.Modules.size(); ++original) {
            const auto& kept = result.Modules[original].Path;
            const auto prefix = Io::Utf8Path(kept.filename());
            if (kept.parent_path() != path.parent_path() || prefix.size() >= name.size() || !name.starts_with(prefix)) continue;
            if (soname(copy).empty() || soname(copy) != soname(original)) continue;
            copies[copy] = true;
            std::cout << "Skipping backup copy: " << path.lexically_relative(root).generic_string() << " (same SONAME as "
                      << kept.lexically_relative(root).generic_string() << ")\n";
            break;
        }
    }
    std::vector<GuestModuleInput> modules;
    for (std::size_t index = 0; index < result.Modules.size(); ++index)
        if (!copies[index]) modules.push_back(std::move(result.Modules[index]));
    result.Modules = std::move(modules);
    return result;
}

void ReportUnconvertedNeeded(const NativeModules& modules, const std::vector<std::string>& neededLibraries, const bool foldCase, std::ostream& out) {
    std::map<std::string, std::vector<std::string>> stems;
    for (const auto& name : neededLibraries) {
        const auto folded = FoldedName(name, foldCase);
        if (!Domain::ModuleSuffix(folded).empty()) stems[std::string(ModuleStem(folded))].push_back(name);
    }
    if (stems.empty()) return;
    struct Found {
        std::filesystem::path Relative;
        std::string Name;
        bool Self;
    };
    std::vector<Found> found;
    std::error_code error;
    for (auto entry = std::filesystem::recursive_directory_iterator(modules.Root, std::filesystem::directory_options::skip_permission_denied, error); !error && entry != std::filesystem::recursive_directory_iterator(); entry.increment(error)) {
        const bool directory = entry->is_directory(error);
        if (error) {
            error.clear();
            continue;
        }
        if (directory) {
            if (std::any_of(modules.Directories.begin(), modules.Directories.end(), [&](const auto& scanned) { return std::filesystem::equivalent(scanned, entry->path(), error) && !error; })) entry.disable_recursion_pending();
            error.clear();
            continue;
        }
        const auto filename = FoldedName(Io::Utf8Path(entry->path().filename()), foldCase);
        if (Domain::ModuleSuffix(filename).empty()) continue;
        const auto stem = stems.find(std::string(ModuleStem(filename)));
        if (stem == stems.end()) continue;
        if (!entry->is_regular_file(error) || error) {
            error.clear();
            continue;
        }
        const auto header = TryReadCandidateHeader(entry->path());
        if (!header || (!header->Self && !(header->Elf && header->Type == 0xfe18))) continue;
        for (const auto& name : stem->second) found.push_back({entry->path().lexically_relative(modules.Root), name, header->Self});
    }
    std::sort(found.begin(), found.end(), [](const auto& left, const auto& right) { return std::tie(left.Relative, left.Name) < std::tie(right.Relative, right.Name); });
    for (const auto& [relative, name, self] : found) {
        const auto directory = relative.parent_path().generic_string();
        out << "Needed module not converted: " << name << " at " << relative.generic_string() << "; add --module-dir " << (directory.empty() ? "." : directory)
            << (self ? " and unwrap the SELF container" : "") << '\n';
    }
}

}
