#ifndef CORE_LIBS_PRX_LIBKERNEL_FILE_WINDOWSPERMISSIONS_HPP
#define CORE_LIBS_PRX_LIBKERNEL_FILE_WINDOWSPERMISSIONS_HPP

#include <windows.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <optional>

namespace File {

class WindowsPermissions {
    using QueryInformationFunction = LONG(NTAPI*)(HANDLE, void*, void*, ULONG, ULONG);
    using QueryEaFunction = LONG(NTAPI*)(HANDLE, void*, void*, ULONG, BOOLEAN, void*, ULONG, ULONG*, BOOLEAN);
    using SetEaFunction = LONG(NTAPI*)(HANDLE, void*, void*, ULONG);
    using StatusToErrorFunction = ULONG(NTAPI*)(LONG);

    struct IoStatus {
        void* status;
        ULONG_PTR information;
    };

    struct UnicodeName {
        USHORT length;
        USHORT maximumLength;
        wchar_t* buffer;
    };

    struct ObjectAttributes {
        ULONG length;
        HANDLE rootDirectory;
        UnicodeName* objectName;
        ULONG attributes;
        void* securityDescriptor;
        void* securityQualityOfService;
    };

    using CreateFileFunction = LONG(NTAPI*)(HANDLE*, ACCESS_MASK, ObjectAttributes*, IoStatus*, LARGE_INTEGER*, ULONG, ULONG, ULONG, ULONG, void*, ULONG);
    static constexpr ULONG openExisting = 1;
    static constexpr ULONG synchronousOptions = 0x00000020;
    static constexpr ULONG backupIntentOption = 0x00004000;

    static constexpr char name[] = "$LXMOD";
    static constexpr std::size_t nameLength = sizeof(name) - 1;
    static constexpr std::size_t valueOffset = 8 + sizeof(name);
    static constexpr std::size_t entrySize = valueOffset + sizeof(std::uint32_t);
    static constexpr std::size_t listSize = 5 + sizeof(name);
    static constexpr ULONG eaInformationClass = 7;
    static constexpr LONG accessDenied = static_cast<LONG>(0xC0000022u);
    static constexpr LONG easNotSupported = static_cast<LONG>(0xC000004Fu);
    static constexpr DWORD shareAll = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;
    static constexpr std::uint32_t directoryType = 0040000;
    static constexpr std::uint32_t regularType = 0100000;

    template <typename TFunction>
    static TFunction Ntdll(const char* function) {
        return reinterpret_cast<TFunction>(reinterpret_cast<void*>(GetProcAddress(GetModuleHandleW(L"ntdll.dll"), function)));
    }

    static DWORD Error(LONG status) {
        static const auto convert = Ntdll<StatusToErrorFunction>("RtlNtStatusToDosError");
        return convert == nullptr ? ERROR_GEN_FAILURE : convert(status);
    }

    static LONG Query(HANDLE handle, std::uint32_t& mode, bool& found) {
        static const auto query = Ntdll<QueryEaFunction>("NtQueryEaFile");
        if (query == nullptr) return easNotSupported;
        alignas(8) std::array<unsigned char, 16> list{};
        list[4] = static_cast<unsigned char>(nameLength);
        std::memcpy(list.data() + 5, name, sizeof(name));
        alignas(8) std::array<unsigned char, 64> entry{};
        IoStatus io{};
        const LONG status = query(handle, &io, entry.data(), static_cast<ULONG>(entry.size()), TRUE, list.data(), static_cast<ULONG>(listSize), nullptr, TRUE);
        if (status < 0) return status;
        std::uint16_t valueLength = 0;
        std::memcpy(&valueLength, entry.data() + 6, sizeof(valueLength));
        found = entry[5] == nameLength && valueLength == sizeof(mode);
        if (found) std::memcpy(&mode, entry.data() + valueOffset, sizeof(mode));
        return status;
    }

    static LONG Write(HANDLE handle, std::uint32_t mode) {
        static const auto set = Ntdll<SetEaFunction>("NtSetEaFile");
        if (set == nullptr) return easNotSupported;
        alignas(8) std::array<unsigned char, entrySize> entry{};
        entry[5] = static_cast<unsigned char>(nameLength);
        const auto valueLength = static_cast<std::uint16_t>(sizeof(mode));
        std::memcpy(entry.data() + 6, &valueLength, sizeof(valueLength));
        std::memcpy(entry.data() + 8, name, sizeof(name));
        std::memcpy(entry.data() + valueOffset, &mode, sizeof(mode));
        IoStatus io{};
        return set(handle, &io, entry.data(), static_cast<ULONG>(entry.size()));
    }

public:
    static HANDLE Reopen(HANDLE handle, DWORD access) {
        const HANDLE reopened = ReOpenFile(handle, access, shareAll, FILE_FLAG_BACKUP_SEMANTICS);
        if (reopened != INVALID_HANDLE_VALUE) return reopened;
        const DWORD reopenError = GetLastError();
        static const auto create = Ntdll<CreateFileFunction>("NtCreateFile");
        if (create == nullptr) {
            SetLastError(reopenError);
            return INVALID_HANDLE_VALUE;
        }
        UnicodeName emptyName{};
        ObjectAttributes object{sizeof(ObjectAttributes), handle, &emptyName, 0, nullptr, nullptr};
        IoStatus io{};
        HANDLE relative = nullptr;
        const LONG status = create(&relative, access | SYNCHRONIZE, &object, &io, nullptr, 0, shareAll, openExisting, synchronousOptions | backupIntentOption, nullptr, 0);
        if (status >= 0) return relative;
        SetLastError(reopenError);
        return INVALID_HANDLE_VALUE;
    }

    static std::optional<std::uint16_t> Read(HANDLE handle) {
        static const auto information = Ntdll<QueryInformationFunction>("NtQueryInformationFile");
        if (information == nullptr) return std::nullopt;
        ULONG size = 0;
        IoStatus io{};
        if (information(handle, &io, &size, sizeof(size), eaInformationClass) < 0 || size == 0) return std::nullopt;
        std::uint32_t mode = 0;
        bool found = false;
        LONG status = Query(handle, mode, found);
        if (status == accessDenied) {
            const HANDLE reader = Reopen(handle, FILE_READ_EA);
            if (reader == INVALID_HANDLE_VALUE) return std::nullopt;
            status = Query(reader, mode, found);
            CloseHandle(reader);
        }
        if (status < 0 || !found) return std::nullopt;
        return static_cast<std::uint16_t>(mode & 07777);
    }

    static DWORD Store(HANDLE handle, std::uint16_t permissions) {
        FILE_BASIC_INFO basic{};
        if (!GetFileInformationByHandleEx(handle, FileBasicInfo, &basic, sizeof(basic))) return GetLastError();
        const std::uint32_t type = (basic.FileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0 ? directoryType : regularType;
        const std::uint32_t mode = type | (permissions & 07777u);
        LONG status = Write(handle, mode);
        if (status == accessDenied) {
            const HANDLE writer = Reopen(handle, FILE_WRITE_EA);
            if (writer == INVALID_HANDLE_VALUE) return GetLastError();
            status = Write(writer, mode);
            CloseHandle(writer);
        }
        if (status >= 0 || status == easNotSupported) return ERROR_SUCCESS;
        return Error(status);
    }

    static DWORD Store(const std::filesystem::path& path, std::uint16_t permissions) {
        const HANDLE handle = CreateFileW(path.c_str(), FILE_READ_ATTRIBUTES | FILE_WRITE_EA, shareAll, nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
        if (handle == INVALID_HANDLE_VALUE) return GetLastError();
        const DWORD error = Store(handle, permissions);
        CloseHandle(handle);
        return error;
    }
};

}

#endif
