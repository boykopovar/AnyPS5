#ifndef CORE_LIBS_PRX_LIBKERNEL_FILE_WINDOWSSYMBOLICLINK_HPP
#define CORE_LIBS_PRX_LIBKERNEL_FILE_WINDOWSSYMBOLICLINK_HPP

#include <windows.h>
#include <winioctl.h>
#include <array>
#include <cstddef>
#include <cstring>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <system_error>

namespace File {

class WindowsSymbolicLink {
    struct ReparseData {
        ULONG tag;
        USHORT dataLength;
        USHORT reserved;
        USHORT substituteNameOffset;
        USHORT substituteNameLength;
        USHORT printNameOffset;
        USHORT printNameLength;
        ULONG flags;
    };
    static constexpr ULONG relativeFlag = 1;

    static std::wstring TextAt(const std::byte* data, DWORD size, std::size_t origin, USHORT offset, USHORT length, const char* operation, const char* malformed) {
        const std::size_t begin = origin + offset;
        if (length % sizeof(wchar_t) != 0 || begin + length > size)
            throw std::runtime_error(std::string(operation) + ": " + malformed);
        std::wstring name(length / sizeof(wchar_t), L'\0');
        if (!name.empty()) std::memcpy(name.data(), data + begin, name.size() * sizeof(wchar_t));
        return name;
    }

    static std::wstring WithoutNtPrefix(std::wstring name) {
        if (name.rfind(L"\\??\\", 0) == 0 || name.rfind(L"\\\\?\\", 0) == 0) name.erase(0, 4);
        return name;
    }

    static bool IsDriveAbsolute(const std::wstring& name) {
        if (name.size() < 3 || name[1] != L':' || (name[2] != L'\\' && name[2] != L'/')) return false;
        const wchar_t letter = name[0];
        return (letter >= L'A' && letter <= L'Z') || (letter >= L'a' && letter <= L'z');
    }

public:
    static bool IsNameSurrogate(const std::filesystem::path& native, const char* operation) {
        const HANDLE handle = CreateFileW(native.c_str(), FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
            OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS, nullptr);
        if (handle == INVALID_HANDLE_VALUE)
            throw std::system_error(static_cast<int>(GetLastError()), std::system_category(), std::string(operation) + ": CreateFileW");
        FILE_ATTRIBUTE_TAG_INFO information{};
        const BOOL read = GetFileInformationByHandleEx(handle, FileAttributeTagInfo, &information, sizeof(information));
        const DWORD code = GetLastError();
        CloseHandle(handle);
        if (!read)
            throw std::system_error(static_cast<int>(code), std::system_category(), std::string(operation) + ": GetFileInformationByHandleEx");
        return (information.FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0 && IsReparseTagNameSurrogate(information.ReparseTag);
    }

    static bool Read(const std::filesystem::path& native, std::string& target, const char* operation) {
        const HANDLE handle = CreateFileW(native.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
            OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS, nullptr);
        if (handle == INVALID_HANDLE_VALUE)
            throw std::system_error(static_cast<int>(GetLastError()), std::system_category(), std::string(operation) + ": CreateFileW");
        std::array<std::byte, MAXIMUM_REPARSE_DATA_BUFFER_SIZE> data{};
        DWORD size = 0;
        const BOOL read = DeviceIoControl(handle, FSCTL_GET_REPARSE_POINT, nullptr, 0, data.data(), static_cast<DWORD>(data.size()), &size, nullptr);
        const DWORD code = GetLastError();
        CloseHandle(handle);
        if (!read)
            throw std::system_error(static_cast<int>(code), std::system_category(), std::string(operation) + ": FSCTL_GET_REPARSE_POINT");
        ReparseData header{};
        if (size < sizeof(header)) return false;
        std::memcpy(&header, data.data(), sizeof(header));
        if (header.tag != IO_REPARSE_TAG_SYMLINK) return false;
        const bool relative = (header.flags & relativeFlag) != 0;
        std::wstring name = relative
            ? TextAt(data.data(), size, sizeof(ReparseData), header.substituteNameOffset, header.substituteNameLength, operation, "malformed symbolic link reparse data")
            : TextAt(data.data(), size, sizeof(ReparseData), header.printNameOffset, header.printNameLength, operation, "malformed symbolic link reparse data");
        if (!relative && name.empty())
            name = WithoutNtPrefix(TextAt(data.data(), size, sizeof(ReparseData), header.substituteNameOffset, header.substituteNameLength, operation, "malformed symbolic link reparse data"));
        target = std::filesystem::path(name).generic_string();
        return true;
    }

    static bool ReadJunction(const std::filesystem::path& native, std::string& target, const char* operation) {
        const HANDLE handle = CreateFileW(native.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
            OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS, nullptr);
        if (handle == INVALID_HANDLE_VALUE)
            throw std::system_error(static_cast<int>(GetLastError()), std::system_category(), std::string(operation) + ": CreateFileW");
        std::array<std::byte, MAXIMUM_REPARSE_DATA_BUFFER_SIZE> data{};
        DWORD size = 0;
        const BOOL read = DeviceIoControl(handle, FSCTL_GET_REPARSE_POINT, nullptr, 0, data.data(), static_cast<DWORD>(data.size()), &size, nullptr);
        const DWORD code = GetLastError();
        CloseHandle(handle);
        if (!read)
            throw std::system_error(static_cast<int>(code), std::system_category(), std::string(operation) + ": FSCTL_GET_REPARSE_POINT");
        struct MountPoint {
            ULONG tag;
            USHORT dataLength;
            USHORT reserved;
            USHORT substituteNameOffset;
            USHORT substituteNameLength;
            USHORT printNameOffset;
            USHORT printNameLength;
        } header{};
        if (size < sizeof(header)) return false;
        std::memcpy(&header, data.data(), sizeof(header));
        if (header.tag != IO_REPARSE_TAG_MOUNT_POINT) return false;
        const auto nameAt = [&](USHORT offset, USHORT length) {
            return WithoutNtPrefix(TextAt(data.data(), size, sizeof(header), offset, length, operation, "malformed junction reparse data"));
        };
        std::wstring name = nameAt(header.printNameOffset, header.printNameLength);
        if (!IsDriveAbsolute(name)) name = nameAt(header.substituteNameOffset, header.substituteNameLength);
        if (!IsDriveAbsolute(name)) return false;
        target = std::filesystem::path(name).generic_string();
        return true;
    }
};

inline bool WindowsSymbolicLinkLoop(const std::filesystem::path& path) {
    const auto handle = CreateFileW(path.c_str(), FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    if (handle == INVALID_HANDLE_VALUE) return GetLastError() == ERROR_CANT_RESOLVE_FILENAME;
    CloseHandle(handle);
    return false;
}

}

#endif
