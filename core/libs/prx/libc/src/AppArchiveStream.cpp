#include "prx/libc/include/AppArchive.hpp"
#ifdef _WIN32
#include "prx/libc/include/General.hpp"
#endif

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <stdexcept>
#include <system_error>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <fcntl.h>
#include <io.h>
#include <process.h>
#include <stdlib.h>
#else
#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace {

constexpr std::size_t kChunkSize = 64 * 1024;

bool ParseMode(const char* mode, bool& exclusive, bool& readOnly) {
    exclusive = false;
    readOnly = false;
    if (mode == nullptr || (*mode != 'r' && *mode != 'w' && *mode != 'a')) {
        errno = EINVAL;
        return false;
    }
    const char base = *mode++;
    bool seenB = false;
    bool seenPlus = false;
    bool seenX = false;
    bool seenE = false;
    for (; *mode != '\0'; ++mode) {
        switch (*mode) {
        case 'b': if (seenB) { errno = EINVAL; return false; } seenB = true; break;
        case '+': if (seenPlus) { errno = EINVAL; return false; } seenPlus = true; break;
        case 'x': if (seenX) { errno = EINVAL; return false; } seenX = true; break;
        case 'e': if (seenE) { errno = EINVAL; return false; } seenE = true; break;
        default: errno = EINVAL; return false;
        }
    }
    if (seenX && base == 'r' && !seenPlus) {
        errno = EINVAL;
        return false;
    }
    exclusive = seenX;
    readOnly = base == 'r' && !seenPlus && !seenE && !seenX;
    return true;
}

#ifndef _WIN32

void RemovePath(const char* path) {
    if (path != nullptr && *path != '\0') unlink(path);
}

bool WriteAll(int descriptor, const unsigned char* data, std::size_t size) {
    while (size != 0) {
        const auto written = write(descriptor, data, size);
        if (written < 0) {
            if (errno == EINTR) continue;
            return false;
        }
        if (written == 0) {
            errno = EIO;
            return false;
        }
        data += written;
        size -= static_cast<std::size_t>(written);
    }
    return true;
}

std::FILE* Materialize(const AppArchiveNode& node) {
    std::error_code error;
    auto path = std::filesystem::temp_directory_path(error) / "anyps5-app0-XXXXXX";
    if (error) {
        errno = error.category() == std::system_category() ? error.value() : EIO;
        return nullptr;
    }
    auto name = path.string();
    std::vector<char> writableName(name.begin(), name.end());
    writableName.push_back('\0');
    const int writer = mkstemp(writableName.data());
    if (writer < 0) return nullptr;
    const char* const createdPath = writableName.data();
    int reader = -1;
    bool removed = false;
    auto cleanup = [&] {
        if (reader >= 0) close(reader);
        close(writer);
        if (!removed) RemovePath(createdPath);
    };

    std::array<unsigned char, kChunkSize> buffer{};
    std::uint64_t offset = 0;
    try {
        while (offset < node.Size) {
            const auto amount = std::min<std::uint64_t>(buffer.size(), node.Size - offset);
            const auto read = AppArchiveRead_nid_no_patch(node.Handle, offset, buffer.data(), amount);
            if (read != amount) throw std::runtime_error("app0.zar is damaged: a compressed block of handle " + std::to_string(node.Handle) + " cannot be read");
            if (!WriteAll(writer, buffer.data(), static_cast<std::size_t>(amount))) {
                const int savedErrno = errno;
                cleanup();
                errno = savedErrno;
                return nullptr;
            }
            offset += amount;
        }
    } catch (...) {
        cleanup();
        throw;
    }

    reader = open(createdPath, O_RDONLY
#ifdef O_CLOEXEC
        | O_CLOEXEC
#endif
    );
    if (reader < 0) {
        const int savedErrno = errno;
        cleanup();
        errno = savedErrno;
        return nullptr;
    }
    struct stat writerStatus{};
    struct stat readerStatus{};
    if (fstat(writer, &writerStatus) != 0 || fstat(reader, &readerStatus) != 0) {
        const int savedErrno = errno;
        cleanup();
        errno = savedErrno;
        return nullptr;
    }
    if (writerStatus.st_dev != readerStatus.st_dev || writerStatus.st_ino != readerStatus.st_ino) {
        cleanup();
        errno = EIO;
        return nullptr;
    }
    if (unlink(createdPath) != 0) {
        const int savedErrno = errno;
        cleanup();
        errno = savedErrno;
        return nullptr;
    }
    removed = true;
    close(writer);
    const int descriptor = reader;
    reader = -1;
    std::FILE* stream = fdopen(descriptor, "rb");
    if (stream == nullptr) {
        const int savedErrno = errno;
        close(descriptor);
        errno = savedErrno;
        return nullptr;
    }
    try {
        AppArchiveAttachMaterialized_nid_no_patch(fileno(stream), node);
    } catch (...) {
        fclose(stream);
        throw;
    }
    return stream;
}

#else

void SetWindowsErrno(DWORD error) {
    errno = WindowsOpenError_nid_no_patch(error);
}

bool SameFile(HANDLE left, HANDLE right, DWORD& error, bool& mismatch) {
    mismatch = false;
    BY_HANDLE_FILE_INFORMATION first{};
    BY_HANDLE_FILE_INFORMATION second{};
    if (!GetFileInformationByHandle(left, &first) || !GetFileInformationByHandle(right, &second)) {
        error = GetLastError();
        return false;
    }
    if (first.dwVolumeSerialNumber != second.dwVolumeSerialNumber || first.nFileIndexHigh != second.nFileIndexHigh || first.nFileIndexLow != second.nFileIndexLow) {
        mismatch = true;
        return false;
    }
    return true;
}

std::wstring TempDirectory() {
    const DWORD required = GetTempPathW(0, nullptr);
    if (required == 0) {
        SetWindowsErrno(GetLastError());
        return {};
    }
    std::wstring path(required, L'\0');
    const DWORD written = GetTempPathW(required, path.data());
    if (written == 0 || written >= required) {
        SetWindowsErrno(GetLastError());
        return {};
    }
    path.resize(written);
    return path;
}

std::FILE* Materialize(const AppArchiveNode& node) {
    static volatile long serial = 0;
    auto directory = TempDirectory();
    if (directory.empty()) return nullptr;
    HANDLE writer = INVALID_HANDLE_VALUE;
    std::wstring path;
    for (int attempt = 0; attempt < 128; ++attempt) {
        const auto id = InterlockedIncrement(&serial);
        path = directory + L"AnyPS5-app0-" + std::to_wstring(_getpid()) + L"-" + std::to_wstring(id) + L".tmp";
        writer = CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, CREATE_NEW, FILE_ATTRIBUTE_TEMPORARY, nullptr);
        if (writer != INVALID_HANDLE_VALUE) break;
        const DWORD error = GetLastError();
        if (error != ERROR_FILE_EXISTS && error != ERROR_ALREADY_EXISTS) {
            SetWindowsErrno(error);
            return nullptr;
        }
    }
    if (writer == INVALID_HANDLE_VALUE) {
        errno = EEXIST;
        return nullptr;
    }
    HANDLE reader = INVALID_HANDLE_VALUE;
    bool transferred = false;
    auto cleanup = [&] {
        if (reader != INVALID_HANDLE_VALUE) CloseHandle(reader);
        if (writer != INVALID_HANDLE_VALUE) CloseHandle(writer);
        if (!transferred) DeleteFileW(path.c_str());
    };

    std::array<unsigned char, kChunkSize> buffer{};
    std::uint64_t offset = 0;
    try {
        while (offset < node.Size) {
            const auto amount = std::min<std::uint64_t>(buffer.size(), node.Size - offset);
            const auto read = AppArchiveRead_nid_no_patch(node.Handle, offset, buffer.data(), amount);
            if (read != amount) throw std::runtime_error("app0.zar is damaged: a compressed block of handle " + std::to_string(node.Handle) + " cannot be read");
            DWORD written = 0;
            const BOOL writeSucceeded = WriteFile(writer, buffer.data(), static_cast<DWORD>(amount), &written, nullptr);
            if (!writeSucceeded || written != amount) {
                const DWORD error = writeSucceeded ? ERROR_WRITE_FAULT : GetLastError();
                cleanup();
                SetWindowsErrno(error);
                return nullptr;
            }
            offset += amount;
        }
    } catch (...) {
        cleanup();
        throw;
    }
    if (!FlushFileBuffers(writer)) {
        const DWORD error = GetLastError();
        cleanup();
        SetWindowsErrno(error);
        return nullptr;
    }
    reader = ReOpenFile(writer, GENERIC_READ | DELETE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, FILE_FLAG_DELETE_ON_CLOSE);
    if (reader == INVALID_HANDLE_VALUE) {
        const DWORD error = GetLastError();
        cleanup();
        SetWindowsErrno(error);
        return nullptr;
    }
    DWORD identityError = ERROR_SUCCESS;
    bool identityMismatch = false;
    if (!SameFile(writer, reader, identityError, identityMismatch)) {
        cleanup();
        if (identityMismatch) errno = EIO;
        else SetWindowsErrno(identityError);
        return nullptr;
    }
    CloseHandle(writer);
    writer = INVALID_HANDLE_VALUE;
    const int descriptor = _open_osfhandle(reinterpret_cast<intptr_t>(reader), _O_RDONLY | _O_BINARY);
    if (descriptor < 0) {
        const int savedErrno = errno;
        CloseHandle(reader);
        reader = INVALID_HANDLE_VALUE;
        DeleteFileW(path.c_str());
        errno = savedErrno;
        return nullptr;
    }
    reader = INVALID_HANDLE_VALUE;
    std::FILE* stream = _fdopen(descriptor, "rb");
    if (stream == nullptr) {
        const int savedErrno = errno;
        _close(descriptor);
        errno = savedErrno;
        return nullptr;
    }
    transferred = true;
    try {
        AppArchiveAttachMaterialized_nid_no_patch(_fileno(stream), node);
    } catch (...) {
        fclose(stream);
        throw;
    }
    return stream;
}

#endif

}

extern "C" std::FILE* AppArchiveOpenStream_nid_no_patch(const std::filesystem::path& hostPath, const char* nativeMode) {
    AppArchiveNode node{};
    if (!AppArchiveLookup_nid_no_patch(hostPath, &node)) {
        errno = ENOENT;
        return nullptr;
    }
    bool exclusive = false;
    bool readOnly = false;
    if (!ParseMode(nativeMode, exclusive, readOnly)) return nullptr;
    if (exclusive) {
        errno = EEXIST;
        return nullptr;
    }
    if (!readOnly) {
        errno = EROFS;
        return nullptr;
    }
    if (node.Directory) {
        errno = EISDIR;
        return nullptr;
    }
    return Materialize(node);
}
