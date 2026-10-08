#include "prx/libkernel/AppMetadata/include/Addcont.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <string>
#ifdef _WIN32
#include <windows.h>
#endif

namespace {

constexpr std::size_t LabelLength = 16;
constexpr std::size_t ContentIdLength = 36;
constexpr std::size_t ContentIdLabelOffset = 20;

std::filesystem::path ExecutableDirectory() {
#ifdef _WIN32
    wchar_t module[MAX_PATH];
    const auto length = GetModuleFileNameW(nullptr, module, MAX_PATH);
    if (length == 0 || length == MAX_PATH) throw std::runtime_error("Addcont: cannot locate the executable");
    return std::filesystem::path(module).parent_path();
#else
    return std::filesystem::read_symlink("/proc/self/exe").parent_path();
#endif
}

const char* Configured(const char* variable) {
    const char* configured = std::getenv(variable);
    return configured != nullptr && configured[0] != '\0' ? configured : nullptr;
}

AddcontEntry& Add(std::vector<AddcontEntry>& entries, const std::string& label) {
    for (auto& entry : entries) {
        if (label == entry.label) return entry;
    }
    AddcontEntry& entry = entries.emplace_back();
    std::memset(entry.label, 0, sizeof(entry.label));
    std::memcpy(entry.label, label.data(), label.size());
    entry.hasData = false;
    return entry;
}

void ReadEntitlements(std::vector<AddcontEntry>& entries) {
    const char* configured = Configured("ANYPS5_ENTITLEMENTS");
    const auto path = configured != nullptr ? std::filesystem::path(configured) : ExecutableDirectory() / "anyps5-entitlements.ini";
    std::ifstream file(path);
    if (!file) {
        if (configured != nullptr || std::filesystem::exists(path)) throw std::runtime_error("NpEntitlementAccess: cannot read " + path.string());
        return;
    }
    std::string line;
    while (std::getline(file, line)) {
        line.erase(std::min(line.find_first_of("#;"), line.size()));
        const auto first = line.find_first_not_of(" \t\r");
        if (first == std::string::npos) continue;
        const auto label = line.substr(first, line.find_last_not_of(" \t\r") + 1 - first);
        if (label.size() > LabelLength) throw std::runtime_error("NpEntitlementAccess: entitlement label '" + label + "' in " + path.string() + " is longer than 16 characters");
        Add(entries, label);
    }
}

std::string DirectoryLabel(const std::string& name) {
    const bool contentId = name.size() == ContentIdLength && name[6] == '-' && name[16] == '_' && name[19] == '-';
    std::string label = contentId ? name.substr(ContentIdLabelOffset) : name;
    const bool valid = !label.empty() && label.size() <= LabelLength &&
        std::all_of(label.begin(), label.end(), [](unsigned char character) { return std::isalnum(character) != 0; });
    return valid ? label : std::string();
}

void ReadDirectories(std::vector<AddcontEntry>& entries) {
    const char* configured = Configured("ANYPS5_ADDCONT");
    const auto root = configured != nullptr ? std::filesystem::path(configured) : ExecutableDirectory() / "anyps5-addcont";
    if (!std::filesystem::exists(root)) {
        if (configured != nullptr) throw std::runtime_error("Addcont: " + root.string() + " does not exist");
        return;
    }
    if (!std::filesystem::is_directory(root)) throw std::runtime_error("Addcont: " + root.string() + " is not a directory");
    std::vector<std::pair<std::string, std::filesystem::path>> found;
    for (const auto& item : std::filesystem::directory_iterator(root)) {
        if (!item.is_directory()) continue;
        const auto name = item.path().filename().string();
        const auto label = DirectoryLabel(name);
        if (label.empty()) throw std::runtime_error("Addcont: directory '" + name + "' in " + root.string() + " is neither an entitlement label nor a content ID");
        for (const auto& [other, otherPath] : found) {
            if (other == label) throw std::runtime_error("Addcont: " + otherPath.string() + " and " + item.path().string() + " are both entitlement " + label);
        }
        found.emplace_back(label, std::filesystem::absolute(item.path()));
    }
    std::sort(found.begin(), found.end());
    for (const auto& [label, directory] : found) {
        auto& entry = Add(entries, label);
        entry.hasData = true;
        entry.directory = directory;
    }
}

}

extern "C" const std::vector<AddcontEntry>& AddcontEntries_nid_no_patch() {
    static const auto entries = [] {
        std::vector<AddcontEntry> result;
        ReadEntitlements(result);
        ReadDirectories(result);
        return result;
    }();
    return entries;
}
