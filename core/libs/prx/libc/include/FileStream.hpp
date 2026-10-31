#ifndef CORE_LIBS_PRX_LIBC_INCLUDE_FILESTREAM_HPP
#define CORE_LIBS_PRX_LIBC_INCLUDE_FILESTREAM_HPP

#include <atomic>
#include <cerrno>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
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

extern "C" int KeepHostStandardError_nid_no_patch();
extern "C" int KeepHostStandardOutput_nid_no_patch();

class FileStream;
extern "C" std::recursive_mutex& FileStreamRegistryMutex_nid_no_patch();
extern "C" FileStream*& FileStreamRegistryHead_nid_no_patch();
void RegisterFileStream_nid_no_patch(FileStream* stream, std::shared_ptr<std::recursive_mutex> preallocatedGate = {});
void UnlinkFileStream_nid_no_patch(FileStream* stream);
std::shared_ptr<std::recursive_mutex> PinFileStreamIfValid_nid_no_patch(FileStream* stream, bool allowRetiredStatic = false);
void UnpinFileStream_nid_no_patch(FileStream* stream);
void RetireFileStream_nid_no_patch(FileStream* stream);
std::optional<std::uint64_t> ReserveFileStreamReopen_nid_no_patch(FileStream* stream);
void PublishFileStreamReopen_nid_no_patch(FileStream* stream, std::uint64_t reservation);
void CancelFileStreamReopenReservation_nid_no_patch(std::uint64_t reservation);
bool IsFileStreamInSnapshot_nid_no_patch(FileStream* stream, std::uint64_t snapshot);
std::pair<FileStream*, std::shared_ptr<std::recursive_mutex>>
    PinNextFileStream_nid_no_patch(std::uint64_t& cursor);
void DetachFileStreamAliasesForNative_nid_no_patch(FileStream* source, std::FILE* native);
void SynchronizeFileStreamAliases_nid_no_patch(FileStream* source);

class FileStream {
    GuestFilePrefix _guest{};
    std::byte _reserved[256 - sizeof(GuestFilePrefix)]{};
    std::FILE* _handle;
    bool _dynamic;
    bool guestError = false;
    bool randomDevice = false;
    bool directory = false;
    char16_t lowSurrogate = 0;
    unsigned pushedBack = 0;
    FileStream* _nextStream_nid_no_patch = nullptr;
    std::shared_ptr<std::recursive_mutex> _gate_nid_no_patch;
    std::uint64_t _registryId_nid_no_patch = 0;
    std::uint32_t _pinCount_nid_no_patch = 0;
    bool _registered_nid_no_patch = false;
    bool _retired_nid_no_patch = false;

    friend void RegisterFileStream_nid_no_patch(FileStream*, std::shared_ptr<std::recursive_mutex>);
    friend void UnlinkFileStream_nid_no_patch(FileStream*);
    friend std::shared_ptr<std::recursive_mutex> PinFileStreamIfValid_nid_no_patch(FileStream*, bool);
    friend void UnpinFileStream_nid_no_patch(FileStream*);
    friend void RetireFileStream_nid_no_patch(FileStream*);
    friend std::optional<std::uint64_t> ReserveFileStreamReopen_nid_no_patch(FileStream*);
    friend void PublishFileStreamReopen_nid_no_patch(FileStream*, std::uint64_t);
    friend bool IsFileStreamInSnapshot_nid_no_patch(FileStream*, std::uint64_t);
    friend std::pair<FileStream*, std::shared_ptr<std::recursive_mutex>>
        PinNextFileStream_nid_no_patch(std::uint64_t&);
    friend void DetachFileStreamAliasesForNative_nid_no_patch(FileStream*, std::FILE*);
    friend void SynchronizeFileStreamAliases_nid_no_patch(FileStream*);

    std::FILE* LoadHandleAtomic_nid_no_patch() const {
        return __atomic_load_n(const_cast<std::FILE* const*>(&_handle), __ATOMIC_ACQUIRE);
    }

    void StoreHandleAtomic_nid_no_patch(std::FILE* value) {
        __atomic_store_n(&_handle, value, __ATOMIC_RELEASE);
    }

public:
    enum class Registration { Immediate, Deferred };

    explicit FileStream(std::FILE* handle, bool dynamic = false, Registration registration = Registration::Immediate)
        : _handle(handle), _dynamic(dynamic) {
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
        if (registration == Registration::Immediate) RegisterFileStream_nid_no_patch(this);
    }

    ~FileStream() {
        if (_registered_nid_no_patch) UnlinkFileStream_nid_no_patch(this);
    }

    void Register_nid_no_patch(std::shared_ptr<std::recursive_mutex> preallocatedGate = {}) {
        RegisterFileStream_nid_no_patch(this, std::move(preallocatedGate));
    }

    static std::recursive_mutex& RegistryMutex_nid_no_patch() { return FileStreamRegistryMutex_nid_no_patch(); }
    static FileStream* RegistryHead_nid_no_patch() { return FileStreamRegistryHead_nid_no_patch(); }
    FileStream* NextStream_nid_no_patch() const { return _nextStream_nid_no_patch; }
    std::FILE* RawHandle_nid_no_patch() const { return LoadHandleAtomic_nid_no_patch(); }
    std::shared_ptr<std::recursive_mutex> GatePtr_nid_no_patch() const { return _gate_nid_no_patch; }
    bool IsRetired_nid_no_patch() const { return _retired_nid_no_patch; }

    FileStream(const FileStream&) = delete;
    FileStream& operator=(const FileStream&) = delete;

    std::FILE* GetHandle() const {
        auto* h = LoadHandleAtomic_nid_no_patch();
        if (!h) throw std::runtime_error("FileStream: closed stream");
        return h;
    }

    bool IsOpen() const {
        return LoadHandleAtomic_nid_no_patch() != nullptr;
    }

    bool IsDynamic() const {
        return _dynamic;
    }

    GuestFilePrefix& GuestState() { return _guest; }
    bool IsHostStandard() const {
        auto* h = LoadHandleAtomic_nid_no_patch();
#ifdef _WIN32
        return h == __acrt_iob_func(0) || h == __acrt_iob_func(1) || h == __acrt_iob_func(2);
#else
        return h == stdin || h == stdout || h == stderr;
#endif
    }

    void Detach() {
        StoreHandleAtomic_nid_no_patch(nullptr);
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
        _guest = {};
        guestError = false;
        randomDevice = false;
        lowSurrogate = 0;
        pushedBack = 0;
        directory = false;
        if (!handle) {
            StoreHandleAtomic_nid_no_patch(nullptr);
            return false;
        }
        _guest.flags = 0x10;
#ifdef _WIN32
        const int descriptor = _fileno(handle);
#else
        const int descriptor = ::fileno(handle);
#endif
        _guest.descriptor = descriptor >= 0 && descriptor <= 32767 ? static_cast<std::int16_t>(descriptor) : -1;
        StoreHandleAtomic_nid_no_patch(handle);
        return true;
    }

    bool Reopen(const std::filesystem::path& filename, const char* mode,
        [[maybe_unused]] std::FILE* (*open)(const std::filesystem::path&, const std::string&) = nullptr) {
        struct SynchronizeAliases {
            FileStream* source;
            ~SynchronizeAliases() { SynchronizeFileStreamAliases_nid_no_patch(source); }
        } synchronizeAliases{this};
        auto* previous = LoadHandleAtomic_nid_no_patch();
        if (!previous) {
            if (!open || filename.empty()) {
                errno = ENOENT;
                return false;
            }
            std::unique_ptr<std::FILE, int (*)(std::FILE*)> replacement(open(filename, mode), std::fclose);
            if (!Adopt(replacement.get())) return false;
            replacement.release();
            return true;
        }
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
        const int error = errno;
        if (IsHostStandard() && replaced == 1 && KeepHostStandardOutput_nid_no_patch() < 0) errno = error;
        if (IsHostStandard() && replaced == 2 && KeepHostStandardError_nid_no_patch() < 0) errno = error;
        if (open && !filename.empty()) {
            if (!IsHostStandard()) {
                StoreHandleAtomic_nid_no_patch(nullptr);
                std::erase(wideMode, L'x');
                std::unique_ptr<std::FILE, int (*)(std::FILE*)> original(
                    ::_wfreopen(L"NUL", wideMode.c_str(), previous), std::fclose);
                if (original) {
                    std::unique_ptr<std::FILE, int (*)(std::FILE*)> replacement(open(filename, mode), std::fclose);
                    if (replacement && ::_dup2(::_fileno(replacement.get()), ::_fileno(original.get())) == 0)
                        StoreHandleAtomic_nid_no_patch(original.release());
                    const int error = errno;
                    replacement.reset();
                    original.reset();
                    errno = error;
                }
            } else {
                std::wstring resetMode;
                for (const char character : std::string_view(mode))
                    if (character != 'x') resetMode.push_back(static_cast<unsigned char>(character));
                const int kept = DuplicateHostDescriptor_nid_no_patch(replaced);
                if (kept >= 0) ::_lock_file(previous);
                StoreHandleAtomic_nid_no_patch(nullptr);
                try {
                    std::FILE* const replacement = open(filename, mode);
                    if (replacement) {
                        if (::_wfreopen(L"NUL", resetMode.c_str(), previous) == previous) {
                            const int moved = ::_dup2(_fileno(replacement), replaced);
                            const int error = errno;
                            std::fclose(replacement);
                            if (moved == 0) StoreHandleAtomic_nid_no_patch(previous);
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
            StoreHandleAtomic_nid_no_patch(::_wfreopen(filename.c_str(), wideMode.c_str(), previous));
            if (kept >= 0) RestoreHostStandard(previous, kept);
        }
        KeepStandardHandle_nid_no_patch(replaced, standard);
#else
        const int error = errno;
        try {
#ifndef __EMSCRIPTEN__
            if (::fileno(previous) == 1 && KeepHostStandardOutput_nid_no_patch() < 0) errno = error;
#endif
            if (::fileno(previous) == 2 && KeepHostStandardError_nid_no_patch() < 0) errno = error;
        } catch (const std::system_error&) {
            errno = error;
        }
        const int kept = IsHostStandard() ? DuplicateHostDescriptor_nid_no_patch(::fileno(previous)) : -1;
        if (kept >= 0) ::flockfile(previous);
        StoreHandleAtomic_nid_no_patch(std::freopen(filename.c_str(), mode, previous));
        if (kept >= 0) RestoreHostStandard(previous, kept);
#endif
        return Adopt(LoadHandleAtomic_nid_no_patch());
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
        auto* h = LoadHandleAtomic_nid_no_patch();
        if (!h) return;
        PublishStatus(std::feof(h) != 0, std::ferror(h) != 0 || guestError);
    }

    void SyncUnlockedStatus() {
        auto* h = LoadHandleAtomic_nid_no_patch();
        if (!h) return;
#ifdef _WIN32
        const bool endOfFile = std::feof(h) != 0;
        const bool error = std::ferror(h) != 0;
#elif defined(__APPLE__)
        const bool endOfFile = feof_unlocked(h) != 0;
        const bool error = ferror_unlocked(h) != 0;
#else
        const bool endOfFile = feof_unlocked(h) != 0;
        const bool error = ferror_unlocked(h) != 0;
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
        auto* h = LoadHandleAtomic_nid_no_patch();
        if (!h) return;
        std::clearerr(h);
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

    int CloseNativeUnderGate_nid_no_patch() {
        auto* native = LoadHandleAtomic_nid_no_patch();
        if (!native) { errno = 9; return -1; }
#ifdef _WIN32
        const int descriptor = ::_fileno(native);
        GuestFileLockForget_nid_no_patch(descriptor);
        if (std::exchange(directory, false)) ForgetDirectoryStream_nid_no_patch(descriptor);
#endif
        StoreHandleAtomic_nid_no_patch(nullptr);
        _guest.flags = 0;
        _guest.descriptor = -1;
        lowSurrogate = 0;
        pushedBack = 0;
        DetachFileStreamAliasesForNative_nid_no_patch(this, native);
        return std::fclose(native);
    }

    int Close() {
        auto gate = PinFileStreamIfValid_nid_no_patch(this);
        if (!gate) { errno = 9; return -1; }
        struct PinGuard {
            FileStream* stream;
            ~PinGuard() { UnpinFileStream_nid_no_patch(stream); }
        } pinGuard{this};
        std::lock_guard<std::recursive_mutex> gateLock(*gate);
        if (!LoadHandleAtomic_nid_no_patch()) { errno = 9; return -1; }
        return CloseNativeUnderGate_nid_no_patch();
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

class FileStreamLease_nid_no_patch {
    FileStream* _stream;
    std::shared_ptr<std::recursive_mutex> _gate;
public:
    FileStreamLease_nid_no_patch(FileStream* stream, std::shared_ptr<std::recursive_mutex> gate)
        : _stream(stream), _gate(std::move(gate)) {}
    FileStreamLease_nid_no_patch(const FileStreamLease_nid_no_patch&) = delete;
    FileStreamLease_nid_no_patch& operator=(const FileStreamLease_nid_no_patch&) = delete;
    FileStreamLease_nid_no_patch(FileStreamLease_nid_no_patch&& other) noexcept
        : _stream(other._stream), _gate(std::move(other._gate)) { other._stream = nullptr; }
    FileStreamLease_nid_no_patch& operator=(FileStreamLease_nid_no_patch&&) = delete;
    ~FileStreamLease_nid_no_patch() {
        if (_stream) UnpinFileStream_nid_no_patch(_stream);
    }
    FileStream* Stream() const { return _stream; }
    const std::shared_ptr<std::recursive_mutex>& Gate() const { return _gate; }
    bool Valid() const { return _stream != nullptr && _gate != nullptr; }
};

inline FileStreamLease_nid_no_patch AcquireFileStreamLease_nid_no_patch(FileStream* stream, bool allowRetiredStatic = false) {
    auto gate = PinFileStreamIfValid_nid_no_patch(stream, allowRetiredStatic);
    FileStream* const pinned = gate ? stream : nullptr;
    return FileStreamLease_nid_no_patch(pinned, std::move(gate));
}

extern "C" {
extern FileStream _Stdout_nid_postfix;
extern FileStream _Stderr_nid_postfix;
}

#endif
