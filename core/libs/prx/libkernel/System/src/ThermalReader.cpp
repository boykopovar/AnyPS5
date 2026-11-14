#include "prx/libkernel/System/include/ThermalSensors.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <optional>
#include <string>
#include <system_error>
#include <vector>

namespace {

std::optional<int> ReadCelsius(const std::filesystem::path& file) {
    std::ifstream stream(file);
    long long millidegrees = 0;
    if (!(stream >> millidegrees)) return std::nullopt;
    if (millidegrees < -273150 || millidegrees > 2000000) return std::nullopt;
    return static_cast<int>(millidegrees / 1000);
}

std::string ReadName(const std::filesystem::path& file) {
    std::ifstream stream(file);
    std::string name;
    std::getline(stream, name);
    return name;
}

bool Contains(const std::string& text, const char* part) {
    return text.find(part) != std::string::npos;
}

std::string Lowercase(std::string text) {
    for (char& character : text) character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
    return text;
}

bool ContainsAny(const std::string& text, std::initializer_list<const char*> parts) {
    for (const char* part : parts)
        if (text.find(part) != std::string::npos) return true;
    return false;
}

bool IsPowerOrNetwork(const std::string& name) {
    if (name.rfind("bat", 0) == 0) return true;
    if (name == "ac" || name.rfind("acad", 0) == 0 || name.rfind("ucsi", 0) == 0) return true;
    for (const char* part : {"iwl", "mt79", "ath", "rtw", "wlan", "wifi", "wireless"})
        if (name.find(part) != std::string::npos) return true;
    return false;
}

std::filesystem::directory_iterator OpenDirectory(const std::filesystem::path& path) {
    std::error_code error;
    std::filesystem::directory_iterator entries(path, error);
    if (error) return {};
    return entries;
}

std::vector<std::filesystem::path> SortedEntries(const std::filesystem::path& directory) {
    std::vector<std::filesystem::path> entries;
    for (const auto& entry : OpenDirectory(directory)) entries.push_back(entry.path());
    std::sort(entries.begin(), entries.end());
    return entries;
}

}  // namespace

extern "C" void ReadThermalSensors_nid_no_patch(const char* root, int celsius[3]) {
    celsius[0] = celsius[1] = celsius[2] = -1;
    if (root == nullptr) return;
    const std::filesystem::path base(root);

    int gpu = -1;
    int board = -1;
    for (const auto& path : SortedEntries(base / "hwmon")) {
        const auto value = ReadCelsius(path / "temp1_input");
        if (!value) continue;
        const std::string name = Lowercase(ReadName(path / "name"));
        if (IsPowerOrNetwork(name)) continue;
        if (ContainsAny(name, {"coretemp", "k10temp", "zenpower", "cpu"})) {
            if (celsius[0] < 0) celsius[0] = *value;
        } else if (ContainsAny(name, {"amdgpu", "i915", "radeon", "nvidia", "gpu"}) || name == "xe") {
            if (gpu < 0) gpu = *value;
        } else if (Contains(name, "nvme")) {
            if (celsius[2] < 0) celsius[2] = *value;
        } else if (board < 0) {
            board = *value;
        }
    }
    for (const auto& path : SortedEntries(base / "thermal")) {
        const auto value = ReadCelsius(path / "temp");
        if (!value) continue;
        const std::string type = Lowercase(ReadName(path / "type"));
        if (ContainsAny(type, {"x86_pkg_temp", "cpu-thermal"})) {
            if (celsius[0] < 0) celsius[0] = *value;
        } else if (Contains(type, "gpu")) {
            if (gpu < 0) gpu = *value;
        } else if (board < 0) {
            board = *value;
        }
    }
    if (celsius[0] < 0) celsius[0] = gpu;
    if (celsius[1] < 0) celsius[1] = board;
    Plausible(celsius[0]);
    Plausible(celsius[1]);
    Plausible(celsius[2]);
}
