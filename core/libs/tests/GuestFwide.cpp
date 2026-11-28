#include "prx/libc/include/general/VabiMacros.hpp"
#include "tests/CommandLineArgument.hpp"
#include <cstdarg>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#include <sys/mman.h>
#include <unistd.h>
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
using PutWideString = int (APS5_VABI*)(const char16_t*, void*);
using StringWideList = int (APS5_VABI*)(char16_t*, std::size_t, const char16_t*, void*);
using ErrorPtr = int* (APS5_VABI*)();

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

int APS5_VABI StringWideThrough(StringWideList print, char16_t* buffer, std::size_t size, const char16_t* format, ...) {
#ifdef _WIN32
    __builtin_sysv_va_list args;
    __builtin_sysv_va_start(args, format);
#else
    std::va_list args;
    va_start(args, format);
#endif
    const int result = print(buffer, size, format, reinterpret_cast<void*>(args));
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

class GuardedPage {
public:
    GuardedPage() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        pageSize = info.dwPageSize;
        base = static_cast<unsigned char*>(VirtualAlloc(nullptr, pageSize * 2, MEM_RESERVE | MEM_COMMIT, PAGE_NOACCESS));
        Require(base != nullptr, "VirtualAlloc failed");
        DWORD previous = 0;
        Require(VirtualProtect(base, pageSize, PAGE_READWRITE, &previous) != 0, "VirtualProtect RW failed");
#else
        const long size = sysconf(_SC_PAGESIZE);
        Require(size > 0, "sysconf page size failed");
        pageSize = static_cast<std::size_t>(size);
        base = static_cast<unsigned char*>(mmap(nullptr, pageSize * 2, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
        Require(base != MAP_FAILED, "mmap failed");
        Require(mprotect(base, pageSize, PROT_READ | PROT_WRITE) == 0, "mprotect RW failed");
#endif
        std::memset(base, 0xA5, pageSize);
    }

    ~GuardedPage() {
#ifdef _WIN32
        VirtualFree(base, 0, MEM_RELEASE);
#else
        munmap(base, pageSize * 2);
#endif
    }

    GuardedPage(const GuardedPage&) = delete;
    GuardedPage& operator=(const GuardedPage&) = delete;

    unsigned char* Data() const { return base; }
    std::size_t PageSize() const { return pageSize; }

    void SetReadOnly(bool readOnly) {
#ifdef _WIN32
        DWORD previous = 0;
        Require(VirtualProtect(base, pageSize, readOnly ? PAGE_READONLY : PAGE_READWRITE, &previous) != 0, "VirtualProtect failed");
#else
        Require(mprotect(base, pageSize, readOnly ? PROT_READ : (PROT_READ | PROT_WRITE)) == 0, "mprotect failed");
#endif
    }

private:
    unsigned char* base = nullptr;
    std::size_t pageSize = 0;
};

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
    const auto library = LoadLibraryW(ArgumentPath(argv, 1).c_str());
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
    Require(wide(stream, -1) == -1, "negative mode should establish byte orientation");
    Require(wide(stream, 1) == -1, "an established byte orientation must not change to wide");
    Require(wide(stream, 0) == -1, "querying established byte orientation");
    Require(close(stream) == 0, "could not close byte-oriented stream");

    stream = open(argv[2], "w+b");
    Require(stream != nullptr, "could not reopen guest stream");
    Require(wide(stream, 1000) == 1, "large positive mode should establish wide orientation");
    Require(wide(stream, -5000) == 1, "large negative mode must not change established wide orientation");
    Require(close(stream) == 0, "could not close wide-oriented stream");

    stream = open(argv[2], "w+b");
    Require(stream != nullptr, "could not reopen guest stream");
    Require(wide(stream, -1000) == -1, "large negative mode should establish byte orientation");
    Require(wide(stream, 5000) == -1, "large positive mode must not change established byte orientation");
    Require(close(stream) == 0, "could not close byte-oriented stream");

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
    Require(wide(stream, 0) == 0, "fgets with a size below 2 should leave the stream unoriented");
    Require(close(stream) == 0, "could not close stream after fgets");
    stream = open(argv[2], "w+b");
    Require(stream != nullptr, "could not reopen guest stream");
    Require(getWideLine(wideLine, 0, stream) == nullptr, "fgetws with size 0 should fail");
    Require(wide(stream, 0) == 0, "fgetws with size 0 should leave the stream unoriented, as the PS5 system module does");
    Require(getWideLine(wideLine, 1, stream) == nullptr, "fgetws with size 1 should fail");
    Require(wide(stream, 0) == 0, "fgetws with size 1 should leave the stream unoriented");
    Require(getWideLine(wideLine, 2, stream) == nullptr, "fgetws at end of file should fail");
    Require(wide(stream, 0) == 1, "fgetws with size 2 should orient the stream even at end of file");
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

    const auto putWideString = Symbol<PutWideString>(library, "fputws_nid_postfix");
    const auto printWide = Symbol<WideList>(library, "vfwprintf_nid_postfix");
    const auto formatWideString = Symbol<StringWideList>(library, "vswprintf_nid_postfix");
    const auto guestErrno = Symbol<ErrorPtr>(library, "__error_nid_postfix");
    Require(putWideString && printWide && formatWideString && guestErrno, "libc is missing wide string output symbols");

    const auto* unmappedWide = reinterpret_cast<const char16_t*>(std::uintptr_t{1});
    const auto* unmappedNarrow = reinterpret_cast<const char*>(std::uintptr_t{1});

    GuardedPage guarded;
    auto* tailWide = reinterpret_cast<char16_t*>(guarded.Data() + guarded.PageSize() - 6 * sizeof(char16_t));
    tailWide[0] = u'o';
    tailWide[1] = u'k';
    tailWide[2] = 0;
    tailWide[3] = u'!';
    guarded.SetReadOnly(true);

    stream = open(argv[2], "w+b");
    Require(stream != nullptr, "could not reopen guest stream for fputws");
    *guestErrno() = 0;
    Require(putWideString(nullptr, stream) == -1 && *guestErrno() == 22, "fputws(nullptr) should fail with EINVAL");
    Require(wide(stream, 0) == 0, "fputws(nullptr) should leave stream unoriented");
    *guestErrno() = 0;
    Require(putWideString(unmappedWide, stream) == -1 && *guestErrno() == 22, "fputws(0x1) should fail with EINVAL");
    Require(wide(stream, 0) == 0, "fputws(0x1) should leave stream unoriented");
    Require(putWideString(tailWide, stream) == 2, "fputws on read-only page should succeed");
    Require(wide(stream, 0) == 1, "fputws should orient stream wide");
    Require(close(stream) == 0, "could not close stream after fputws");

    guarded.SetReadOnly(false);
    auto* unterminatedWide = reinterpret_cast<char16_t*>(guarded.Data() + guarded.PageSize() - 2 * sizeof(char16_t));
    unterminatedWide[0] = u'a';
    unterminatedWide[1] = u'b';
    guarded.SetReadOnly(true);

    stream = open(argv[2], "w+b");
    Require(stream != nullptr, "could not reopen guest stream for unterminated fputws");
    *guestErrno() = 0;
    Require(putWideString(unterminatedWide, stream) == -1 && *guestErrno() == 22, "fputws crossing into guard page should fail with EINVAL");
    Require(wide(stream, 0) == 0, "failing fputws should leave stream unoriented");
    Require(close(stream) == 0, "could not close stream after unterminated fputws");

    stream = open(argv[2], "w+b");
    Require(stream != nullptr, "could not reopen guest stream for vfwprintf");
    *guestErrno() = 0;
    Require(WideThrough(printWide, stream, nullptr) == -1 && *guestErrno() == 22, "vfwprintf(nullptr) should fail with EINVAL");
    Require(wide(stream, 0) == 0, "vfwprintf(nullptr) should leave stream unoriented");
    *guestErrno() = 0;
    Require(WideThrough(printWide, stream, unmappedWide) == -1 && *guestErrno() == 22, "vfwprintf(0x1) should fail with EINVAL");
    Require(wide(stream, 0) == 0, "vfwprintf(0x1) should leave stream unoriented");
    *guestErrno() = 0;
    Require(WideThrough(printWide, stream, unterminatedWide) == -1 && *guestErrno() == 22, "vfwprintf crossing into guard page should fail with EINVAL");
    Require(wide(stream, 0) == 0, "failing vfwprintf should leave stream unoriented");
    Require(WideThrough(printWide, stream, tailWide) == 2, "vfwprintf on read-only page should succeed");
    Require(wide(stream, 0) == 1, "vfwprintf should orient stream wide");
    *guestErrno() = 0;
    Require(WideThrough(printWide, stream, u"%ls", unmappedWide) == -1 && *guestErrno() == 22, "vfwprintf(%ls, 0x1) should fail with EINVAL");
    *guestErrno() = 0;
    Require(WideThrough(printWide, stream, u"%s", unmappedNarrow) == -1 && *guestErrno() == 22, "vfwprintf(%s, 0x1) should fail with EINVAL");
    Require(close(stream) == 0, "could not close stream after vfwprintf");

    stream = open(argv[2], "w+b");
    Require(stream != nullptr, "could not reopen guest stream for vfwprintf_s");
    *guestErrno() = 77;
    Require(WideThrough(printWideSecure, stream, nullptr) == -1 && *guestErrno() == 77, "vfwprintf_s(nullptr) should return -1");
    Require(wide(stream, 0) == 0, "vfwprintf_s(nullptr) should leave stream unoriented");
    Require(WideThrough(printWideSecure, stream, unmappedWide) == -1 && *guestErrno() == 77, "vfwprintf_s(0x1) should return -1");
    Require(wide(stream, 0) == 0, "vfwprintf_s(0x1) should leave stream unoriented");
    Require(WideThrough(printWideSecure, stream, unterminatedWide) == -1 && *guestErrno() == 77, "vfwprintf_s crossing into guard page should return -1");
    Require(wide(stream, 0) == 0, "failing vfwprintf_s should leave stream unoriented");
    Require(WideThrough(printWideSecure, stream, tailWide) == 2, "vfwprintf_s on read-only page should succeed");
    Require(wide(stream, 0) == 1, "vfwprintf_s should orient stream wide");
    Require(WideThrough(printWideSecure, stream, u"%ls", unmappedWide) == -1, "vfwprintf_s(%ls, 0x1) should return -1");
    Require(WideThrough(printWideSecure, stream, u"%s", unmappedNarrow) == -1, "vfwprintf_s(%s, 0x1) should return -1");
    Require(close(stream) == 0, "could not close stream after vfwprintf_s");

    guarded.SetReadOnly(false);
    auto* outBuf = reinterpret_cast<char16_t*>(guarded.Data());
    std::memset(outBuf, 0xA5, 32);
    guarded.SetReadOnly(true);
    *guestErrno() = 0;
    Require(StringWideThrough(formatWideString, outBuf, 8, u"hi") == -1 && *guestErrno() == 22, "vswprintf to read-only buffer should fail with EINVAL");
    for (std::size_t i = 0; i < 32; ++i) {
        Require(guarded.Data()[i] == 0xA5, "read-only output buffer sentinel was modified");
    }
    guarded.SetReadOnly(false);
    std::memset(outBuf, 0xA5, 32);
    *guestErrno() = 0;
    Require(StringWideThrough(formatWideString, outBuf, 8, unmappedWide) == -1 && *guestErrno() == 22, "vswprintf with invalid format should fail with EINVAL");
    for (std::size_t i = sizeof(char16_t); i < 32; ++i) {
        Require(guarded.Data()[i] == 0xA5, "vswprintf error touched bytes past terminator");
    }

    guarded.SetReadOnly(false);
    auto* pageEnd = guarded.Data() + guarded.PageSize();
    std::memcpy(pageEnd - 3, "abc", 3);
    stream = open(argv[2], "w+b");
    Require(stream != nullptr, "could not open stream for N1");
    Require(WideThrough(printWide, stream, u"%.3s", reinterpret_cast<const char*>(pageEnd - 3)) == 3, "N1 %.3s at page end");
    Require(close(stream) == 0, "close N1");

    std::memcpy(pageEnd - 4, "abcd", 4);
    stream = open(argv[2], "w+b");
    Require(stream != nullptr, "could not open stream for N2");
    Require(WideThrough(printWide, stream, u"%.3s", reinterpret_cast<const char*>(pageEnd - 4)) == 3, "N2 %.3s at page end");
    Require(close(stream) == 0, "close N2");

    std::memcpy(pageEnd - 3, "ab\0", 3);
    stream = open(argv[2], "w+b");
    Require(stream != nullptr, "could not open stream for N3");
    Require(WideThrough(printWide, stream, u"%s", reinterpret_cast<const char*>(pageEnd - 3)) == 2, "N3 %s at page end");
    Require(close(stream) == 0, "close N3");

    const char16_t wideEdge[2] = {u'x', u'y'};
    std::memcpy(pageEnd - sizeof(wideEdge), wideEdge, sizeof(wideEdge));
    stream = open(argv[2], "w+b");
    Require(stream != nullptr, "could not open stream for W1");
    Require(WideThrough(printWide, stream, u"%.2ls", reinterpret_cast<const char16_t*>(pageEnd - sizeof(wideEdge))) == 2, "W1 %.2ls at page end");
    Require(close(stream) == 0, "close W1");

    std::remove(argv[2]);
#ifdef _WIN32
    FreeLibrary(library);
#else
    dlclose(library);
#endif
    return 0;
}
