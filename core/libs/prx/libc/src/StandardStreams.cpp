#include "prx/libc/include/FileStream.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cerrno>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

namespace { FileStream input{stdin}; }
extern "C" {
FileStream* __stdinp_nid_postfix = &input;
FileStream* __stdoutp_nid_postfix = &_Stdout_nid_postfix;
FileStream* __stderrp_nid_postfix = &_Stderr_nid_postfix;
int __isthreaded_nid_postfix = 1;

int APS5_VABI fgetc_nid_postfix(FileStream* stream) {
    const int result = std::fgetc(GetNativeStream(stream));
    stream->SyncStatus();
    return result;
}
std::int32_t APS5_VABI fgetwc_nid_postfix(FileStream* stream) {
    return fgetc_nid_postfix(stream);
}

std::int32_t APS5_VABI ungetwc_nid_postfix(std::int32_t value, FileStream* stream) {
    if (value == -1) return -1;
    if (value < 0 || value > 255) {
        errno = 86;
        stream->SetEncodingError();
        return -1;
    }
    const int result = std::ungetc(value, GetNativeStream(stream));
    stream->SyncStatus();
    return result;
}
int APS5_VABI getc_nid_postfix(FileStream* stream) { return fgetc_nid_postfix(stream); }
int APS5_VABI __srget_nid_postfix(FileStream* stream) { return fgetc_nid_postfix(stream); }
int APS5_VABI getchar_nid_postfix() { return fgetc_nid_postfix(__stdinp_nid_postfix); }
int APS5_VABI fputc_nid_postfix(int value, FileStream* stream) {
    const int result = std::fputc(value, GetNativeStream(stream));
    stream->SyncStatus();
    return result;
}
int APS5_VABI putc_nid_postfix(int value, FileStream* stream) { return fputc_nid_postfix(value, stream); }
int APS5_VABI __swbuf_nid_postfix(int value, FileStream* stream) { return fputc_nid_postfix(value, stream); }
int APS5_VABI putchar_nid_postfix(int value) { return fputc_nid_postfix(value, __stdoutp_nid_postfix); }
int APS5_VABI ungetc_nid_postfix(int value, FileStream* stream) {
    const int result = std::ungetc(value, GetNativeStream(stream));
    stream->SyncStatus();
    return result;
}
char* APS5_VABI fgets_nid_postfix(char* buffer, int size, FileStream* stream) {
    auto* native = GetNativeStream(stream);
    if (size <= 0) {
        stream->SetEncodingError();
        errno = EINVAL;
        return nullptr;
    }
    auto* result = std::fgets(buffer, size, native);
    stream->SyncStatus();
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
void APS5_VABI clearerr_nid_postfix(FileStream* stream) {
    stream->ClearError();
}
int APS5_VABI fileno_nid_postfix(FileStream* stream) {
#ifdef _WIN32
    const int descriptor = _fileno(GetNativeStream(stream));
#else
    const int descriptor = ::fileno(GetNativeStream(stream));
#endif
    stream->GuestState().descriptor = static_cast<std::int16_t>(descriptor);
    return descriptor;
}
int APS5_VABI setvbuf_nid_postfix(FileStream* stream, char* buffer, int mode, std::size_t size) {
    if (mode < 0 || mode > 2) return -1;
    const int native = mode == 0 ? _IOFBF : mode == 1 ? _IOLBF : _IONBF;
    if (native != _IONBF && size == 0) {
        buffer = nullptr;
        size = BUFSIZ;
    }
    return std::setvbuf(GetNativeStream(stream), buffer, native, size);
}
void APS5_VABI setbuf_nid_postfix(FileStream* stream, char* buffer) {
    setvbuf_nid_postfix(stream, buffer, buffer ? 0 : 2, buffer ? 1024 : 0);
}
}
