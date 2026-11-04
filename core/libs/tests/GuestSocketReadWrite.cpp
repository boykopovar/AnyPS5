#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/File/include/File.hpp"
#include "prx/libc/include/Api.hpp"
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
constexpr int KernelIllegalSeek = static_cast<int>(0x8002001du);

static bool Readable(int descriptor, short accepted = In) {
    GuestPollDescriptor waited{descriptor, In, 0};
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

    const char vectorFirst[] = "vector ";
    const char vectorSecond[] = "write";
    KernelIovec vectorWrite[] = {{const_cast<char*>(vectorFirst), sizeof(vectorFirst) - 1},
        {const_cast<char*>(vectorSecond), sizeof(vectorSecond)}};
    Require(sceKernelWritev(client, vectorWrite, 2) == sizeof(vectorFirst) + sizeof(vectorSecond) - 1);
    char vectorReadFirst[sizeof(vectorFirst) - 1]{};
    char vectorReadSecond[sizeof(vectorSecond)]{};
    KernelIovec vectorRead[] = {{vectorReadFirst, sizeof(vectorReadFirst)}, {vectorReadSecond, sizeof(vectorReadSecond)}};
    Require(Readable(server) && sceKernelReadv(server, vectorRead, 2) == sizeof(vectorFirst) + sizeof(vectorSecond) - 1);
    Require(std::memcmp(vectorReadFirst, "vector ", sizeof(vectorReadFirst)) == 0 && std::strcmp(vectorReadSecond, "write") == 0);

    const char sceVectorFirst[] = "sceKernel";
    const char sceVectorSecond[] = "Writev";
    KernelIovec sceVectorWrite[] = {{const_cast<char*>(sceVectorFirst), sizeof(sceVectorFirst) - 1},
        {const_cast<char*>(sceVectorSecond), sizeof(sceVectorSecond)}};
    Require(sceKernelWritev(client, sceVectorWrite, 2) == sizeof(sceVectorFirst) + sizeof(sceVectorSecond) - 1);
    char sceVectorReadFirst[sizeof(sceVectorFirst) - 1]{};
    char sceVectorReadSecond[sizeof(sceVectorSecond)]{};
    KernelIovec sceVectorRead[] = {{sceVectorReadFirst, sizeof(sceVectorReadFirst)}, {sceVectorReadSecond, sizeof(sceVectorReadSecond)}};
    Require(Readable(server) && sceKernelReadv(server, sceVectorRead, 2) == sizeof(sceVectorFirst) + sizeof(sceVectorSecond) - 1);
    Require(std::memcmp(sceVectorReadFirst, "sceKernel", sizeof(sceVectorReadFirst)) == 0 && std::strcmp(sceVectorReadSecond, "Writev") == 0);

    const char positionedWrite[] = "positioned";
    KernelIovec positionedIov[] = {{const_cast<char*>(positionedWrite), sizeof(positionedWrite)}};
    *__error_nid_postfix() = 0;
    Require(pread_nid_postfix(server, received, 1, 0) == -1 && *__error_nid_postfix() == 29);
    Require(pwrite_nid_disambig1_nid_postfix(client, positionedWrite, 1, 0) == -1 && *__error_nid_postfix() == 29);
    Require(preadv_nid_postfix(server, positionedIov, 1, 0) == -1 && *__error_nid_postfix() == 29);
    Require(pwritev_nid_postfix(client, positionedIov, 1, 0) == -1 && *__error_nid_postfix() == 29);
    Require(sceKernelPread(server, received, 1, 0) == KernelIllegalSeek);
    Require(sceKernelPwrite(client, positionedWrite, 1, 0) == KernelIllegalSeek);
    Require(sceKernelPreadv(server, positionedIov, 1, 0) == KernelIllegalSeek);
    Require(sceKernelPwritev(client, positionedIov, 1, 0) == KernelIllegalSeek);

    Require(fcntl_nid_postfix(server, 4, 4) == 0);
    *__error_nid_postfix() = 0;
    Require(read_nid_postfix(server, received, sizeof(received)) == -1 && *__error_nid_postfix() == 35);
    Require(sceKernelRead(server, received, sizeof(received)) == KernelAgain);
    KernelIovec emptyRead[] = {{received, sizeof(received)}};
    Require(sceKernelReadv(server, emptyRead, 1) == KernelAgain);

    Require(read_nid_postfix(server, received, 0) == 0 && sceKernelRead(server, received, 0) == 0);
    Require(close_nid_postfix(client) == 0);
    Require(Readable(server, In | Hup) && read_nid_postfix(server, received, sizeof(received)) == 0);
    Require(sceKernelRead(server, received, sizeof(received)) == 0);
    std::int64_t written = 0;
    for (int i = 0; i < 100 && written >= 0; ++i) written = write_nid_postfix(server, request, sizeof(request));
    Require(written == -1 && *__error_nid_postfix() == 32);
    Require(close_nid_postfix(server) == 0);
    *__error_nid_postfix() = 0;
    Require(read_nid_postfix(server, received, sizeof(received)) == -1 && *__error_nid_postfix() == 9);
    Require(read_nid_postfix(server, received, 0) == -1 && *__error_nid_postfix() == 9);
    Require(write_nid_postfix(server, request, sizeof(request)) == -1 && *__error_nid_postfix() == 9);
    Require(_write_nid_postfix(server, request, sizeof(request)) == -1 && *__error_nid_postfix() == 9);
    Require(_read_nid_postfix(server, received, sizeof(received)) == -1 && *__error_nid_postfix() == 9);
    Require(sceKernelRead(server, received, sizeof(received)) == KernelBadDescriptor);
    Require(sceKernelWrite(server, request, sizeof(request)) == KernelBadDescriptor);
    Require(sceKernelPread(server, received, 1, 0) == KernelBadDescriptor);
    Require(sceKernelPwrite(server, request, 1, 0) == KernelBadDescriptor);
    Require(pread_nid_postfix(server, received, 1, 0) == -1 && *__error_nid_postfix() == 9);
    Require(pwrite_nid_disambig1_nid_postfix(server, request, 1, 0) == -1 && *__error_nid_postfix() == 9);
    Require(preadv_nid_postfix(server, positionedIov, 1, 0) == -1 && *__error_nid_postfix() == 9);
    Require(pwritev_nid_postfix(server, positionedIov, 1, 0) == -1 && *__error_nid_postfix() == 9);
    Require(sceKernelClose(listener) == 0);
    Require(sceKernelClose(listener) == KernelBadDescriptor);
    Require(close_nid_postfix(listener) == -1 && *__error_nid_postfix() == 9);
    return 0;
}
