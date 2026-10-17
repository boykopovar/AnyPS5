#ifndef CORE_LIBS_PRX_LIBC_INCLUDE_FILESTREAM_HPP
#define CORE_LIBS_PRX_LIBC_INCLUDE_FILESTREAM_HPP

#include <cstdio>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <cstdint>
#include <cstddef>
#include <type_traits>
#ifdef _WIN32
#include <io.h>
#include "prx/libc/include/FileLock.hpp"
extern "C" void* HostStandardHandle_nid_no_patch(int descriptor);
extern "C" void KeepStandardHandle_nid_no_patch(int descriptor, void* handle);
#endif

struct GuestFilePrefix {
    unsigned char* position = nullptr;
    std::int32_t readRemaining = 0;
    std::int32_t writeRemaining = 0;
    std::int16_t flags = 0;
    std::int16_t descriptor = -1;
    unsigned char* buffer = nullptr;
    std::int32_t bufferSize = 0;
    std::int32_t bufferPadding = 0;
    std::int32_t lineBufferSize = 0;
};
static_assert(offsetof(GuestFilePrefix, flags) == 16);
static_assert(offsetof(GuestFilePrefix, descriptor) == 18);
static_assert(offsetof(GuestFilePrefix, buffer) == 24);
static_assert(offsetof(GuestFilePrefix, lineBufferSize) == 40);

static constexpr const char* FOPEN_EXT_VERT = ".vert";
static constexpr const char* FOPEN_MSG_NULL_ARG = "null argument";
static constexpr const char* FOPEN_MSG_NOT_FOUND = "file not found";
static constexpr const char* FOPEN_MSG_OPEN_FAILED = "open failed";

#ifndef _WIN32
extern "C" int KeepHostStandardError_nid_no_patch();
#endif

class FileStream {
    // Only the macro-accessed FreeBSD prefix is exposed. The remaining guest
    // FILE area is reserved; host state lives beyond it and is never a guest FILE*.
    GuestFilePrefix _guest{};
    std::byte _reserved[256 - sizeof(GuestFilePrefix)]{};
    std::FILE* _handle;
    bool _dynamic;
    bool guestError = false;

public:
    explicit FileStream(std::FILE* handle, bool dynamic = false) : _handle(handle), _dynamic(dynamic) {
        if (!_handle) throw std::runtime_error("FileStream: null handle");
        _guest.flags = handle == stdin ? 4 : handle == stdout || handle == stderr ? 8 : 0x10;
#ifdef _WIN32
        const int descriptor = _fileno(handle);
#else
        const int descriptor = ::fileno(handle);
#endif
        _guest.descriptor = descriptor >= 0 && descriptor <= 32767 ? static_cast<std::int16_t>(descriptor) : -1;
    }

    FileStream(const FileStream&) = delete;
    FileStream& operator=(const FileStream&) = delete;

    std::FILE* GetHandle() const {
        if (!_handle) throw std::runtime_error("FileStream: closed stream");
        return _handle;
    }

    bool IsOpen() const {
        return _handle != nullptr;
    }

    bool IsDynamic() const {
        return _dynamic;
    }

    GuestFilePrefix& GuestState() { return _guest; }
    bool Reopen(const std::filesystem::path& filename, const char* mode) {
        auto* previous = GetHandle();
        _guest = {};
        guestError = false;
#ifdef _WIN32
        std::wstring wideMode;
        for (const char character : std::string_view(mode)) wideMode.push_back(static_cast<unsigned char>(character));
        const int replaced = previous ? _fileno(previous) : -1;
        auto* const standard = HostStandardHandle_nid_no_patch(replaced);
        GuestFileLockForget_nid_no_patch(replaced);
        _handle = ::_wfreopen(filename.c_str(), wideMode.c_str(), previous);
        KeepStandardHandle_nid_no_patch(replaced, standard);
#else
        if (::fileno(previous) == 2) KeepHostStandardError_nid_no_patch();
        _handle = std::freopen(filename.c_str(), mode, previous);
#endif
        if (!_handle) return false;
        _guest.flags = 0x10;
#ifdef _WIN32
        const int descriptor = _fileno(_handle);
#else
        const int descriptor = ::fileno(_handle);
#endif
        _guest.descriptor = descriptor >= 0 && descriptor <= 32767 ? static_cast<std::int16_t>(descriptor) : -1;
        return true;
    }
    void SyncStatus() {
        if (!_handle) return;
        _guest.readRemaining = 0;
        _guest.writeRemaining = 0;
        _guest.flags = static_cast<std::int16_t>((_guest.flags & ~0x60) |
            (std::feof(GetHandle()) ? 0x20 : 0) | (std::ferror(GetHandle()) || guestError ? 0x40 : 0));
    }

    void SyncUnlockedStatus() {
        if (!_handle) return;
        _guest.readRemaining = 0;
        _guest.writeRemaining = 0;
#ifdef _WIN32
        const bool endOfFile = std::feof(GetHandle()) != 0;
        const bool error = std::ferror(GetHandle()) != 0;
#else
        const bool endOfFile = feof_unlocked(GetHandle()) != 0;
        const bool error = ferror_unlocked(GetHandle()) != 0;
#endif
        _guest.flags = static_cast<std::int16_t>((_guest.flags & ~0x60) |
            (endOfFile ? 0x20 : 0) | (error || guestError ? 0x40 : 0));
    }

    void SetError() {
        guestError = true;
        SyncStatus();
    }

    void ClearError() {
        if (!_handle) return;
        std::clearerr(GetHandle());
        guestError = false;
        SyncStatus();
    }

    int Close() {
        GetHandle();
#ifdef _WIN32
        GuestFileLockForget_nid_no_patch(::_fileno(_handle));
#endif
        const int result = std::fclose(std::exchange(_handle, nullptr));
        _guest.flags = 0;
        _guest.descriptor = -1;
        return result;
    }
};
static_assert(std::is_standard_layout_v<FileStream>);

inline std::FILE* GetNativeStream(FileStream* stream) {
    if (!stream) throw std::runtime_error("FileStream: null stream");
    return stream->GetHandle();
}

extern "C" {
extern FileStream _Stdout_nid_postfix;
extern FileStream _Stderr_nid_postfix;
}

#endif
