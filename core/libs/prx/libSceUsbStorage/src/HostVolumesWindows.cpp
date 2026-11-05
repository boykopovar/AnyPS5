#include "prx/libSceUsbStorage/include/HostVolumes.hpp"

#include <windows.h>
#include <initguid.h>
#include <winioctl.h>
#include <setupapi.h>
#include <cfgmgr32.h>

#include <cstring>
#include <cwchar>
#include <memory>
#include <optional>
#include <string_view>

namespace UsbStorage {
namespace {

struct Handle {
    HANDLE value;
    ~Handle() {
        if (value != INVALID_HANDLE_VALUE) CloseHandle(value);
    }
};

struct ErrorMode {
    DWORD previous = 0;
    ErrorMode() { SetThreadErrorMode(SEM_FAILCRITICALERRORS | SEM_NOOPENFILEERRORBOX, &previous); }
    ~ErrorMode() { SetThreadErrorMode(previous, nullptr); }
};

struct UsbIdentity {
    std::uint16_t vendorId;
    std::uint16_t productId;
    std::uint16_t bcdDevice;
};

std::string Trimmed(std::string text) {
    while (!text.empty() && (text.back() == ' ' || text.back() == '\t')) text.pop_back();
    while (!text.empty() && (text.front() == ' ' || text.front() == '\t')) text.erase(text.begin());
    return text;
}

std::string DescriptorString(const std::vector<BYTE>& descriptor, DWORD size, DWORD offset) {
    if (offset == 0 || offset >= size) return {};
    const auto* begin = reinterpret_cast<const char*>(descriptor.data() + offset);
    return Trimmed(std::string(begin, strnlen(begin, size - offset)));
}

std::optional<std::uint16_t> Hex(std::wstring_view text) {
    if (text.size() < 4) return std::nullopt;
    std::uint16_t value = 0;
    for (std::size_t i = 0; i < 4; ++i) {
        const wchar_t digit = text[i];
        value = static_cast<std::uint16_t>(value << 4);
        if (digit >= L'0' && digit <= L'9') value = static_cast<std::uint16_t>(value | (digit - L'0'));
        else if (digit >= L'a' && digit <= L'f') value = static_cast<std::uint16_t>(value | (digit - L'a' + 10));
        else if (digit >= L'A' && digit <= L'F') value = static_cast<std::uint16_t>(value | (digit - L'A' + 10));
        else return std::nullopt;
    }
    return value;
}

std::optional<std::uint16_t> TaggedHex(std::wstring_view text, std::wstring_view tag) {
    for (std::size_t at = 0; at + tag.size() <= text.size(); ++at) {
        if (_wcsnicmp(text.data() + at, tag.data(), tag.size()) == 0) return Hex(text.substr(at + tag.size()));
    }
    return std::nullopt;
}

std::optional<STORAGE_DEVICE_NUMBER> DeviceNumber(HANDLE handle) {
    STORAGE_DEVICE_NUMBER number{};
    DWORD bytes = 0;
    if (!DeviceIoControl(handle, IOCTL_STORAGE_GET_DEVICE_NUMBER, nullptr, 0, &number, sizeof(number), &bytes, nullptr)) return std::nullopt;
    return number;
}

std::optional<DEVINST> DiskInstance(DWORD diskNumber) {
    const HDEVINFO set = SetupDiGetClassDevsW(&GUID_DEVINTERFACE_DISK, nullptr, nullptr, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (set == INVALID_HANDLE_VALUE) return std::nullopt;
    const std::unique_ptr<void, decltype(&SetupDiDestroyDeviceInfoList)> owner(set, &SetupDiDestroyDeviceInfoList);
    SP_DEVICE_INTERFACE_DATA interfaceData{};
    interfaceData.cbSize = sizeof(interfaceData);
    for (DWORD index = 0; SetupDiEnumDeviceInterfaces(set, nullptr, &GUID_DEVINTERFACE_DISK, index, &interfaceData); ++index) {
        DWORD required = 0;
        SetupDiGetDeviceInterfaceDetailW(set, &interfaceData, nullptr, 0, &required, nullptr);
        if (required < sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W)) continue;
        std::vector<DWORD> storage((required + sizeof(DWORD) - 1) / sizeof(DWORD));
        auto* detail = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W*>(storage.data());
        detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);
        SP_DEVINFO_DATA deviceData{};
        deviceData.cbSize = sizeof(deviceData);
        if (!SetupDiGetDeviceInterfaceDetailW(set, &interfaceData, detail, required, nullptr, &deviceData)) continue;
        const Handle disk{CreateFileW(detail->DevicePath, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr)};
        if (disk.value == INVALID_HANDLE_VALUE) continue;
        const auto number = DeviceNumber(disk.value);
        if (number && number->DeviceNumber == diskNumber) return deviceData.DevInst;
    }
    return std::nullopt;
}

std::optional<UsbIdentity> Identity(DEVINST instance) {
    for (DEVINST current = instance;;) {
        DEVINST parent = 0;
        if (CM_Get_Parent(&parent, current, 0) != CR_SUCCESS) return std::nullopt;
        current = parent;
        ULONG length = 0;
        if (CM_Get_Device_ID_Size(&length, current, 0) != CR_SUCCESS) return std::nullopt;
        std::wstring id(length + 1, L'\0');
        if (CM_Get_Device_IDW(current, id.data(), length + 1, 0) != CR_SUCCESS) return std::nullopt;
        id.resize(length);
        if (id.size() < 8 || _wcsnicmp(id.c_str(), L"USB\\VID_", 8) != 0) continue;
        const auto vendorId = TaggedHex(id, L"VID_");
        const auto productId = TaggedHex(id, L"PID_");
        ULONG size = 0;
        if (CM_Get_DevNode_Registry_PropertyW(current, CM_DRP_HARDWAREID, nullptr, nullptr, &size, 0) != CR_BUFFER_SMALL || size == 0) return std::nullopt;
        std::vector<wchar_t> hardwareIds(size / sizeof(wchar_t) + 2, L'\0');
        if (CM_Get_DevNode_Registry_PropertyW(current, CM_DRP_HARDWAREID, nullptr, hardwareIds.data(), &size, 0) != CR_SUCCESS) return std::nullopt;
        std::optional<std::uint16_t> bcdDevice;
        for (const wchar_t* entry = hardwareIds.data(); *entry != L'\0' && !bcdDevice; entry += std::wcslen(entry) + 1) {
            bcdDevice = TaggedHex(entry, L"REV_");
        }
        if (!vendorId || !productId || !bcdDevice) return std::nullopt;
        return UsbIdentity{*vendorId, *productId, *bcdDevice};
    }
}

std::optional<HostVolume> Describe(wchar_t letter) {
    const wchar_t root[] = {letter, L':', L'\\', L'\0'};
    const UINT type = GetDriveTypeW(root);
    if (type != DRIVE_REMOVABLE && type != DRIVE_FIXED) return std::nullopt;
    wchar_t fileSystemName[MAX_PATH + 1]{};
    DWORD fileSystemFlags = 0;
    if (!GetVolumeInformationW(root, nullptr, 0, nullptr, nullptr, &fileSystemFlags, fileSystemName, MAX_PATH + 1)) return std::nullopt;
    std::optional<FileSystem> fileSystem;
    if (_wcsicmp(fileSystemName, L"FAT") == 0 || _wcsicmp(fileSystemName, L"FAT32") == 0) fileSystem = FileSystem::Fat;
    if (_wcsicmp(fileSystemName, L"exFAT") == 0) fileSystem = FileSystem::ExFat;
    if (!fileSystem) return std::nullopt;
    const wchar_t devicePath[] = {L'\\', L'\\', L'.', L'\\', letter, L':', L'\0'};
    const Handle volume{CreateFileW(devicePath, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr)};
    if (volume.value == INVALID_HANDLE_VALUE) return std::nullopt;
    STORAGE_PROPERTY_QUERY query{};
    query.PropertyId = StorageDeviceProperty;
    query.QueryType = PropertyStandardQuery;
    std::vector<BYTE> descriptor(1024);
    DWORD bytes = 0;
    for (int attempt = 0; attempt < 2; ++attempt) {
        if (!DeviceIoControl(volume.value, IOCTL_STORAGE_QUERY_PROPERTY, &query, sizeof(query), descriptor.data(),
                             static_cast<DWORD>(descriptor.size()), &bytes, nullptr)) {
            return std::nullopt;
        }
        if (bytes < sizeof(STORAGE_DESCRIPTOR_HEADER)) return std::nullopt;
        const auto size = reinterpret_cast<const STORAGE_DESCRIPTOR_HEADER*>(descriptor.data())->Size;
        if (size <= descriptor.size()) break;
        descriptor.resize(size);
    }
    if (bytes < sizeof(STORAGE_DEVICE_DESCRIPTOR)) return std::nullopt;
    STORAGE_DEVICE_DESCRIPTOR header{};
    std::memcpy(&header, descriptor.data(), sizeof(header));
    if (header.BusType != BusTypeUsb) return std::nullopt;
    const auto number = DeviceNumber(volume.value);
    if (!number) return std::nullopt;
    const auto instance = DiskInstance(number->DeviceNumber);
    if (!instance) return std::nullopt;
    const auto identity = Identity(*instance);
    if (!identity) return std::nullopt;
    ULARGE_INTEGER total{};
    if (!GetDiskFreeSpaceExW(root, nullptr, &total, nullptr)) return std::nullopt;
    wchar_t volumeName[MAX_PATH]{};
    if (!GetVolumeNameForVolumeMountPointW(root, volumeName, MAX_PATH)) return std::nullopt;
    HostVolume result;
    for (const wchar_t* character = volumeName; *character != L'\0'; ++character) result.key += static_cast<char>(*character & 0x7f);
    result.root = std::filesystem::path(root);
    result.volumeSize = total.QuadPart;
    result.vendorId = identity->vendorId;
    result.productId = identity->productId;
    result.bcdDevice = identity->bcdDevice;
    result.vendor = DescriptorString(descriptor, bytes, header.VendorIdOffset);
    result.product = DescriptorString(descriptor, bytes, header.ProductIdOffset);
    result.serialNumber = DescriptorString(descriptor, bytes, header.SerialNumberOffset);
    result.readOnly = (fileSystemFlags & FILE_READ_ONLY_VOLUME) != 0;
    result.fileSystem = *fileSystem;
    return result;
}

}

std::vector<HostVolume> ScanWindowsVolumes() {
    const ErrorMode errorMode;
    std::vector<HostVolume> volumes;
    const DWORD drives = GetLogicalDrives();
    for (int letter = 0; letter < 26; ++letter) {
        if ((drives & (DWORD{1} << letter)) == 0) continue;
        if (auto volume = Describe(static_cast<wchar_t>(L'A' + letter))) volumes.push_back(std::move(*volume));
    }
    return volumes;
}

}
