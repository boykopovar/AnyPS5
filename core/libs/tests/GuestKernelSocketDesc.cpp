#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {

constexpr int ErrorBadDescriptor = 9;
constexpr int ErrorNotSocket = 38;

constexpr int AfInet = 2;
constexpr int SockStream = 1;
constexpr int SockDgram = 2;

constexpr int SocketDescriptorLimit = 1024;

void Require(bool condition) {
    if (!condition) {
        std::fprintf(stderr, "assertion failed, errno %d\n", *__error_nid_postfix());
        std::abort();
    }
}

bool Fails(long result, int error) {
    return result == -1 && *__error_nid_postfix() == error;
}

void AuditInvalidSocketDescriptors() {
    std::array<std::uint8_t, 16> address{16, AfInet, 0, 0, 127, 0, 0, 1};
    std::uint32_t addressSize = address.size();
    char buffer[16]{};
    int optionValue = 1;
    std::uint32_t optionLength = sizeof(optionValue);

    const int invalidDescriptors[] = {-1000, -100, -2, -1, 1024, 1025, 4096, 65535, 100000};
    for (const int fd : invalidDescriptors) {
        Require(Fails(getsockname_nid_postfix(fd, address.data(), &addressSize), ErrorBadDescriptor));
        Require(Fails(getpeername_nid_postfix(fd, address.data(), &addressSize), ErrorBadDescriptor));
        Require(Fails(shutdown_nid_postfix(fd, 0), ErrorBadDescriptor));
        Require(Fails(shutdown_nid_postfix(fd, 1), ErrorBadDescriptor));
        Require(Fails(shutdown_nid_postfix(fd, 2), ErrorBadDescriptor));
        Require(Fails(getsockopt_nid_postfix(fd, 0xffff, 4, &optionValue, &optionLength), ErrorBadDescriptor));
        Require(Fails(setsockopt_nid_postfix(fd, 0xffff, 4, &optionValue, sizeof(optionValue)), ErrorBadDescriptor));
        Require(Fails(send_nid_postfix(fd, "x", 1, 0), ErrorBadDescriptor));
        Require(Fails(recv_nid_postfix(fd, buffer, sizeof(buffer), 0), ErrorBadDescriptor));
        Require(Fails(bind_nid_postfix(fd, address.data(), address.size()), ErrorBadDescriptor));
        Require(Fails(listen_nid_postfix(fd, 1), ErrorBadDescriptor));
        Require(Fails(accept_nid_postfix(fd, nullptr, nullptr), ErrorBadDescriptor));
        Require(Fails(connect_nid_postfix(fd, address.data(), address.size()), ErrorBadDescriptor));
        Require(Fails(sendto_nid_postfix(fd, "x", 1, 0, nullptr, 0), ErrorBadDescriptor));
        Require(Fails(recvfrom_nid_postfix(fd, buffer, sizeof(buffer), 0, nullptr, nullptr), ErrorBadDescriptor));
        Require(Fails(close_nid_postfix(fd), ErrorBadDescriptor));
    }
}

void AuditErrorPrecedenceOnInvalidDescriptors() {
    Require(Fails(shutdown_nid_postfix(-1, -1), ErrorBadDescriptor));
    Require(Fails(shutdown_nid_postfix(-1, 3), ErrorBadDescriptor));
    Require(Fails(shutdown_nid_postfix(-100, 999), ErrorBadDescriptor));
    Require(Fails(shutdown_nid_postfix(1024, -1), ErrorBadDescriptor));
    Require(Fails(shutdown_nid_postfix(1024, 3), ErrorBadDescriptor));
    Require(Fails(shutdown_nid_postfix(65535, 999), ErrorBadDescriptor));

    int optionValue = 1;
    std::uint32_t optionLength = sizeof(optionValue);
    Require(Fails(setsockopt_nid_postfix(-1, 9999, 9999, &optionValue, sizeof(optionValue)), ErrorBadDescriptor));
    Require(Fails(setsockopt_nid_postfix(1024, 9999, 9999, &optionValue, sizeof(optionValue)), ErrorBadDescriptor));
    Require(Fails(setsockopt_nid_postfix(65535, 9999, 9999, &optionValue, sizeof(optionValue)), ErrorBadDescriptor));
    Require(Fails(getsockopt_nid_postfix(-1, 9999, 9999, &optionValue, &optionLength), ErrorBadDescriptor));
    Require(Fails(getsockopt_nid_postfix(1024, 9999, 9999, &optionValue, &optionLength), ErrorBadDescriptor));
    Require(Fails(getsockopt_nid_postfix(65535, 9999, 9999, &optionValue, &optionLength), ErrorBadDescriptor));
}

void AuditNonSocketDescriptors() {
    int ends[2]{-1, -1};
    Require(pipe_nid_postfix(ends) == 0);
    Require(ends[0] >= 0 && ends[0] < SocketDescriptorLimit);
    Require(ends[1] >= 0 && ends[1] < SocketDescriptorLimit);

    std::array<std::uint8_t, 16> address{16, AfInet, 0, 0, 127, 0, 0, 1};
    std::uint32_t addressSize = address.size();
    char buffer[16]{};
    int optionValue = 0;
    std::uint32_t optionLength = sizeof(optionValue);

    for (const int fd : {ends[0], ends[1]}) {
        Require(Fails(getsockname_nid_postfix(fd, address.data(), &addressSize), ErrorNotSocket));
        Require(Fails(getpeername_nid_postfix(fd, address.data(), &addressSize), ErrorNotSocket));
        Require(Fails(shutdown_nid_postfix(fd, 0), ErrorNotSocket));
        Require(Fails(shutdown_nid_postfix(fd, 1), ErrorNotSocket));
        Require(Fails(shutdown_nid_postfix(fd, 2), ErrorNotSocket));
        Require(Fails(shutdown_nid_postfix(fd, -1), ErrorNotSocket));
        Require(Fails(shutdown_nid_postfix(fd, 3), ErrorNotSocket));
        Require(Fails(getsockopt_nid_postfix(fd, 0xffff, 4, &optionValue, &optionLength), ErrorNotSocket));
        Require(Fails(setsockopt_nid_postfix(fd, 0xffff, 4, &optionValue, sizeof(optionValue)), ErrorNotSocket));
        Require(Fails(send_nid_postfix(fd, "x", 1, 0), ErrorNotSocket));
        Require(Fails(recv_nid_postfix(fd, buffer, sizeof(buffer), 0), ErrorNotSocket));
        Require(Fails(bind_nid_postfix(fd, address.data(), address.size()), ErrorNotSocket));
        Require(Fails(listen_nid_postfix(fd, 1), ErrorNotSocket));
        Require(Fails(accept_nid_postfix(fd, nullptr, nullptr), ErrorNotSocket));
        Require(Fails(connect_nid_postfix(fd, address.data(), address.size()), ErrorNotSocket));
        Require(Fails(sendto_nid_postfix(fd, "x", 1, 0, nullptr, 0), ErrorNotSocket));
        Require(Fails(recvfrom_nid_postfix(fd, buffer, sizeof(buffer), 0, nullptr, nullptr), ErrorNotSocket));
    }

    Require(close_nid_postfix(ends[0]) == 0);
    Require(close_nid_postfix(ends[1]) == 0);

    for (const int fd : {ends[0], ends[1]}) {
        Require(Fails(close_nid_postfix(fd), ErrorBadDescriptor));
        Require(Fails(getsockname_nid_postfix(fd, address.data(), &addressSize), ErrorBadDescriptor));
        Require(Fails(getpeername_nid_postfix(fd, address.data(), &addressSize), ErrorBadDescriptor));
        Require(Fails(shutdown_nid_postfix(fd, 0), ErrorBadDescriptor));
        Require(Fails(getsockopt_nid_postfix(fd, 0xffff, 4, &optionValue, &optionLength), ErrorBadDescriptor));
        Require(Fails(setsockopt_nid_postfix(fd, 0xffff, 4, &optionValue, sizeof(optionValue)), ErrorBadDescriptor));
        Require(Fails(send_nid_postfix(fd, "x", 1, 0), ErrorBadDescriptor));
        Require(Fails(recv_nid_postfix(fd, buffer, sizeof(buffer), 0), ErrorBadDescriptor));
        Require(Fails(bind_nid_postfix(fd, address.data(), address.size()), ErrorBadDescriptor));
        Require(Fails(listen_nid_postfix(fd, 1), ErrorBadDescriptor));
        Require(Fails(accept_nid_postfix(fd, nullptr, nullptr), ErrorBadDescriptor));
        Require(Fails(connect_nid_postfix(fd, address.data(), address.size()), ErrorBadDescriptor));
    }
}

void AuditSocketLifecycleAndDescriptorLimits() {
    const int streamSocket = socket_nid_postfix(AfInet, SockStream, 0);
    Require(streamSocket >= 0);
    Require(streamSocket < SocketDescriptorLimit);

    std::array<std::uint8_t, 16> address{16, AfInet, 0, 0, 127, 0, 0, 1};
    std::uint32_t addressSize = address.size();
    Require(getsockname_nid_postfix(streamSocket, address.data(), &addressSize) == 0);

    int optionValue = 1;
    Require(setsockopt_nid_postfix(streamSocket, 0xffff, 4, &optionValue, sizeof(optionValue)) == 0);

    int retrievedOption = 0;
    std::uint32_t optionLength = sizeof(retrievedOption);
    Require(getsockopt_nid_postfix(streamSocket, 0xffff, 4, &retrievedOption, &optionLength) == 0);
    Require(retrievedOption != 0);

    Require(close_nid_postfix(streamSocket) == 0);
    Require(Fails(close_nid_postfix(streamSocket), ErrorBadDescriptor));

    char buffer[16]{};
    Require(Fails(getsockname_nid_postfix(streamSocket, address.data(), &addressSize), ErrorBadDescriptor));
    Require(Fails(getpeername_nid_postfix(streamSocket, address.data(), &addressSize), ErrorBadDescriptor));
    Require(Fails(shutdown_nid_postfix(streamSocket, 0), ErrorBadDescriptor));
    Require(Fails(getsockopt_nid_postfix(streamSocket, 0xffff, 4, &retrievedOption, &optionLength), ErrorBadDescriptor));
    Require(Fails(setsockopt_nid_postfix(streamSocket, 0xffff, 4, &optionValue, sizeof(optionValue)), ErrorBadDescriptor));
    Require(Fails(send_nid_postfix(streamSocket, "x", 1, 0), ErrorBadDescriptor));
    Require(Fails(recv_nid_postfix(streamSocket, buffer, sizeof(buffer), 0), ErrorBadDescriptor));
    Require(Fails(bind_nid_postfix(streamSocket, address.data(), address.size()), ErrorBadDescriptor));
    Require(Fails(listen_nid_postfix(streamSocket, 1), ErrorBadDescriptor));
    Require(Fails(accept_nid_postfix(streamSocket, nullptr, nullptr), ErrorBadDescriptor));
    Require(Fails(connect_nid_postfix(streamSocket, address.data(), address.size()), ErrorBadDescriptor));

    const int dgramSocket = socket_nid_postfix(AfInet, SockDgram, 0);
    Require(dgramSocket >= 0);
    Require(dgramSocket < SocketDescriptorLimit);
    Require(close_nid_postfix(dgramSocket) == 0);
    Require(Fails(close_nid_postfix(dgramSocket), ErrorBadDescriptor));
    Require(Fails(shutdown_nid_postfix(dgramSocket, 0), ErrorBadDescriptor));
}

void AuditSequentialSocketDescriptors() {
    int sockets[8];
    for (int i = 0; i < 8; ++i) {
        sockets[i] = socket_nid_postfix(AfInet, SockStream, 0);
        Require(sockets[i] >= 0);
        Require(sockets[i] < SocketDescriptorLimit);
        for (int j = 0; j < i; ++j) {
            Require(sockets[i] != sockets[j]);
        }
    }

    for (int i = 7; i >= 0; --i) {
        Require(close_nid_postfix(sockets[i]) == 0);
        Require(Fails(close_nid_postfix(sockets[i]), ErrorBadDescriptor));
    }
}

} // namespace

int main() {
    AuditInvalidSocketDescriptors();
    AuditErrorPrecedenceOnInvalidDescriptors();
    AuditNonSocketDescriptors();
    AuditSocketLifecycleAndDescriptorLimits();
    AuditSequentialSocketDescriptors();
    return 0;
}
