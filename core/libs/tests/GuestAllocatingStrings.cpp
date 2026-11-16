#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "SceTypes.hpp"
#include <array>
#include <cerrno>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

namespace {

void Require(bool value) {
    if (!value) std::abort();
}

bool failAllocation = false;
std::size_t allocations = 0;
std::size_t releases = 0;
std::uintptr_t lastAllocated = 0;
std::uintptr_t lastReleased = 0;
bool clobberFreeErrno = false;
int writeCalls = 0;
int failureCall = 0;
struct WriteFailure {};

void* APS5_VABI Allocate(std::size_t size) {
    void* pointer = failAllocation ? nullptr : std::malloc(size);
    if (pointer) {
        ++allocations;
        lastAllocated = reinterpret_cast<std::uintptr_t>(pointer);
    }
    return pointer;
}
void APS5_VABI Free(void* ptr) {
    if (ptr) {
        ++releases;
        lastReleased = reinterpret_cast<std::uintptr_t>(ptr);
    }
    std::free(ptr);
    if (clobberFreeErrno) *__error_nid_postfix() = 77;
}

std::int64_t APS5_VABI ThrowingWrite(int, const void*, std::size_t count) {
    if (++writeCalls == failureCall) throw WriteFailure{};
    return count > 5 ? 5 : static_cast<std::int64_t>(count);
}
void* APS5_VABI Reallocate(void* ptr, std::size_t size) { return std::realloc(ptr, size); }
void* APS5_VABI Calloc(std::size_t count, std::size_t size) { return std::calloc(count, size); }
void* APS5_VABI Align(std::size_t, std::size_t size) { return std::malloc(size); }
void* APS5_VABI Realign(void* ptr, std::size_t size, std::size_t) { return std::realloc(ptr, size); }
int APS5_VABI PosixAlign(void** ptr, std::size_t, std::size_t size) {
    *ptr = std::malloc(size);
    return *ptr ? 0 : 12;
}

void InitializeHeap() {
    alignas(16) static std::array<std::byte, 0x40> process{};
    alignas(16) static std::array<std::byte, 0x38> libc{};
    alignas(16) static std::array<std::byte, 0x78> replacement{};
    const auto write = [](auto& data, std::size_t offset, auto value) {
        std::memcpy(data.data() + offset, &value, sizeof(value));
    };
    write(process, 0, std::uint64_t{0x40});
    write(process, 8, std::uint32_t{0x4942524f});
    write(process, 0x38, libc.data());
    write(libc, 0, std::uint64_t{0x38});
    write(libc, 0x30, replacement.data());
    write(replacement, 0, std::uint64_t{0x78});
    write(replacement, 8, std::uint64_t{2});
    write(replacement, 0x20, reinterpret_cast<void*>(&Allocate));
    write(replacement, 0x28, reinterpret_cast<void*>(&Free));
    write(replacement, 0x30, reinterpret_cast<void*>(&Calloc));
    write(replacement, 0x38, reinterpret_cast<void*>(&Reallocate));
    write(replacement, 0x40, reinterpret_cast<void*>(&Align));
    write(replacement, 0x48, reinterpret_cast<void*>(&Realign));
    write(replacement, 0x50, reinterpret_cast<void*>(&PosixAlign));
    ApplicationHeapInitialize_nid_no_patch(process.data());
}

static int APS5_VABI CallVasprintf(char** destination, const char* format, ...) {
#ifdef _WIN32
    __builtin_sysv_va_list args;
    __builtin_sysv_va_start(args, format);
#else
    std::va_list args;
    va_start(args, format);
#endif
    const int result = vasprintf_nid_postfix(destination, format, reinterpret_cast<VaList*>(args));
#ifdef _WIN32
    __builtin_sysv_va_end(args);
#else
    va_end(args);
#endif
    return result;
}

static int APS5_VABI CallVdprintf(int descriptor, const char* format, ...) {
#ifdef _WIN32
    __builtin_sysv_va_list args;
    __builtin_sysv_va_start(args, format);
#else
    std::va_list args;
    va_start(args, format);
#endif
    int result;
    try {
        result = vdprintf_nid_postfix(descriptor, format, reinterpret_cast<VaList*>(args));
    } catch (...) {
#ifdef _WIN32
        __builtin_sysv_va_end(args);
#else
        va_end(args);
#endif
        throw;
    }
#ifdef _WIN32
    __builtin_sysv_va_end(args);
#else
    va_end(args);
#endif
    return result;
}

}

int main() {
    *__error_nid_postfix() = 0;
    Require(strndup_nid_postfix(nullptr, 32) == nullptr);
    Require(*__error_nid_postfix() == 22);

    *__error_nid_postfix() = 0;
    Require(strdup_nid_postfix(nullptr) == nullptr);
    Require(*__error_nid_postfix() == 22);

    *__error_nid_postfix() = 0;
    Require(vasprintf_nid_postfix(nullptr, "format", nullptr) == -1);
    Require(*__error_nid_postfix() == 22);

    char* allocated = nullptr;
    *__error_nid_postfix() = 0;
    Require(CallVasprintf(&allocated, nullptr) == -1);
    Require(*__error_nid_postfix() == 22);
    Require(allocated == nullptr);

    *__error_nid_postfix() = 0;
    Require(vasprintf_nid_postfix(&allocated, "format", nullptr) == -1);
    Require(*__error_nid_postfix() == 22);
    Require(allocated == nullptr);

    InitializeHeap();

    *__error_nid_postfix() = 0;
    char* empty = strndup_nid_postfix(nullptr, 0);
    Require(empty != nullptr);
    Require(empty[0] == '\0');
    Require(*__error_nid_postfix() == 0);
    free_nid_postfix(empty);

    char* dup1 = strndup_nid_postfix("hello world", 5);
    Require(dup1 != nullptr);
    Require(std::strcmp(dup1, "hello") == 0);
    free_nid_postfix(dup1);

    char* dup2 = strdup_nid_postfix("testing 1 2 3");
    Require(dup2 != nullptr);
    Require(std::strcmp(dup2, "testing 1 2 3") == 0);
    free_nid_postfix(dup2);

    allocated = nullptr;
    const int count = asprintf_nid_postfix(&allocated, "value=%d %s", 42, "ok");
    Require(count == 11);
    Require(allocated != nullptr);
    Require(std::strcmp(allocated, "value=42 ok") == 0);
    free_nid_postfix(allocated);

    *__error_nid_postfix() = 0;
    Require(dprintf_nid_postfix(-1, "test") == -1);
    Require(*__error_nid_postfix() == 9);

    *__error_nid_postfix() = 0;
    Require(CallVdprintf(-1, "test") == -1);
    Require(*__error_nid_postfix() == 9);

    *__error_nid_postfix() = 0;
    Require(dprintf_nid_postfix(1, nullptr) == -1);
    Require(*__error_nid_postfix() == 22);

    *__error_nid_postfix() = 0;
    Require(CallVdprintf(1, nullptr) == -1);
    Require(*__error_nid_postfix() == 22);

    const char tempFile[] = "guest_allocating_strings_dprintf.tmp";
#ifdef _WIN32
    const int fd = ::_open(tempFile, _O_CREAT | _O_TRUNC | _O_RDWR | _O_BINARY, 0600);
#else
    const int fd = ::open(tempFile, O_CREAT | O_TRUNC | O_RDWR, 0600);
#endif
    Require(fd >= 0);

    Require(dprintf_nid_postfix(fd, "") == 0);

    const int dwritten = dprintf_nid_postfix(fd, "hello %d %s\n", 123, "world");
    Require(dwritten == 16);

    const int vdwritten = CallVdprintf(fd, "pi=%.2f\n", 3.14159);
    Require(vdwritten == 8);

#ifdef _WIN32
    Require(::_lseek(fd, 0, SEEK_SET) == 0);
    char readBuffer[64]{};
    const int readBytes = ::_read(fd, readBuffer, sizeof(readBuffer));
    Require(readBytes == 24);
    ::_close(fd);
    ::_unlink(tempFile);
#else
    Require(::lseek(fd, 0, SEEK_SET) == 0);
    char readBuffer[64]{};
    const int readBytes = static_cast<int>(::read(fd, readBuffer, sizeof(readBuffer)));
    Require(readBytes == 24);
    ::close(fd);
    ::unlink(tempFile);
#endif
    Require(std::memcmp(readBuffer, "hello 123 world\npi=3.14\n", 24) == 0);

    *__error_nid_postfix() = 0;
    Require(dprintf_nid_postfix(fd, "closed") == -1);
    Require(*__error_nid_postfix() == 9);

    *__error_nid_postfix() = 0;
    Require(dprintf_nid_postfix(99999, "invalid") == -1);
    Require(*__error_nid_postfix() == 24);

    *__error_nid_postfix() = 0;
    Require(dprintf_nid_postfix(32767, "invalid") == -1);
    Require(*__error_nid_postfix() == 9);

    Require(dprintf_nid_postfix(1, "") == 0);
    Require(dprintf_nid_postfix(2, "") == 0);

    const char guestFile[] = "guest_allocating_strings_guest.tmp";
    const int guestFd = open_nid_postfix(guestFile, 0x202, 0644);
    Require(guestFd >= 0);
    Require(dprintf_nid_postfix(guestFd, "guest %d %s\n", 99, "ok") == 12);
    close_nid_postfix(guestFd);
    const int readFd = open_nid_postfix(guestFile, 0, 0);
    Require(readFd >= 0);
    char guestReadBuf[32]{};
    Require(read_nid_postfix(readFd, guestReadBuf, sizeof(guestReadBuf)) == 12);
    Require(std::strcmp(guestReadBuf, "guest 99 ok\n") == 0);
    close_nid_postfix(readFd);
    unlink_nid_postfix(guestFile);

    int pair[2] = {-1, -1};
    Require(socketpair_nid_postfix(1, 1, 0, pair) == 0);
    Require(dprintf_nid_postfix(pair[0], "socket %d %s\n", 777, "peer") == 16);
    char sockReadBuf[32]{};
    Require(read_nid_postfix(pair[1], sockReadBuf, sizeof(sockReadBuf)) == 16);
    Require(std::strcmp(sockReadBuf, "socket 777 peer\n") == 0);
    close_nid_postfix(pair[0]);
    close_nid_postfix(pair[1]);

    static int shortState = 0;
    const auto origHook = LibcGetGuestWriteHook_nid_no_patch();

    LibcSetGuestWriteHook_nid_no_patch(+[](int, const void*, std::size_t count) APS5_VABI -> std::int64_t {
        if (shortState++ == 0 && count > 5) return 5;
        return static_cast<std::int64_t>(count);
    });
    shortState = 0;
    Require(dprintf_nid_postfix(1, "abcdefghijk") == 11);
    Require(shortState == 2);

    LibcSetGuestWriteHook_nid_no_patch(+[](int, const void*, std::size_t count) APS5_VABI -> std::int64_t {
        if (shortState++ == 0 && count > 5) return 5;
        errno = 5;
        return -1;
    });
    shortState = 0;
    *__error_nid_postfix() = 0;
    Require(dprintf_nid_postfix(1, "abcdefghijk") == -1);
    Require(*__error_nid_postfix() == 5);

    LibcSetGuestWriteHook_nid_no_patch(+[](int, const void*, std::size_t) APS5_VABI -> std::int64_t {
        errno = 35;
        return -1;
    });
    *__error_nid_postfix() = 0;
    Require(dprintf_nid_postfix(1, "nonblocking") == -1);
    Require(*__error_nid_postfix() == 35);
    *__error_nid_postfix() = 0;
    Require(CallVdprintf(1, "nonblocking %s", "va") == -1);
    Require(*__error_nid_postfix() == 35);

    LibcSetGuestWriteHook_nid_no_patch(+[](int, const void*, std::size_t) APS5_VABI -> std::int64_t {
        errno = 11;
        return -1;
    });
    *__error_nid_postfix() = 0;
    Require(dprintf_nid_postfix(1, "deadlock") == -1);
    Require(*__error_nid_postfix() == 11);
    *__error_nid_postfix() = 0;
    Require(CallVdprintf(1, "deadlock %s", "va") == -1);
    Require(*__error_nid_postfix() == 11);

    for (const bool useVaList : {false, true}) {
        LibcSetGuestWriteHook_nid_no_patch(ThrowingWrite);
        for (const int failAt : {1, 2}) {
            failureCall = failAt;
            writeCalls = 0;
            const auto allocatedBefore = allocations;
            const auto releasedBefore = releases;
            bool caught = false;
            try {
                if (useVaList) CallVdprintf(1, "abcdefghijk");
                else dprintf_nid_postfix(1, "abcdefghijk");
            } catch (const WriteFailure&) {
                caught = true;
            }
            if (!caught || writeCalls != failAt || allocations != allocatedBefore + 1 || releases != releasedBefore + 1 || lastReleased != lastAllocated) {
                std::fprintf(stderr, "vdprintf cleanup failed: va=%d write=%d caught=%d allocations=%zu releases=%zu\n",
                    useVaList, failAt, caught, allocations - allocatedBefore, releases - releasedBefore);
                Require(false);
            }
        }
        LibcSetGuestWriteHook_nid_no_patch(+[](int, const void*, std::size_t) APS5_VABI -> std::int64_t {
            *__error_nid_postfix() = 35;
            return -1;
        });
        const auto allocatedBefore = allocations;
        const auto releasedBefore = releases;
        clobberFreeErrno = true;
        const int error = useVaList ? CallVdprintf(1, "nonblocking") : dprintf_nid_postfix(1, "nonblocking");
        clobberFreeErrno = false;
        Require(error == -1 && *__error_nid_postfix() == 35);
        Require(allocations == allocatedBefore + 1 && releases == releasedBefore + 1 && lastReleased == lastAllocated);
    }
    LibcSetGuestWriteHook_nid_no_patch(origHook);

#ifndef _WIN32
    LibcSetGuestWriteHook_nid_no_patch(nullptr);
    Require(LibcGetGuestWriteHook_nid_no_patch() == nullptr);
    int pipeFds[2] = {-1, -1};
    Require(::pipe(pipeFds) == 0);
    const int readFlags = ::fcntl(pipeFds[0], F_GETFL, 0);
    Require(readFlags >= 0);
    Require(::fcntl(pipeFds[0], F_SETFL, readFlags | O_NONBLOCK) == 0);
    const int writeFlags = ::fcntl(pipeFds[1], F_GETFL, 0);
    Require(writeFlags >= 0);
    Require(::fcntl(pipeFds[1], F_SETFL, writeFlags | O_NONBLOCK) == 0);

    *__error_nid_postfix() = 35;
    Require(dprintf_nid_postfix(pipeFds[1], "a") == 1);
    Require(*__error_nid_postfix() == 35);
    char singleByte = 0;
    Require(::read(pipeFds[0], &singleByte, 1) == 1);
    Require(singleByte == 'a');

    *__error_nid_postfix() = 0;
    Require(dprintf_nid_postfix(pipeFds[1], "") == 0);
    Require(*__error_nid_postfix() == 0);
    Require(CallVdprintf(pipeFds[1], "") == 0);
    Require(*__error_nid_postfix() == 0);

    *__error_nid_postfix() = 0;
    Require(CallVdprintf(pipeFds[1], "bc%d", 42) == 4);
    Require(*__error_nid_postfix() == 0);
    char freeReadBuf[8]{};
    Require(::read(pipeFds[0], freeReadBuf, 4) == 4);
    Require(std::memcmp(freeReadBuf, "bc42", 4) == 0);

    char fillChunk[4096]{};
    std::memset(fillChunk, 'z', sizeof(fillChunk));
    std::size_t totalFilled = 0;
    bool pipeFull = false;
    for (int i = 0; i < 512; ++i) {
        const auto written = ::write(pipeFds[1], fillChunk, sizeof(fillChunk));
        if (written > 0) {
            totalFilled += static_cast<std::size_t>(written);
        } else if (written < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            pipeFull = true;
            break;
        } else if (written < 0 && errno == EINTR) {
            continue;
        } else {
            Require(false);
        }
    }
    Require(pipeFull);

    *__error_nid_postfix() = 0;
    Require(dprintf_nid_postfix(pipeFds[1], "full direct") == -1);
    Require(*__error_nid_postfix() == 35);

    *__error_nid_postfix() = 0;
    Require(CallVdprintf(pipeFds[1], "full %s %d", "valist", 999) == -1);
    Require(*__error_nid_postfix() == 35);

#ifdef __linux__
    const auto pageSize = ::sysconf(_SC_PAGESIZE);
    Require(pageSize > 0);
    const auto pageBytes = static_cast<std::size_t>(pageSize);
    Require(totalFilled >= pageBytes);
    std::vector<char> drainPage(pageBytes);
    std::size_t drained = 0;
    while (drained < drainPage.size()) {
        const auto r = ::read(pipeFds[0], drainPage.data() + drained, drainPage.size() - drained);
        if (r > 0) {
            drained += static_cast<std::size_t>(r);
        } else if (r < 0 && errno == EINTR) {
            continue;
        } else {
            Require(false);
        }
    }
    Require(drained == pageBytes);

    std::vector<char> multiPage(pageBytes * 3 + 1, 'k');
    multiPage.back() = '\0';

    *__error_nid_postfix() = 0;
    Require(dprintf_nid_postfix(pipeFds[1], "%s", multiPage.data()) == -1);
    Require(*__error_nid_postfix() == 35);

    std::size_t remainingFillToDrain = totalFilled - pageBytes;
    while (remainingFillToDrain > 0) {
        char discard[4096]{};
        const std::size_t toRead = remainingFillToDrain < sizeof(discard) ? remainingFillToDrain : sizeof(discard);
        const auto r = ::read(pipeFds[0], discard, toRead);
        if (r > 0) {
            remainingFillToDrain -= static_cast<std::size_t>(r);
        } else if (r < 0 && errno == EINTR) {
            continue;
        } else {
            Require(false);
        }
    }

    std::size_t transferredBytes = 0;
    std::vector<char> verifyBuf(pageBytes);
    while (transferredBytes < verifyBuf.size()) {
        const auto r = ::read(pipeFds[0], verifyBuf.data() + transferredBytes, verifyBuf.size() - transferredBytes);
        if (r > 0) {
            transferredBytes += static_cast<std::size_t>(r);
        } else if (r < 0 && errno == EINTR) {
            continue;
        } else {
            Require(false);
        }
    }
    Require(transferredBytes == pageBytes);
    for (std::size_t i = 0; i < verifyBuf.size(); ++i) {
        Require(verifyBuf[i] == 'k');
    }

    char emptyProbe = 0;
    const auto probeResult = ::read(pipeFds[0], &emptyProbe, 1);
    Require(probeResult < 0 && (errno == EAGAIN || errno == EWOULDBLOCK));
#endif

    Require(::close(pipeFds[0]) == 0);
    Require(::close(pipeFds[1]) == 0);
    LibcSetGuestWriteHook_nid_no_patch(origHook);
#endif

    failAllocation = true;
    allocated = nullptr;
    *__error_nid_postfix() = 0;
    Require(asprintf_nid_postfix(&allocated, "value=%d", 1) == -1);
    Require(*__error_nid_postfix() == 12);
    Require(allocated == nullptr);
    failAllocation = false;

    failAllocation = true;
    *__error_nid_postfix() = 0;
    Require(dprintf_nid_postfix(1, "fail %d", 1) == -1);
    Require(*__error_nid_postfix() == 12);
    failAllocation = false;

    return 0;
}
