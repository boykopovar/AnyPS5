#include "prx/libc/include/FileStream.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#ifdef _WIN32
#include <io.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

namespace {
#ifndef _WIN32
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
void APS5_VABI flockfile_nid_postfix(FileStream*);
void APS5_VABI funlockfile_nid_postfix(FileStream*);
FileStream* __stdinp_nid_postfix = &input;
FileStream* __stdoutp_nid_postfix = &_Stdout_nid_postfix;
FileStream* __stderrp_nid_postfix = &_Stderr_nid_postfix;
int __isthreaded_nid_postfix = 1;
std::size_t APS5_VABI mbrtowc_nid_postfix(std::uint16_t* destination, const char* source, std::size_t count, void* state);
int APS5_VABI mbsinit_nid_postfix(const void* state);
void HostEntropy_nid_no_patch(void* buffer, std::size_t size);

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
    if (stream == nullptr) { errno = 22; return -1; }
    if (!stream->IsOpen()) { errno = 9; return -1; }
    auto* native = stream->GetHandle();
    struct HostStreamLock {
        std::FILE* stream;
        explicit HostStreamLock(std::FILE* value) : stream(value) {
#ifdef _WIN32
            ::_lock_file(stream);
#else
            ::flockfile(stream);
#endif
        }
        ~HostStreamLock() {
#ifdef _WIN32
            ::_unlock_file(stream);
#else
            ::funlockfile(stream);
#endif
        }
    } lock(native);
    if (!stream->SwitchToReadingUnlocked()) return -1;
    std::uint16_t character = 0;
    auto* const state = stream->WideConversionState();
    for (;;) {
        int next;
        if (stream->IsRandomDevice()) {
            unsigned char value = 0;
            HostEntropy_nid_no_patch(&value, sizeof(value));
            next = value;
        } else {
#ifdef _WIN32
            next = ::_fgetc_nolock(native);
#else
            next = ::getc_unlocked(native);
#endif
        }
        if (next == EOF) {
            stream->SyncUnlockedStatus();
            stream->KeepDirectoryReadError(true);
            if (mbsinit_nid_postfix(state) == 0) {
                errno = 86;
                stream->SetError();
                stream->KeepDirectoryReadError(true);
            }
            return -1;
        }
        const char byte = static_cast<char>(static_cast<unsigned char>(next));
        const std::size_t converted = mbrtowc_nid_postfix(&character, &byte, 1, state);
        if (converted == static_cast<std::size_t>(-1)) {
            stream->SyncUnlockedStatus();
            stream->SetError();
            return -1;
        }
        if (converted == static_cast<std::size_t>(-2)) continue;
        stream->SyncUnlockedStatus();
        return character;
    }
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
