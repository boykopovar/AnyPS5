#ifndef CORE_LIBS_PRX_LIBC_INCLUDE_FILESTREAM_HPP
#define CORE_LIBS_PRX_LIBC_INCLUDE_FILESTREAM_HPP

#include <atomic>
#include <cerrno>
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
extern "C" void PrepareStandardStreams_nid_no_patch();
struct UcrtStreamState {
    char* pointer;
    char* base;
    int count;
    long flags;
};
extern "C" void LibcRegisterDirectoryStreams_nid_no_patch(int (*open)(const wchar_t* path), void (*forget)(int descriptor));
extern "C" void ForgetDirectoryStream_nid_no_patch(int descriptor);
#else
#include <unistd.h>
#endif
#if defined(__GLIBC__)
#include <stdio_ext.h>
extern "C" int _IO_switch_to_get_mode(std::FILE* stream);
#endif
extern "C" int DuplicateHostDescriptor_nid_no_patch(int descriptor);

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
    bool randomDevice = false;
    bool directory = false;
    char16_t lowSurrogate = 0;
    unsigned pushedBack = 0;

public:
    explicit FileStream(std::FILE* handle, bool dynamic = false) : _handle(handle), _dynamic(dynamic) {
        if (!_handle) throw std::runtime_error("FileStream: null handle");
#ifdef _WIN32
        PrepareStandardStreams_nid_no_patch();
#endif
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
    bool IsHostStandard() const {
        return _handle == stdin || _handle == stdout || _handle == stderr;
    }

    void Detach() {
        _handle = nullptr;
        _guest.flags = 0;
        _guest.descriptor = -1;
        lowSurrogate = 0;
        pushedBack = 0;
    }

    void RestoreHostStandard(std::FILE* stream, int kept) {
        const int error = errno;
#ifdef _WIN32
        const bool restored = _handle || (::_wfreopen(L"NUL", stream == stdin ? L"r" : L"w", stream) && ::_dup2(kept, ::_fileno(stream)) == 0);
        ::_unlock_file(stream);
        ::_close(kept);
#else
        const bool restored = _handle || (std::freopen("/dev/null", stream == stdin ? "r" : "w", stream) && ::dup2(kept, ::fileno(stream)) >= 0);
        ::funlockfile(stream);
        ::close(kept);
#endif
        if (!restored) throw std::runtime_error("FileStream: cannot restore the host standard stream");
        errno = error;
    }

    void MarkRandomDevice() {
        randomDevice = true;
        _guest.descriptor = -1;
        lowSurrogate = 0;
        pushedBack = 0;
    }

    bool IsRandomDevice() const {
        return randomDevice;
    }

    void MarkDirectory() {
        directory = true;
    }

    bool IsDirectory() const {
        return directory;
    }

    bool Adopt(std::FILE* handle) {
        _handle = handle;
        _guest = {};
        guestError = false;
        randomDevice = false;
        lowSurrogate = 0;
        pushedBack = 0;
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

    bool Reopen(const std::filesystem::path& filename, const char* mode,
        [[maybe_unused]] std::FILE* (*open)(const std::filesystem::path&, const std::string&) = nullptr) {
        if (!_handle) {
            if (!open || filename.empty()) {
                errno = ENOENT;
                return false;
            }
            return Adopt(open(filename, mode));
        }
        auto* previous = GetHandle();
        _guest = {};
        guestError = false;
        randomDevice = false;
        lowSurrogate = 0;
        pushedBack = 0;
#ifdef _WIN32
        std::wstring wideMode;
        for (const char character : std::string_view(mode)) wideMode.push_back(static_cast<unsigned char>(character));
        const int replaced = previous ? _fileno(previous) : -1;
        if (std::exchange(directory, false)) ForgetDirectoryStream_nid_no_patch(replaced);
        auto* const standard = HostStandardHandle_nid_no_patch(replaced);
        GuestFileLockForget_nid_no_patch(replaced);
        if (open && !filename.empty()) {
            if (!IsHostStandard()) {
                _handle = nullptr;
                std::fclose(previous);
                _handle = open(filename, mode);
            } else {
                std::wstring resetMode;
                for (const char character : std::string_view(mode))
                    if (character != 'x') resetMode.push_back(static_cast<unsigned char>(character));
                const int kept = DuplicateHostDescriptor_nid_no_patch(replaced);
                if (kept >= 0) ::_lock_file(previous);
                _handle = nullptr;
                try {
                    std::FILE* const replacement = open(filename, mode);
                    if (replacement) {
                        if (::_wfreopen(L"NUL", resetMode.c_str(), previous) == previous) {
                            const int moved = ::_dup2(_fileno(replacement), replaced);
                            const int error = errno;
                            std::fclose(replacement);
                            if (moved == 0) _handle = previous;
                            else errno = error;
                        } else {
                            const int error = errno;
                            std::fclose(replacement);
                            errno = error;
                        }
                    }
                } catch (...) {
                    if (kept >= 0) RestoreHostStandard(previous, kept);
                    KeepStandardHandle_nid_no_patch(replaced, standard);
                    throw;
                }
                if (kept >= 0) RestoreHostStandard(previous, kept);
            }
        } else {
            const int kept = IsHostStandard() ? DuplicateHostDescriptor_nid_no_patch(replaced) : -1;
            if (kept >= 0) ::_lock_file(previous);
            _handle = ::_wfreopen(filename.c_str(), wideMode.c_str(), previous);
            if (kept >= 0) RestoreHostStandard(previous, kept);
        }
        KeepStandardHandle_nid_no_patch(replaced, standard);
#else
        if (::fileno(previous) == 2) KeepHostStandardError_nid_no_patch();
        const int kept = IsHostStandard() ? DuplicateHostDescriptor_nid_no_patch(::fileno(previous)) : -1;
        if (kept >= 0) ::flockfile(previous);
        _handle = std::freopen(filename.c_str(), mode, previous);
        if (kept >= 0) RestoreHostStandard(previous, kept);
#endif
        return Adopt(_handle);
    }
    void PublishStatus(bool endOfFile, bool error) {
        __atomic_store_n(&_guest.readRemaining, 0, __ATOMIC_RELAXED);
        __atomic_store_n(&_guest.writeRemaining, 0, __ATOMIC_RELAXED);
        auto old = __atomic_load_n(&_guest.flags, __ATOMIC_RELAXED);
        std::int16_t next;
        do next = static_cast<std::int16_t>((old & ~0x60) | (endOfFile ? 0x20 : 0) | (error ? 0x40 : 0));
        while (!__atomic_compare_exchange_n(&_guest.flags, &old, next, false, __ATOMIC_RELAXED, __ATOMIC_RELAXED));
    }
    void SyncStatus() {
        if (!_handle) return;
        PublishStatus(std::feof(GetHandle()) != 0, std::ferror(GetHandle()) != 0 || guestError);
    }

    void SyncUnlockedStatus() {
        if (!_handle) return;
#ifdef _WIN32
        const bool endOfFile = std::feof(GetHandle()) != 0;
        const bool error = std::ferror(GetHandle()) != 0;
#elif defined(__APPLE__)
        const bool endOfFile = feof_unlocked(GetHandle()) != 0;
        const bool error = ferror_unlocked(GetHandle()) != 0;
#else
        const bool endOfFile = feof_unlocked(GetHandle()) != 0;
        const bool error = ferror_unlocked(GetHandle()) != 0;
#endif
        PublishStatus(endOfFile, error || guestError);
    }

    bool SwitchToReading() {
#ifdef _WIN32
        ::_lock_file(GetHandle());
        const bool switched = SwitchToReadingUnlocked();
        ::_unlock_file(GetHandle());
        return switched;
#else
        ::flockfile(GetHandle());
        const bool switched = SwitchToReadingUnlocked();
        ::funlockfile(GetHandle());
        return switched;
#endif
    }

    bool SwitchToReadingUnlocked() {
#ifdef _WIN32
        constexpr long UcrtReading = 0x0001;
        constexpr long UcrtWriting = 0x0002;
        constexpr long UcrtUpdate = 0x0004;
        auto* native = GetHandle();
        std::atomic_ref<long> state(reinterpret_cast<UcrtStreamState*>(native)->flags);
        if ((state.load() & (UcrtReading | UcrtUpdate)) != UcrtUpdate) return true;
        const bool switched = (state.load() & UcrtWriting) == 0 || ::_fflush_nolock(native) == 0;
        if (switched) state.fetch_or(UcrtReading);
        SyncUnlockedStatus();
        return switched;
#elif defined(__GLIBC__)
        auto* native = GetHandle();
        if (!::__freadable(native) || !::__fwriting(native)) return true;
        const bool switched = ::_IO_switch_to_get_mode(native) == 0;
        SyncUnlockedStatus();
        return switched;
#else
        return true;
#endif
    }

    bool SwitchToWriting() {
#ifdef _WIN32
        ::_lock_file(GetHandle());
        const bool switched = SwitchToWritingUnlocked();
        ::_unlock_file(GetHandle());
        return switched;
#else
        return true;
#endif
    }

    bool SwitchToWritingUnlocked() {
#ifdef _WIN32
        constexpr long UcrtReading = 0x0001;
        constexpr long UcrtWriting = 0x0002;
        constexpr long UcrtUpdate = 0x0004;
        constexpr long UcrtEndOfFile = 0x0008;
        if (randomDevice) return true;
        auto* native = reinterpret_cast<UcrtStreamState*>(GetHandle());
        std::atomic_ref<long> state(native->flags);
        if ((state.load() & (UcrtWriting | UcrtUpdate)) == 0) {
            errno = 9;
            guestError = true;
            SyncUnlockedStatus();
            return false;
        }
        if ((state.load() & (UcrtReading | UcrtUpdate)) != (UcrtReading | UcrtUpdate)) return true;
        native->count = 0;
        native->pointer = native->base;
        state.fetch_and(~(UcrtReading | UcrtEndOfFile));
        SyncUnlockedStatus();
        return true;
#else
        return true;
#endif
    }

    void KeepDirectoryReadError(bool failed) {
        if (failed && directory && (__atomic_load_n(&_guest.flags, __ATOMIC_RELAXED) & 0x40) != 0) errno = 21;
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

    char16_t TakeLowSurrogate() {
        if (pushedBack != 0) return 0;
        return std::exchange(lowSurrogate, char16_t{0});
    }

    void SetLowSurrogate(char16_t unit) {
        lowSurrogate = unit;
        pushedBack = 0;
    }

    void NotePushback() {
        if (lowSurrogate != 0) ++pushedBack;
    }

    void NoteWideRead() {
        if (pushedBack != 0) --pushedBack;
    }

    int Close() {
        GetHandle();
#ifdef _WIN32
        GuestFileLockForget_nid_no_patch(::_fileno(_handle));
        if (std::exchange(directory, false)) ForgetDirectoryStream_nid_no_patch(::_fileno(_handle));
#endif
        const int result = std::fclose(std::exchange(_handle, nullptr));
        _guest.flags = 0;
        _guest.descriptor = -1;
        lowSurrogate = 0;
        pushedBack = 0;
        return result;
    }
};
static_assert(std::is_standard_layout_v<FileStream>);

inline std::FILE* GetStreamHandle(FileStream* stream) {
    if (!stream) throw std::runtime_error("FileStream: null stream");
    return stream->GetHandle();
}

inline std::FILE* GetNativeStream(FileStream* stream) {
    auto* handle = GetStreamHandle(stream);
    if (stream->IsRandomDevice()) throw std::runtime_error("FileStream: this function is not implemented on a /dev/random or /dev/urandom stream");
    return handle;
}

extern "C" {
extern FileStream _Stdout_nid_postfix;
extern FileStream _Stderr_nid_postfix;
}

#endif
