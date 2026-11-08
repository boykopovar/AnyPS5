#pragma once

#include <domain/Types.hpp>
#include <io/NativePath.hpp>
#include <windows.h>
#include <sddl.h>
#include <algorithm>
#include <array>
#include <filesystem>
#include <ios>
#include <memory>
#include <random>
#include <string_view>
#include <system_error>
#include <vector>

namespace Io::Detail {

struct WindowsFileOperations {
    decltype(&WriteFile) Write = &WriteFile;
    decltype(&FlushFileBuffers) Flush = &FlushFileBuffers;
    decltype(&CloseHandle) Close = &CloseHandle;
    decltype(&ReplaceFileW) Replace = &ReplaceFileW;
    decltype(&MoveFileExW) Move = &MoveFileExW;
};

class WindowsStage {
    std::filesystem::path directory;
    std::filesystem::path file;
    HANDLE handle = INVALID_HANDLE_VALUE;
    const WindowsFileOperations& operations;

public:
    WindowsStage(const std::filesystem::path& target, const bool copyDacl, const WindowsFileOperations& calls) : operations(calls) {
        std::vector<std::uint8_t> descriptor;
        SECURITY_ATTRIBUTES attributes{sizeof(SECURITY_ATTRIBUTES), nullptr, FALSE};
        SECURITY_ATTRIBUTES* security = nullptr;
        if (copyDacl) {
            DWORD length = 0;
            GetFileSecurityW(target.c_str(), DACL_SECURITY_INFORMATION, nullptr, 0, &length);
            if (length == 0) return;
            descriptor.resize(length);
            if (!GetFileSecurityW(target.c_str(), DACL_SECURITY_INFORMATION, descriptor.data(), length, &length)) return;
            PACL dacl = nullptr;
            BOOL present = FALSE;
            BOOL defaulted = FALSE;
            if (!GetSecurityDescriptorDacl(descriptor.data(), &present, &dacl, &defaulted)) return;
            if (present && dacl) {
                for (DWORD index = 0; index < dacl->AceCount; ++index) {
                    void* ace = nullptr;
                    if (!GetAce(dacl, index, &ace)) return;
                    static_cast<ACE_HEADER*>(ace)->AceFlags &= ~INHERITED_ACE;
                }
            }
            if (!SetSecurityDescriptorControl(descriptor.data(), SE_DACL_PROTECTED | SE_DACL_AUTO_INHERITED | SE_DACL_AUTO_INHERIT_REQ, SE_DACL_PROTECTED)) return;
            attributes.lpSecurityDescriptor = descriptor.data();
            security = &attributes;
        }
        PSECURITY_DESCRIPTOR backupDescriptor = nullptr;
        if (copyDacl && !ConvertStringSecurityDescriptorToSecurityDescriptorW(L"D:P(A;;FA;;;OW)", SDDL_REVISION_1, &backupDescriptor, nullptr)) return;
        const std::unique_ptr<void, decltype(&LocalFree)> backupSecurity(backupDescriptor, LocalFree);
        SECURITY_ATTRIBUTES backupAttributes{sizeof(SECURITY_ATTRIBUTES), backupDescriptor, FALSE};
        constexpr std::wstring_view characters = L"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
        std::random_device device;
        for (int attempt = 0; attempt < 100; ++attempt) {
            std::wstring name = L".anyps5-";
            for (int index = 0; index < 12; ++index) name += characters[device() % characters.size()];
            const auto candidate = target.parent_path() / name;
            if (!CreateDirectoryW(candidate.c_str(), backupDescriptor ? &backupAttributes : nullptr)) {
                if (GetLastError() == ERROR_ALREADY_EXISTS) continue;
                return;
            }
            directory = candidate;
            file = target.parent_path() / (name + L".output");
            handle = CreateFileW(file.c_str(), GENERIC_WRITE, 0, security, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (handle == INVALID_HANDLE_VALUE) file.clear();
            return;
        }
    }

    ~WindowsStage() {
        if (handle != INVALID_HANDLE_VALUE) CloseHandle(handle);
        if (!file.empty()) DeleteFileW(file.c_str());
        if (!directory.empty()) RemoveDirectoryW(directory.c_str());
    }

    WindowsStage(const WindowsStage&) = delete;
    WindowsStage& operator=(const WindowsStage&) = delete;

    HANDLE Handle() const { return handle; }
    const std::filesystem::path& Path() const { return file; }
    void ReleasePath() { file.clear(); }
    std::filesystem::path Backup() const { return directory / L"previous"; }

    bool Finish() {
        const bool flushed = operations.Flush(handle) != 0;
        const bool closed = operations.Close(handle) != 0;
        if (closed) handle = INVALID_HANDLE_VALUE;
        return flushed && closed;
    }
};

inline bool writeAll(const HANDLE handle, const char* data, std::size_t size, const WindowsFileOperations& operations) {
    while (size != 0) {
        const auto chunk = static_cast<DWORD>(std::min<std::size_t>(size, 0xffffffffu));
        DWORD written = 0;
        if (!operations.Write(handle, data, chunk, &written, nullptr) || written == 0) return false;
        data += written;
        size -= written;
    }
    return true;
}

inline bool writeText(const HANDLE handle, const char* data, const std::size_t size, const WindowsFileOperations& operations) {
    std::array<char, 16384> buffer;
    std::size_t used = 0;
    for (std::size_t index = 0; index < size; ++index) {
        if (data[index] == '\n') buffer[used++] = '\r';
        buffer[used++] = data[index];
        if (used >= buffer.size() - 1) {
            if (!writeAll(handle, buffer.data(), used, operations)) return false;
            used = 0;
        }
    }
    return writeAll(handle, buffer.data(), used, operations);
}

inline bool ReplaceWindowsFile(std::filesystem::path target, const std::string& path, const std::ios::openmode mode, const char* data, const std::size_t size, const WindowsFileOperations& operations = {}) {
    if (target.filename().native().find(L':') != std::wstring::npos) return false;
    const auto fail = [&] { throw Domain::RelinkerException("Cannot open output file: " + path); };
    std::error_code error;
    const auto attributes = GetFileAttributesW(target.c_str());
    if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
        WIN32_FIND_DATAW entry{};
        const auto find = FindFirstFileW(target.c_str(), &entry);
        if (find == INVALID_HANDLE_VALUE) fail();
        FindClose(find);
        if (entry.dwReserved0 != IO_REPARSE_TAG_SYMLINK) return false;
    }
    auto previous = std::filesystem::symlink_status(target, error);
    if (std::filesystem::is_symlink(previous)) {
        target = std::filesystem::canonical(target, error);
        if (error) return false;
        previous = std::filesystem::symlink_status(target, error);
    }
    const bool regular = std::filesystem::is_regular_file(previous);
    if (!regular && previous.type() != std::filesystem::file_type::not_found) return false;
    const auto probe = CreateFileW(target.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (probe != INVALID_HANDLE_VALUE) {
        const std::unique_ptr<void, decltype(&CloseHandle)> held(probe, CloseHandle);
        const auto type = GetFileType(probe);
        if (type != FILE_TYPE_DISK) return false;
        if ((GetFileAttributesW(target.c_str()) & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
            const auto length = GetFinalPathNameByHandleW(probe, nullptr, 0, FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
            if (length == 0) fail();
            std::wstring resolved(length, L'\0');
            const auto written = GetFinalPathNameByHandleW(probe, resolved.data(), length, FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
            if (written == 0 || written >= length) fail();
            resolved.resize(written);
            target = resolved;
        }
    } else if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
        return false;
    }
    bool copyDacl = false;
    if (regular) {
        const auto targetAttributes = GetFileAttributesW(target.c_str());
        if (targetAttributes == INVALID_FILE_ATTRIBUTES || (targetAttributes & FILE_ATTRIBUTE_READONLY) != 0) fail();
        if ((targetAttributes & (FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_ENCRYPTED)) != 0) return false;
        const auto writable = CreateFileW(target.c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                          nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (writable == INVALID_HANDLE_VALUE) fail();
        const std::unique_ptr<void, decltype(&CloseHandle)> held(writable, CloseHandle);
        DWORD fileSystemFlags = 0;
        copyDacl = !GetVolumeInformationByHandleW(writable, nullptr, 0, nullptr, nullptr, &fileSystemFlags, nullptr, 0) ||
                   (fileSystemFlags & FILE_PERSISTENT_ACLS) != 0;
    }
    WindowsStage staged(target, copyDacl, operations);
    if (staged.Handle() == INVALID_HANDLE_VALUE) fail();
    const bool written = (mode & std::ios::binary) != 0 ? writeAll(staged.Handle(), data, size, operations) : writeText(staged.Handle(), data, size, operations);
    if (!staged.Finish() || !written) throw Domain::RelinkerException("Failed to write file: " + path);
    if (!regular) {
        if (!operations.Move(staged.Path().c_str(), target.c_str(), MOVEFILE_WRITE_THROUGH)) fail();
        staged.ReleasePath();
        return true;
    }
    const auto backup = staged.Backup();
    if (operations.Replace(target.c_str(), staged.Path().c_str(), backup.c_str(), 0, nullptr, nullptr)) {
        staged.ReleasePath();
        if (!DeleteFileW(backup.c_str()) && GetLastError() != ERROR_FILE_NOT_FOUND)
            throw Domain::RelinkerException("Replaced output file: " + path + "; could not remove previous output at: " + Utf8Path(backup));
        return true;
    }
    if (GetLastError() == ERROR_UNABLE_TO_MOVE_REPLACEMENT_2) {
        if (!operations.Move(backup.c_str(), target.c_str(), MOVEFILE_WRITE_THROUGH))
            throw Domain::RelinkerException("Cannot replace output file: " + path + "; previous output retained at: " + Utf8Path(backup));
    }
    fail();
    return false;
}

}
