#include "prx/libSceUsbStorage/include/HostVolumes.hpp"

#include <algorithm>
#include <charconv>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <iterator>
#include <mutex>
#include <optional>
#include <set>
#include <string_view>
#include <system_error>
#include <utility>

namespace UsbStorage {
namespace {

std::mutex rootsMutex;
std::optional<std::pair<std::filesystem::path, std::filesystem::path>> testRoots;
std::optional<std::vector<HostVolume>> testVolumes;

std::filesystem::path Utf8Path(std::string_view text) {
    return std::filesystem::path(std::u8string(text.begin(), text.end()));
}

std::optional<std::string> ReadText(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return std::nullopt;
    std::string text((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    if (file.bad()) return std::nullopt;
    while (!text.empty() && (text.back() == '\n' || text.back() == '\r' || text.back() == ' ' || text.back() == '\t')) text.pop_back();
    return text;
}

std::optional<std::uint16_t> ReadHex(const std::filesystem::path& path) {
    const auto text = ReadText(path);
    if (!text || text->empty() || text->size() > 4) return std::nullopt;
    std::uint16_t value = 0;
    const auto [end, error] = std::from_chars(text->data(), text->data() + text->size(), value, 16);
    if (error != std::errc{} || end != text->data() + text->size()) return std::nullopt;
    return value;
}

std::string Unescape(std::string_view text) {
    std::string result;
    const auto octal = [&](std::size_t at) { return text[at] >= '0' && text[at] <= '7'; };
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '\\' && i + 3 < text.size() && octal(i + 1) && octal(i + 2) && octal(i + 3)) {
            result += static_cast<char>((text[i + 1] - '0') * 64 + (text[i + 2] - '0') * 8 + (text[i + 3] - '0'));
            i += 3;
            continue;
        }
        result += text[i];
    }
    return result;
}

std::vector<std::string_view> Fields(std::string_view line) {
    std::vector<std::string_view> fields;
    while (!line.empty()) {
        const auto separator = line.find(' ');
        if (separator != 0) fields.push_back(line.substr(0, separator));
        if (separator == std::string_view::npos) break;
        line.remove_prefix(separator + 1);
    }
    return fields;
}

bool ReadOnly(std::string_view options) {
    while (true) {
        const auto separator = options.find(',');
        if (options.substr(0, separator) == "ro") return true;
        if (separator == std::string_view::npos) return false;
        options.remove_prefix(separator + 1);
    }
}

std::optional<FileSystem> FileSystemOf(std::string_view type) {
    if (type == "vfat" || type == "msdos") return FileSystem::Fat;
    if (type == "exfat") return FileSystem::ExFat;
    return std::nullopt;
}

std::optional<HostVolume> Describe(const std::filesystem::path& sysRoot, std::string_view device, const std::string& mountPoint,
                                   FileSystem fileSystem, bool readOnly) {
    std::error_code error;
    const auto top = std::filesystem::weakly_canonical(sysRoot, error);
    if (error) return std::nullopt;
    const auto block = std::filesystem::canonical(sysRoot / "dev" / "block" / std::filesystem::path(std::string(device)), error);
    if (error) return std::nullopt;
    std::optional<std::filesystem::path> scsi;
    std::optional<std::filesystem::path> usb;
    for (auto current = block; current != top && current.has_relative_path(); current = current.parent_path()) {
        if (!scsi && std::filesystem::is_regular_file(current / "vendor", error) && std::filesystem::is_regular_file(current / "model", error)) {
            scsi = current;
        }
        if (ReadText(current / "bInterfaceClass") == "08") {
            usb = current.parent_path();
            break;
        }
    }
    if (!usb) return std::nullopt;
    const auto vendorId = ReadHex(*usb / "idVendor");
    const auto productId = ReadHex(*usb / "idProduct");
    const auto bcdDevice = ReadHex(*usb / "bcdDevice");
    if (!vendorId || !productId || !bcdDevice) return std::nullopt;
    const auto root = Utf8Path(mountPoint);
    if (!std::filesystem::is_directory(root, error)) return std::nullopt;
    const auto space = std::filesystem::space(root, error);
    if (error) return std::nullopt;
    HostVolume volume;
    volume.key = std::string(device) + ' ' + mountPoint;
    volume.root = root;
    volume.volumeSize = space.capacity;
    volume.vendorId = *vendorId;
    volume.productId = *productId;
    volume.bcdDevice = *bcdDevice;
    if (scsi) {
        volume.vendor = ReadText(*scsi / "vendor").value_or(std::string());
        volume.product = ReadText(*scsi / "model").value_or(std::string());
    }
    volume.serialNumber = ReadText(*usb / "serial").value_or(std::string());
    volume.readOnly = readOnly;
    volume.fileSystem = fileSystem;
    return volume;
}

}

std::vector<HostVolume> ScanSysfsVolumes(const std::filesystem::path& mountInfo, const std::filesystem::path& sysRoot) {
    std::vector<HostVolume> volumes;
    std::ifstream file(mountInfo, std::ios::binary);
    if (!file) return volumes;
    std::set<std::string, std::less<>> devices;
    std::string line;
    while (std::getline(file, line)) {
        const auto fields = Fields(line);
        const auto separator = std::find(fields.begin(), fields.end(), std::string_view("-"));
        if (separator - fields.begin() < 6 || fields.end() - separator < 3) continue;
        const auto fileSystem = FileSystemOf(separator[1]);
        if (!fileSystem || fields[3] != "/" || devices.contains(fields[2])) continue;
        devices.emplace(fields[2]);
        if (auto volume = Describe(sysRoot, fields[2], Unescape(fields[4]), *fileSystem, ReadOnly(fields[5]))) volumes.push_back(std::move(*volume));
    }
    return volumes;
}

namespace {

bool PassthroughEnabled() {
    const char* configured = std::getenv("APS5_USB_STORAGE_PASSTHROUGH");
    return configured != nullptr && configured[0] != '\0';
}

}

std::vector<HostVolume> ScanHostVolumes() {
    if (!PassthroughEnabled()) return {};
    std::optional<std::pair<std::filesystem::path, std::filesystem::path>> roots;
    {
        std::lock_guard lock(rootsMutex);
        if (testVolumes) return *testVolumes;
        roots = testRoots;
    }
    if (roots) return ScanSysfsVolumes(roots->first, roots->second);
#ifdef _WIN32
    return ScanWindowsVolumes();
#elif defined(__linux__)
    return ScanSysfsVolumes("/proc/self/mountinfo", "/sys");
#else
    return {};
#endif
}

}

extern "C" void SetUsbStorageHostRootsForTest_nid_no_patch(const char* mountInfo, const char* sysRoot) {
    std::lock_guard lock(UsbStorage::rootsMutex);
    UsbStorage::testVolumes.reset();
    if (mountInfo == nullptr || sysRoot == nullptr) {
        UsbStorage::testRoots.reset();
        return;
    }
    UsbStorage::testRoots.emplace(UsbStorage::Utf8Path(mountInfo), UsbStorage::Utf8Path(sysRoot));
}

extern "C" void SetUsbStorageHostVolumesForTest_nid_no_patch(const UsbStorageTestVolume* volumes, std::size_t count) {
    std::lock_guard lock(UsbStorage::rootsMutex);
    UsbStorage::testRoots.reset();
    if (volumes == nullptr) {
        UsbStorage::testVolumes.reset();
        return;
    }
    const auto text = [](const char* value) { return value == nullptr ? std::string() : std::string(value); };
    std::vector<UsbStorage::HostVolume> copies;
    for (std::size_t i = 0; i < count; ++i) {
        UsbStorage::HostVolume volume;
        volume.key = text(volumes[i].key);
        volume.root = UsbStorage::Utf8Path(text(volumes[i].root));
        volume.volumeSize = volumes[i].volumeSize;
        volume.vendorId = volumes[i].vendorId;
        volume.productId = volumes[i].productId;
        volume.bcdDevice = volumes[i].bcdDevice;
        volume.vendor = text(volumes[i].vendor);
        volume.product = text(volumes[i].product);
        volume.serialNumber = text(volumes[i].serialNumber);
        volume.readOnly = volumes[i].readOnly;
        volume.fileSystem = volumes[i].exFat ? UsbStorage::FileSystem::ExFat : UsbStorage::FileSystem::Fat;
        copies.push_back(std::move(volume));
    }
    UsbStorage::testVolumes = std::move(copies);
}
