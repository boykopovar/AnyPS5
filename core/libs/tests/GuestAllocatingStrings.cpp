#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "SceTypes.hpp"
#include <array>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
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

void* APS5_VABI Allocate(std::size_t size) { return failAllocation ? nullptr : std::malloc(size); }
void APS5_VABI Free(void* ptr) { std::free(ptr); }
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
    const int result = vdprintf_nid_postfix(descriptor, format, reinterpret_cast<VaList*>(args));
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
    Require(strndup_nid_postfix(nullptr, 0) == nullptr);
    Require(*__error_nid_postfix() == 22);

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

    LibcSetGuestWriteHook_nid_no_patch(origHook);

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
