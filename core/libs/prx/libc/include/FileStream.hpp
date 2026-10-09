#ifndef CORE_LIBS_PRX_LIBC_INCLUDE_FILESTREAM_HPP
#define CORE_LIBS_PRX_LIBC_INCLUDE_FILESTREAM_HPP

#include <cstdio>
#include <stdexcept>
#include <utility>
#include <cstdint>
#include <cstddef>
#include <type_traits>
#include <filesystem>
#ifdef _WIN32
#include <io.h>
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

class FileStream {
    // Only the macro-accessed FreeBSD prefix is exposed. The remaining guest
    // FILE area is reserved; host state lives beyond it and is never a guest FILE*.
    GuestFilePrefix _guest{};
    std::byte _reserved[256 - sizeof(GuestFilePrefix)]{};
    std::FILE* _handle;
    bool _dynamic;
    bool encodingError = false;

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

    bool IsDynamic() const {
        return _dynamic;
    }

    GuestFilePrefix& GuestState() { return _guest; }
#ifdef _WIN32
    bool Reopen(const std::filesystem::path& path, const wchar_t* mode) {
        auto* previous = GetHandle();
        _guest = {};
        encodingError = false;
        _handle = path.empty() ? ::_wfreopen(nullptr, mode, previous) : ::_wfreopen(path.c_str(), mode, previous);
        if (!_handle) return false;
        _guest.flags = 0x10;
        const int descriptor = _fileno(_handle);
        _guest.descriptor = descriptor >= 0 && descriptor <= 32767 ? static_cast<std::int16_t>(descriptor) : -1;
        return true;
    }
    bool Reopen(const char* filename, const char* mode) {
        const std::string modeStr = mode ? mode : "";
        const std::wstring wideMode(modeStr.begin(), modeStr.end());
        return Reopen(filename ? std::filesystem::path(filename) : std::filesystem::path{}, wideMode.c_str());
    }
#else
    bool Reopen(const std::filesystem::path& path, const char* mode) {
        auto* previous = GetHandle();
        _guest = {};
        encodingError = false;
        _handle = path.empty() ? std::freopen(nullptr, mode, previous) : std::freopen(path.c_str(), mode, previous);
        if (!_handle) return false;
        _guest.flags = 0x10;
        const int descriptor = ::fileno(_handle);
        _guest.descriptor = descriptor >= 0 && descriptor <= 32767 ? static_cast<std::int16_t>(descriptor) : -1;
        return true;
    }
    bool Reopen(const char* filename, const char* mode) {
        return Reopen(filename ? std::filesystem::path(filename) : std::filesystem::path{}, mode);
    }
#endif
    void SyncStatus() {
        _guest.readRemaining = 0;
        _guest.writeRemaining = 0;
        _guest.flags = static_cast<std::int16_t>((_guest.flags & ~0x60) |
            (std::feof(GetHandle()) ? 0x20 : 0) | (std::ferror(GetHandle()) || encodingError ? 0x40 : 0));
    }

    void SetEncodingError() {
        encodingError = true;
        SyncStatus();
    }

    void ClearError() {
        std::clearerr(GetHandle());
        encodingError = false;
        SyncStatus();
    }

    void Close() {
        GetHandle();
        if (std::fclose(std::exchange(_handle, nullptr)) != 0) throw std::runtime_error("FileStream: close failed");
        _guest.flags = 0;
        _guest.descriptor = -1;
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
