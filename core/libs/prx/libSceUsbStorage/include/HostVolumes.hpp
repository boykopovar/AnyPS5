#ifndef CORE_LIBS_PRX_LIBSCEUSBSTORAGE_INCLUDE_HOSTVOLUMES_HPP
#define CORE_LIBS_PRX_LIBSCEUSBSTORAGE_INCLUDE_HOSTVOLUMES_HPP

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace UsbStorage {

enum class FileSystem {
    Fat,
    ExFat,
};

struct HostVolume {
    std::string key;
    std::filesystem::path root;
    std::uint64_t volumeSize = 0;
    std::uint16_t vendorId = 0;
    std::uint16_t productId = 0;
    std::uint16_t bcdDevice = 0;
    std::string vendor;
    std::string product;
    std::string serialNumber;
    bool readOnly = false;
    FileSystem fileSystem = FileSystem::Fat;
};

std::vector<HostVolume> ScanSysfsVolumes(const std::filesystem::path& mountInfo, const std::filesystem::path& sysRoot);
#ifdef _WIN32
std::vector<HostVolume> ScanWindowsVolumes();
#endif
std::vector<HostVolume> ScanHostVolumes();

}

struct UsbStorageTestVolume {
    const char* key;
    const char* root;
    std::uint64_t volumeSize;
    std::uint16_t vendorId;
    std::uint16_t productId;
    std::uint16_t bcdDevice;
    const char* vendor;
    const char* product;
    const char* serialNumber;
    bool readOnly;
    bool exFat;
};

extern "C" void SetUsbStorageHostRootsForTest_nid_no_patch(const char* mountInfo, const char* sysRoot);
extern "C" void SetUsbStorageHostVolumesForTest_nid_no_patch(const UsbStorageTestVolume* volumes, std::size_t count);

#endif
