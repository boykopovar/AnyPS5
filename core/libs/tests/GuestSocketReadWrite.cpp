#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#ifndef _WIN32
#include <csignal>
#endif

struct PollDescriptor {
    int descriptor;
    short events;
    short revents;
};

extern "C" {
int APS5_VABI socket_nid_postfix(int, int, int);
int APS5_VABI bind_nid_postfix(int, const void*, std::uint32_t);
int APS5_VABI listen_nid_postfix(int, int);
int APS5_VABI getsockname_nid_postfix(int, void*, std::uint32_t*);
int APS5_VABI connect_nid_postfix(int, const void*, std::uint32_t);
int APS5_VABI accept_nid_postfix(int, void*, std::uint32_t*);
int APS5_VABI fcntl_nid_postfix(int, int, ...);
int APS5_VABI poll_nid_postfix(PollDescriptor*, std::uint32_t, int);
int APS5_VABI close_nid_postfix(int);
std::int64_t APS5_VABI read_nid_postfix(int, void*, std::uint64_t);
std::int64_t APS5_VABI write_nid_postfix(int, const char*, std::int64_t);
std::int64_t APS5_VABI _read_nid_postfix(int, void*, std::size_t);
std::int64_t APS5_VABI _write_nid_postfix(int, const void*, std::size_t);
std::int64_t APS5_VABI sceKernelRead(int, void*, std::size_t);
std::int64_t APS5_VABI sceKernelWrite(int, const void*, std::size_t);
int* APS5_VABI __error_nid_postfix();
}

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "socket read/write check failed at line %d, errno %d\n", line, *__error_nid_postfix());
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

constexpr short In = 0x1;
constexpr short Hup = 0x10;
constexpr int KernelAgain = static_cast<int>(0x80020023u);
constexpr int KernelBadDescriptor = static_cast<int>(0x80020009u);

static bool Readable(int descriptor, short accepted = In) {
    PollDescriptor waited{descriptor, In, 0};
    return poll_nid_postfix(&waited, 1, 2000) == 1 && (waited.revents & accepted) != 0;
}

int main() {
#ifndef _WIN32
    std::signal(SIGPIPE, SIG_IGN);
#endif
    const int listener = socket_nid_postfix(2, 1, 0);
    Require(listener >= 0);
    std::array<std::uint8_t, 16> address{16, 2, 0, 0, 127, 0, 0, 1};
    Require(bind_nid_postfix(listener, address.data(), address.size()) == 0);
    Require(listen_nid_postfix(listener, 4) == 0);
    std::uint32_t addressSize = address.size();
    Require(getsockname_nid_postfix(listener, address.data(), &addressSize) == 0);
    const int client = socket_nid_postfix(2, 1, 0);
    Require(client >= 0 && connect_nid_postfix(client, address.data(), address.size()) == 0);
    const int server = accept_nid_postfix(listener, nullptr, nullptr);
    Require(server >= 0);

    const char request[] = "written to a socket";
    char received[32]{};
    Require(write_nid_postfix(client, request, sizeof(request)) == sizeof(request));
    Require(Readable(server) && read_nid_postfix(server, received, sizeof(received)) == sizeof(request));
    Require(std::strcmp(received, request) == 0);

    const char reply[] = "and back";
    std::memset(received, 0, sizeof(received));
    Require(_write_nid_postfix(server, reply, sizeof(reply)) == sizeof(reply));
    Require(Readable(client) && _read_nid_postfix(client, received, sizeof(received)) == sizeof(reply));
    Require(std::strcmp(received, reply) == 0);

    const char kernel[] = "through sceKernelWrite";
    std::memset(received, 0, sizeof(received));
    Require(sceKernelWrite(client, kernel, sizeof(kernel)) == sizeof(kernel));
    Require(Readable(server) && sceKernelRead(server, received, sizeof(received)) == sizeof(kernel));
    Require(std::strcmp(received, kernel) == 0);

    Require(fcntl_nid_postfix(server, 4, 4) == 0);
    *__error_nid_postfix() = 0;
    Require(read_nid_postfix(server, received, sizeof(received)) == -1 && *__error_nid_postfix() == 35);
    Require(sceKernelRead(server, received, sizeof(received)) == KernelAgain);

    Require(read_nid_postfix(server, received, 0) == 0 && sceKernelRead(server, received, 0) == 0);
    Require(close_nid_postfix(client) == 0);
    Require(Readable(server, In | Hup) && read_nid_postfix(server, received, sizeof(received)) == 0);
    Require(sceKernelRead(server, received, sizeof(received)) == 0);
#ifndef _WIN32
    std::int64_t written = 0;
    for (int i = 0; i < 100 && written >= 0; ++i) written = write_nid_postfix(server, request, sizeof(request));
    Require(written == -1 && *__error_nid_postfix() == 32);
#endif
    Require(close_nid_postfix(server) == 0);
    *__error_nid_postfix() = 0;
    Require(read_nid_postfix(server, received, sizeof(received)) == -1 && *__error_nid_postfix() == 9);
    Require(read_nid_postfix(server, received, 0) == -1 && *__error_nid_postfix() == 9);
    Require(write_nid_postfix(server, request, sizeof(request)) == -1 && *__error_nid_postfix() == 9);
    Require(_write_nid_postfix(server, request, sizeof(request)) == -1 && *__error_nid_postfix() == 9);
    Require(_read_nid_postfix(server, received, sizeof(received)) == -1 && *__error_nid_postfix() == 9);
    Require(sceKernelRead(server, received, sizeof(received)) == KernelBadDescriptor);
    Require(sceKernelWrite(server, request, sizeof(request)) == KernelBadDescriptor);
    Require(close_nid_postfix(listener) == 0);
    return 0;
}
