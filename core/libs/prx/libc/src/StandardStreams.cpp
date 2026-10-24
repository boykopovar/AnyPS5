#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/FileStream.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#include <stdlib.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

#ifdef _WIN32
extern "C" _invalid_parameter_handler _set_thread_local_invalid_parameter_handler(_invalid_parameter_handler);
#endif

namespace {
#ifdef _WIN32
constexpr std::intptr_t NoStreamHandle = -2;

std::intptr_t StandardHandle(int descriptor) {
    const auto previous = _set_thread_local_invalid_parameter_handler([](const wchar_t*, const wchar_t*, const wchar_t*, unsigned int, std::uintptr_t) {});
    const std::intptr_t handle = ::_get_osfhandle(descriptor);
    _set_thread_local_invalid_parameter_handler(previous);
    return handle;
}

}

extern "C" void PrepareStandardStreams_nid_no_patch() {
    static const bool ready = [] {
        for (int descriptor = 0; descriptor <= 2; ++descriptor) {
            const std::intptr_t handle = StandardHandle(descriptor);
            if (handle != -1 && handle != NoStreamHandle) continue;
            auto* const stream = __acrt_iob_func(descriptor);
            const int existing = ::_fileno(stream);
            if (existing >= 0) {
                const std::intptr_t existingHandle = StandardHandle(existing);
                if (existingHandle != -1 && existingHandle != NoStreamHandle) {
                    if (::_dup2(existing, descriptor) != 0)
                        throw std::runtime_error("cannot restore a standard descriptor from its stream");
                    continue;
                }
            }
            const int opened = ::_open("NUL", (descriptor == 0 ? _O_RDONLY : _O_WRONLY) | _O_BINARY);
            if (opened < 0) throw std::runtime_error("cannot open NUL for a missing standard descriptor");
            if (opened > descriptor) {
                const int moved = ::_dup2(opened, descriptor);
                ::_close(opened);
                if (moved != 0) throw std::runtime_error("cannot restore a missing standard descriptor");
            }
            if (::_fileno(stream) != descriptor && ::_close(descriptor) != 0)
                throw std::runtime_error("cannot release a restored standard descriptor");
            if (::_wfreopen(L"NUL", descriptor == 0 ? L"rb" : L"wb", stream) != stream || ::_fileno(stream) != descriptor)
                throw std::runtime_error("cannot attach a standard stream to its NUL descriptor");
        }
        return true;
    }();
    static_cast<void>(ready);
}

namespace {
#else
const bool standardDescriptorsOpen = [] {
    for (int descriptor = 0; descriptor <= 2; ++descriptor) {
        if (::fcntl(descriptor, F_GETFD) != -1 || errno != EBADF) continue;
        const int opened = ::open("/dev/null", descriptor == 0 ? O_RDONLY : O_WRONLY);
        if (opened > descriptor) {
            ::dup2(opened, descriptor);
            ::close(opened);
        }
    }
    return true;
}();
#endif
FileStream input{stdin};
}
extern "C" {

FileStream* __stdinp_nid_postfix = &input;
FileStream* __stdoutp_nid_postfix = &_Stdout_nid_postfix;
FileStream* __stderrp_nid_postfix = &_Stderr_nid_postfix;
int __isthreaded_nid_postfix = 1;

int APS5_VABI fgetc_nid_postfix(FileStream* stream) {
    if (stream && !stream->IsOpen()) { errno = 9; return -1; }
    auto* native = GetNativeStream(stream);
    if (!stream->SwitchToReading()) return -1;
    const int result = std::fgetc(native);
    stream->SyncStatus();
    stream->KeepDirectoryReadError(result == -1);
    return result;
}
std::int32_t APS5_VABI fgetwc_nid_postfix(FileStream* stream) {
    if (stream == nullptr) {
        errno = 22;
        return -1;
    }
    const int first = fgetc_nid_postfix(stream);
    if (first == -1) return -1;
    const unsigned char lead = static_cast<unsigned char>(first);
    if (lead < 0x80) return lead;
    const std::size_t extra = lead >= 0xf0 ? 3 : lead >= 0xe0 ? 2 : lead >= 0xc0 ? 1 : 0;
    if (extra == 0) {
        errno = 86;
        stream->SetError();
        return -1;
    }
    std::uint32_t code = extra == 3 ? (lead & 0x07) : extra == 2 ? (lead & 0x0f) : (lead & 0x1f);
    for (std::size_t index = 0; index < extra; ++index) {
        const int next = fgetc_nid_postfix(stream);
        if (next == -1 || (static_cast<unsigned char>(next) & 0xc0) != 0x80) {
            errno = 86;
            stream->SetError();
            return -1;
        }
        code = (code << 6) | (static_cast<unsigned char>(next) & 0x3f);
    }
    return static_cast<std::int32_t>(code);
}

std::int32_t APS5_VABI ungetwc_nid_postfix(std::int32_t value, FileStream* stream) {
    if (value == -1) return -1;
    if (value < 0 || value > 255) {
        errno = 86;
        stream->SetError();
        return -1;
    }
    auto* native = GetNativeStream(stream);
    if (!stream->SwitchToReading()) return -1;
    const int result = std::ungetc(value, native);
    stream->SyncStatus();
    return result;
}
int APS5_VABI getc_nid_postfix(FileStream* stream) { return fgetc_nid_postfix(stream); }
int APS5_VABI getc_unlocked_nid_postfix(FileStream* stream) {
    if (stream && !stream->IsOpen()) { errno = 9; return -1; }
    auto* native = GetNativeStream(stream);
    if (!stream->SwitchToReadingUnlocked()) return -1;
#ifdef _WIN32
    const int result = _fgetc_nolock(native);
#else
    const int result = getc_unlocked(native);
#endif
    stream->SyncUnlockedStatus();
    stream->KeepDirectoryReadError(result == -1);
    return result;
}
int APS5_VABI __srget_nid_postfix(FileStream* stream) { return fgetc_nid_postfix(stream); }
int APS5_VABI getchar_nid_postfix() { return fgetc_nid_postfix(__stdinp_nid_postfix); }
char* APS5_VABI gets_nid_postfix(char* buffer) {
    if (buffer == nullptr) throw std::invalid_argument("gets: null buffer");
    auto* stream = __stdinp_nid_postfix;
    if (stream && !stream->IsOpen()) { errno = 9; return nullptr; }
    flockfile_nid_postfix(stream);

    struct Unlock {
        FileStream* stream;
        ~Unlock() { funlockfile_nid_postfix(stream); }
    } unlock{stream};
    char* cursor = buffer;
    for (;;) {
        const int value = fgetc_nid_postfix(stream);
        if (value == EOF) {
            if (cursor == buffer) return nullptr;
            *cursor = 0;
            return buffer;
        }
        if (value == '\n') {
            *cursor = 0;
            return buffer;
        }
        *cursor++ = static_cast<char>(value);
    }
}
std::int32_t APS5_VABI getwc_nid_postfix(FileStream* stream) { return fgetwc_nid_postfix(stream); }
std::int32_t APS5_VABI getwchar_nid_postfix() { return fgetwc_nid_postfix(__stdinp_nid_postfix); }
int APS5_VABI getchar_unlocked_nid_postfix() { return getc_unlocked_nid_postfix(__stdinp_nid_postfix); }
int APS5_VABI fputc_nid_postfix(int value, FileStream* stream) {
    if (stream && !stream->IsOpen()) { errno = 9; return -1; }
    const int result = std::fputc(value, GetNativeStream(stream));
    stream->SyncStatus();
    return result;
}
int APS5_VABI putc_nid_postfix(int value, FileStream* stream) { return fputc_nid_postfix(value, stream); }
int APS5_VABI putc_unlocked_nid_postfix(int value, FileStream* stream) {
    if (stream && !stream->IsOpen()) { errno = 9; return -1; }
#ifdef _WIN32
    const int result = _fputc_nolock(value, GetNativeStream(stream));
#else
    const int result = putc_unlocked(value, GetNativeStream(stream));
#endif
    stream->SyncUnlockedStatus();
    return result;
}
int APS5_VABI __swbuf_nid_postfix(int value, FileStream* stream) { return fputc_nid_postfix(value, stream); }
int APS5_VABI putchar_nid_postfix(int value) { return fputc_nid_postfix(value, __stdoutp_nid_postfix); }
int APS5_VABI putchar_unlocked_nid_postfix(int value) { return putc_unlocked_nid_postfix(value, __stdoutp_nid_postfix); }
int APS5_VABI ungetc_nid_postfix(int value, FileStream* stream) {
    if (stream && !stream->IsOpen()) { errno = 9; return -1; }
    auto* native = GetNativeStream(stream);
    if (value != EOF && !stream->SwitchToReading()) return -1;
    const int result = std::ungetc(value, native);
    stream->SyncStatus();
    return result;
}
char* APS5_VABI fgets_nid_postfix(char* buffer, int size, FileStream* stream) {
    if (stream && !stream->IsOpen()) { errno = 9; return nullptr; }
    if (size <= 0) {
        stream->SetError();
        errno = 22;
        return nullptr;
    }
    auto* native = GetNativeStream(stream);
    if (!stream->SwitchToReading()) return nullptr;
    auto* result = std::fgets(buffer, size, native);
    stream->SyncStatus();
    stream->KeepDirectoryReadError(result == nullptr);
    return result;
}
int APS5_VABI feof_nid_postfix(FileStream* stream) {
    stream->SyncStatus();
    return (stream->GuestState().flags & 0x20) != 0;
}
int APS5_VABI ferror_nid_postfix(FileStream* stream) {
    stream->SyncStatus();
    return (stream->GuestState().flags & 0x40) != 0;
}

int APS5_VABI feof_unlocked_nid_postfix(FileStream* stream) {
    stream->SyncUnlockedStatus();
    return (stream->GuestState().flags & 0x20) != 0;
}

int APS5_VABI ferror_unlocked_nid_postfix(FileStream* stream) {
    stream->SyncUnlockedStatus();
    return (stream->GuestState().flags & 0x40) != 0;
}
void APS5_VABI clearerr_nid_postfix(FileStream* stream) {
    stream->ClearError();
}
int APS5_VABI fileno_nid_postfix(FileStream* stream) {
    if (stream && !stream->IsOpen()) { errno = 9; return -1; }
#ifdef _WIN32
    const int descriptor = _fileno(GetNativeStream(stream));
#else
    const int descriptor = ::fileno(GetNativeStream(stream));
#endif
    stream->GuestState().descriptor = static_cast<std::int16_t>(descriptor);
    return descriptor;
}
void APS5_VABI clearerr_unlocked_nid_postfix(FileStream* stream) {
    stream->ClearError();
}
int APS5_VABI fileno_unlocked_nid_postfix(FileStream* stream) {
    if (stream == nullptr || !stream->IsOpen()) return -1;
    return fileno_nid_postfix(stream);
}
[[noreturn]] char* APS5_VABI fgetln_nid_postfix(FileStream*, std::size_t*) {
    throw std::runtime_error("fgetln: the console's export is a hlt instruction, which ends the process");
}
int APS5_VABI setvbuf_nid_postfix(FileStream* stream, char* buffer, int mode, std::size_t size) {
    if (stream && !stream->IsOpen()) { errno = 9; return -1; }
    if (mode < 0 || mode > 2) return -1;
    const int native = mode == 0 ? _IOFBF : mode == 1 ? _IOLBF : _IONBF;
    if (native != _IONBF && size == 0) {
        buffer = nullptr;
        size = BUFSIZ;
    }
    auto* handle = GetStreamHandle(stream);
#ifdef _WIN32
    if (handle == __acrt_iob_func(1) && KeepHostStandardOutput_nid_no_patch() < 0) return -1;
    if (handle == __acrt_iob_func(2) && KeepHostStandardError_nid_no_patch() < 0) return -1;
#endif
    return std::setvbuf(handle, buffer, native, size);
}
void APS5_VABI setbuf_nid_postfix(FileStream* stream, char* buffer) {
    setvbuf_nid_postfix(stream, buffer, buffer ? 0 : 2, buffer ? 1024 : 0);

}
}
