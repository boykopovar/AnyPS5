#include "prx/libc/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel_sys/include/Api.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>

namespace {

constexpr std::int64_t SceEbadf = static_cast<std::int32_t>(0x80020009u);
constexpr std::int64_t SceEnotty = static_cast<std::int32_t>(0x80020019u);
constexpr std::int64_t SceEaddrnotavail = static_cast<std::int32_t>(0x80020031u);

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "lwfs check failed at line %u\n", location.line());
        std::abort();
    }
}

void ExpectAll(int fd, std::int64_t sceError, int guestErrno, std::source_location location = std::source_location::current()) {
    static const char data[16] = "lwfs";
    *__error_nid_postfix() = 0;
    Require(sceKernelLwfsWrite(fd, data, sizeof(data)) == sceError, location);
    Require(*__error_nid_postfix() == guestErrno, location);
    *__error_nid_postfix() = 0;
    Require(sceKernelLwfsWrite(fd, nullptr, 0) == sceError, location);
    *__error_nid_postfix() = 0;
    Require(sceKernelLwfsLseek(fd, 0, 0) == sceError, location);
    Require(*__error_nid_postfix() == guestErrno, location);
    *__error_nid_postfix() = 0;
    Require(sceKernelLwfsSetAttribute(fd, 0) == sceError, location);
    Require(*__error_nid_postfix() == guestErrno, location);
    *__error_nid_postfix() = 0;
    Require(sceKernelLwfsAllocateBlock(fd, 0) == sceError, location);
    Require(*__error_nid_postfix() == guestErrno, location);
}

} // namespace

int main() {
    ExpectAll(-1, SceEbadf, 9);
    ExpectAll(100000, SceEbadf, 9);

    int pipeFds[2]{-1, -1};
    Require(pipe_nid_postfix(pipeFds) == 0);
    ExpectAll(pipeFds[0], SceEnotty, 25);
    ExpectAll(pipeFds[1], SceEnotty, 25);
    Require(close_nid_postfix(pipeFds[0]) == 0);
    Require(close_nid_postfix(pipeFds[1]) == 0);
    ExpectAll(pipeFds[0], SceEbadf, 9);

    const int sock = socket_nid_postfix(2, 1, 0);
    Require(sock >= 0);
    ExpectAll(sock, SceEaddrnotavail, 49);
    Require(close_nid_postfix(sock) == 0);
    ExpectAll(sock, SceEbadf, 9);

    const int queue = kqueue_nid_postfix();
    Require(queue >= 0);
    ExpectAll(queue, SceEnotty, 25);
    Require(close_nid_postfix(queue) == 0);
    ExpectAll(queue, SceEbadf, 9);
    return 0;
}
