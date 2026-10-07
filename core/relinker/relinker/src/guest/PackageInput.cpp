#include <relinker/guest/PackageInput.hpp>
#include <domain/Types.hpp>
#include <pkg/Package.hpp>
#include <pkg/SelfImage.hpp>
#include <algorithm>

namespace Relinker {

namespace {

bool isElf(const std::vector<std::uint8_t>& bytes) {
    return bytes.size() >= 4 && bytes[0] == 0x7f && bytes[1] == 'E' && bytes[2] == 'L' && bytes[3] == 'F';
}

std::vector<std::uint8_t> readExecutable(const Pkg::Package& package, const Pkg::PackageNode& node, const std::string& path) {
    auto bytes = package.ReadAll(node);
    if (!Pkg::IsSelfImage(bytes.data(), bytes.size())) return bytes;
    try {
        return Pkg::UnwrapSelfImage(bytes);
    } catch (const std::exception& error) {
        throw Domain::RelinkerException(path + ": " + error.what());
    }
}

std::string utf8(const std::filesystem::path& path) {
    const auto text = path.u8string();
    return std::string(text.begin(), text.end());
}

}

PackageInput ReadPackageInput(const std::filesystem::path& packagePath, const std::filesystem::path& oodle, const bool readModules, const std::set<std::string>& excludedModules) {
    const Pkg::Package package(packagePath, oodle);
    PackageInput input;
    input.ContentId = package.ContentId();
    input.PackageSize = package.FileSize();
    const auto* executable = package.Find("eboot.bin");
    if (executable == nullptr || executable->Directory) throw Domain::RelinkerException("Package does not contain eboot.bin");
    input.Executable = readExecutable(package, *executable, "eboot.bin");
    if (const auto* icon = package.Find("sce_sys/icon0.png"); icon != nullptr && !icon->Directory) input.Icon = package.ReadAll(*icon);
    if (!readModules) return input;

    const auto* singular = package.Find("sce_module");
    const auto* plural = package.Find("sce_modules");
    const auto* prx = package.Find("prx");
    if (singular != nullptr && plural != nullptr) throw Domain::RelinkerException("Both sce_module and sce_modules exist in the package");
    if (singular == nullptr && plural == nullptr && prx == nullptr)
        throw Domain::RelinkerException("sce_module/sce_modules/prx was not found in the package: " + packagePath.string() + ". Use --skip-sce-module only if this game can run without these modules.");
    const auto root = std::filesystem::absolute(packagePath).parent_path();
    std::set<std::string> unmatchedExclusions = excludedModules;
    for (const auto* directory : {singular != nullptr ? singular : plural, prx}) {
        if (directory == nullptr) continue;
        if (!directory->Directory) throw Domain::RelinkerException("Guest module path in the package is not a directory: " + directory->Name);
        for (const auto index : directory->Children) {
            const auto& child = package.Node(index);
            if (child.Name.ends_with(GuestModuleSuffix)) continue;
            if (excludedModules.contains(child.Name)) {
                unmatchedExclusions.erase(child.Name);
                continue;
            }
            if (child.Directory) continue;
            auto bytes = readExecutable(package, child, directory->Name + "/" + child.Name);
            if (isElf(bytes)) input.Modules.push_back(GuestModuleSource{root / directory->Name / child.Name, std::move(bytes), false});
        }
    }
    if (!unmatchedExclusions.empty()) throw Domain::RelinkerException("Excluded guest module file not found: " + *unmatchedExclusions.begin());
    std::sort(input.Modules.begin(), input.Modules.end(), [](const GuestModuleSource& left, const GuestModuleSource& right) { return left.Path < right.Path; });
    return input;
}

std::string FormatPackageSidecar(const std::filesystem::path& package, const PackageInput& input, const std::filesystem::path& oodle) {
    return "package=" + utf8(std::filesystem::absolute(package).make_preferred()) + "\n" +
           "size=" + std::to_string(input.PackageSize) + "\n" +
           "content_id=" + input.ContentId + "\n" +
           "oodle=" + (oodle.empty() ? std::string{} : utf8(std::filesystem::absolute(oodle).make_preferred())) + "\n";
}

}
