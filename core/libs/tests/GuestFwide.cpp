#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdarg>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace {
using Open = void* (APS5_VABI*)(const char*, const char*);
using Close = int (APS5_VABI*)(void*);
using Wide = int (APS5_VABI*)(void*, int);
using PutByte = int (APS5_VABI*)(int, void*);
using PutWide = int (APS5_VABI*)(char16_t, void*);
using Seek = int (APS5_VABI*)(void*, std::int64_t, int);
using Reopen = void* (APS5_VABI*)(const char*, const char*, void*);
using Read = std::size_t (APS5_VABI*)(void*, std::size_t, std::size_t, void*);
using Write = std::size_t (APS5_VABI*)(const void*, std::size_t, std::size_t, void*);
using Flush = int (APS5_VABI*)(void*);
using Descriptor = int (APS5_VABI*)(void*);
using GetByte = int (APS5_VABI*)(void*);
using GetWide = std::int32_t (APS5_VABI*)(void*);
using UngetWide = std::int32_t (APS5_VABI*)(std::int32_t, void*);
using GetLine = char* (APS5_VABI*)(char*, int, void*);
using GetWideLine = char16_t* (APS5_VABI*)(char16_t*, int, void*);
using ScanList = int (APS5_VABI*)(void*, const char*, void*);
using WideList = int (APS5_VABI*)(void*, const char16_t*, void*);

int APS5_VABI ScanThrough(ScanList scan, void* stream, const char* format, ...) {
#ifdef _WIN32
    __builtin_sysv_va_list args;
    __builtin_sysv_va_start(args, format);
#else
    std::va_list args;
    va_start(args, format);
#endif
    const int result = scan(stream, format, reinterpret_cast<void*>(args));
#ifdef _WIN32
    __builtin_sysv_va_end(args);
#else
    va_end(args);
#endif
    return result;
}

int APS5_VABI WideThrough(WideList print, void* stream, const char16_t* format, ...) {
#ifdef _WIN32
    __builtin_sysv_va_list args;
    __builtin_sysv_va_start(args, format);
#else
    std::va_list args;
    va_start(args, format);
#endif
    const int result = print(stream, format, reinterpret_cast<void*>(args));
#ifdef _WIN32
    __builtin_sysv_va_end(args);
#else
    va_end(args);
#endif
    return result;
}

void Require(bool value, const char* message) {
    if (!value) {
        std::fprintf(stderr, "%s\n", message);
        std::abort();
    }
}

template<class Function>
Function Symbol(
#ifdef _WIN32
    HMODULE library,
#else
    void* library,
#endif
    const char* name) {
#ifdef _WIN32
    return reinterpret_cast<Function>(GetProcAddress(library, name));
#else
    return reinterpret_cast<Function>(dlsym(library, name));
#endif
}
}

int main(int argc, char** argv) {
    Require(argc == 3, "expected libc path and scratch filename");
#ifdef _WIN32
    const auto library = LoadLibraryA(argv[1]);
#else
    const auto library = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
#endif
    Require(library != nullptr, "could not load libc");
    const auto open = Symbol<Open>(library, "fopen_nid_postfix");
    const auto close = Symbol<Close>(library, "fclose_nid_postfix");
    const auto wide = Symbol<Wide>(library, "fwide_nid_postfix");
    const auto putByte = Symbol<PutByte>(library, "fputc_nid_postfix");
    const auto putWide = Symbol<PutWide>(library, "fputwc_nid_postfix");
    const auto seek = Symbol<Seek>(library, "fseek_nid_postfix");
    const auto read = Symbol<Read>(library, "fread_nid_postfix");
    const auto write = Symbol<Write>(library, "fwrite_nid_postfix");
    const auto flush = Symbol<Flush>(library, "fflush_nid_postfix");
    const auto descriptor = Symbol<Descriptor>(library, "fileno_nid_postfix");
    const auto getByte = Symbol<GetByte>(library, "fgetc_nid_postfix");
    const auto getWide = Symbol<GetWide>(library, "fgetwc_nid_postfix");
    const auto putByteUnlocked = Symbol<PutByte>(library, "putc_unlocked_nid_postfix");
    Require(open && close && wide && putByte && putWide && seek && read && write && flush && descriptor && getByte && getWide && putByteUnlocked,
        "libc is missing a stdio orientation test symbol");

    void* stream = open(argv[2], "w+b");
    Require(stream != nullptr, "could not open guest stream");
    Require(wide(stream, 0) == 0, "a new stream should be unoriented");
    Require(seek(stream, 0, SEEK_SET) == 0, "seeking should succeed");
    Require(flush(stream) == 0, "flushing should succeed");
    Require(descriptor(stream) >= 0, "fileno should succeed");
    Require(read(nullptr, 0, 0, stream) == 0, "zero-length fread should be a no-op");
    Require(write(nullptr, 0, 0, stream) == 0, "zero-length fwrite should be a no-op");
    Require(wide(stream, 0) == 0, "metadata operations must not orient a stream");
    Require(wide(stream, 1) == 1, "positive mode should establish wide orientation");
    Require(wide(stream, -1) == 1, "an established orientation must not change");
    Require(close(stream) == 0, "could not close query-oriented stream");

    stream = open(argv[2], "w+b");
    Require(stream != nullptr, "could not reopen guest stream");
    Require(putByte('B', stream) == 'B', "byte output failed");
    Require(wide(stream, 0) == -1, "byte output should establish byte orientation");
    Require(seek(stream, 0, SEEK_SET) == 0, "could not rewind byte stream");
    Require(getByte(stream) == 'B', "byte stream contents did not round-trip");
    Require(wide(stream, 0) == -1, "byte reads must preserve byte orientation");
    Require(close(stream) == 0, "could not close byte-oriented stream");

    stream = open(argv[2], "w+b");
    Require(stream != nullptr, "could not reopen guest stream");
    Require(putByteUnlocked('U', stream) == 'U', "unlocked byte output failed");
    Require(wide(stream, 0) == -1, "unlocked byte output should establish byte orientation");
    Require(close(stream) == 0, "could not close unlocked byte-oriented stream");

    stream = open(argv[2], "w+b");
    Require(stream != nullptr, "could not reopen guest stream");
    Require(putWide(u'W', stream) == u'W', "wide output failed");
    Require(wide(stream, 0) == 1, "wide output should establish wide orientation");
    Require(close(stream) == 0, "could not close wide-oriented stream");

    {
        std::FILE* host = std::fopen(argv[2], "wb");
        Require(host != nullptr, "could not prepare wide-input fixture");
        Require(std::fputc('I', host) == 'I', "could not write wide-input fixture");
        Require(std::fclose(host) == 0, "could not close wide-input fixture");
    }
    stream = open(argv[2], "rb");
    Require(stream != nullptr, "could not open wide-input fixture");
    Require(getWide(stream) == 'I', "wide input returned the wrong character");
    Require(wide(stream, 0) == 1, "wide input should establish wide orientation");
    Require(close(stream) == 0, "could not close wide-input stream");

    stream = open(argv[2], "w+b");
    Require(stream != nullptr, "could not reopen guest stream");
    Require(putByte('R', stream) == 'R', "byte output before freopen failed");
    Require(wide(stream, 0) == -1, "precondition: byte orientation");
    const auto reopen = Symbol<Reopen>(library, "freopen_nid_postfix");
    Require(reopen != nullptr, "libc is missing freopen");
    stream = reopen(argv[2], "w+b", stream);
    Require(stream != nullptr, "freopen failed");
    Require(wide(stream, 0) == 0, "successful freopen should reset orientation");
    Require(getWide(stream) == -1, "wide read at EOF should fail");
    Require(wide(stream, 0) == 1, "wide input should establish orientation even at EOF");
    Require(close(stream) == 0, "could not close reopened stream");

    const auto ungetWide = Symbol<UngetWide>(library, "ungetwc_nid_postfix");
    const auto getLine = Symbol<GetLine>(library, "fgets_nid_postfix");
    const auto getWideLine = Symbol<GetWideLine>(library, "fgetws_nid_postfix");
    Require(ungetWide && getLine && getWideLine, "libc is missing a line or unget symbol");
    char line[4]{};
    char16_t wideLine[4]{};
    stream = open(argv[2], "w+b");
    Require(stream != nullptr, "could not reopen guest stream");
    Require(ungetWide(-1, stream) == -1, "ungetwc of WEOF should fail");
    Require(wide(stream, 0) == 1, "ungetwc should orient the stream even for WEOF");
    Require(close(stream) == 0, "could not close stream after ungetwc");
    stream = open(argv[2], "w+b");
    Require(stream != nullptr, "could not reopen guest stream");
    Require(getLine(line, 0, stream) == nullptr, "fgets with size 0 should fail");
    Require(wide(stream, 0) == -1, "fgets should orient the stream even for size 0");
    Require(close(stream) == 0, "could not close stream after fgets");
    stream = open(argv[2], "w+b");
    Require(stream != nullptr, "could not reopen guest stream");
    Require(getWideLine(wideLine, 0, stream) == nullptr, "fgetws with size 0 should fail");
    Require(wide(stream, 0) == 1, "fgetws should orient the stream even for size 0");
    Require(close(stream) == 0, "could not close stream after fgetws");

    const auto scanList = Symbol<ScanList>(library, "vfscanf_nid_postfix");
    Require(scanList != nullptr, "libc is missing vfscanf");
    int number = 0;
    stream = open(argv[2], "w+b");
    Require(stream != nullptr, "could not reopen guest stream");
    Require(ScanThrough(scanList, stream, "%d", &number) == EOF, "vfscanf at end of file should fail");
    Require(wide(stream, 0) == -1, "vfscanf should orient the stream to bytes");
    Require(close(stream) == 0, "could not close stream after vfscanf");

    const auto scanSecure = Symbol<ScanList>(library, "vfscanf_s_nid_postfix");
    const auto printSecure = Symbol<ScanList>(library, "vfprintf_s_nid_postfix");
    const auto printWideSecure = Symbol<WideList>(library, "vfwprintf_s_nid_postfix");
    Require(scanSecure && printSecure && printWideSecure, "libc is missing a bounds-checked stdio symbol");
    stream = open(argv[2], "w+b");
    Require(stream != nullptr, "could not reopen guest stream");
    Require(ScanThrough(scanSecure, stream, "%d", &number) == EOF, "vfscanf_s at end of file should fail");
    Require(wide(stream, 0) == -1, "vfscanf_s should orient the stream to bytes");
    Require(close(stream) == 0, "could not close stream after vfscanf_s");
    stream = open(argv[2], "w+b");
    Require(stream != nullptr, "could not reopen guest stream");
    Require(ScanThrough(printSecure, stream, "%d", 7) == 1, "vfprintf_s should write one byte");
    Require(wide(stream, 0) == -1, "vfprintf_s should orient the stream to bytes");
    Require(close(stream) == 0, "could not close stream after vfprintf_s");
    stream = open(argv[2], "w+b");
    Require(stream != nullptr, "could not reopen guest stream");
    Require(WideThrough(printWideSecure, stream, u"%d", 7) == 1, "vfwprintf_s should write one character");
    Require(wide(stream, 0) == 1, "vfwprintf_s should orient the stream wide");
    Require(close(stream) == 0, "could not close stream after vfwprintf_s");

    std::remove(argv[2]);
#ifdef _WIN32
    FreeLibrary(library);
#else
    dlclose(library);
#endif
    return 0;
}
