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

public:
    static bool ReadRelative(const std::filesystem::path& native, std::string& target, const char* operation) {
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
        if (header.tag != IO_REPARSE_TAG_SYMLINK || (header.flags & relativeFlag) == 0) return false;
        const std::size_t begin = sizeof(header) + header.substituteNameOffset;
        if (begin + header.substituteNameLength > size)
            throw std::runtime_error(std::string(operation) + ": malformed symbolic link reparse data");
        std::wstring name(header.substituteNameLength / sizeof(wchar_t), L'\0');
        std::memcpy(name.data(), data.data() + begin, name.size() * sizeof(wchar_t));
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
        const auto textAt = [&](USHORT offset, USHORT length) {
            const std::size_t begin = sizeof(header) + offset;
            if (length % sizeof(wchar_t) != 0 || begin + length > size)
                throw std::runtime_error(std::string(operation) + ": malformed junction reparse data");
            std::wstring name(length / sizeof(wchar_t), L'\0');
            if (!name.empty()) std::memcpy(name.data(), data.data() + begin, name.size() * sizeof(wchar_t));
            return name;
        };
        std::wstring name = textAt(header.printNameOffset, header.printNameLength);
        if (name.empty()) name = textAt(header.substituteNameOffset, header.substituteNameLength);
        if (name.rfind(L"\\??\\", 0) == 0 || name.rfind(L"\\\\?\\", 0) == 0) name.erase(0, 4);
        target = std::filesystem::path(name).generic_string();
        return true;
    }
};

}

#endif
