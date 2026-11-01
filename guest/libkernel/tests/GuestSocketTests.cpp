#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {

constexpr int Stream = 1;
constexpr int Datagram = 2;
constexpr int SolSocket = 0xffff;
constexpr int SoKeepAlive = 0x8;
constexpr int SoSendBuffer = 0x1001;
constexpr int SoReceiveBuffer = 0x1002;
constexpr int BadDescriptor = 9;
constexpr int Fault = 14;
constexpr int Invalid = 22;
constexpr int NoProtocolOption = 42;

static void Check(bool condition, int line) {
    if (!condition) {
        std::fprintf(stderr, "assertion failed at line %d, errno %d\n", line, *__error_nid_postfix());
        std::abort();
    }
}
#define Require(condition) Check((condition), __LINE__)

void TestRecvfromBounds(int datagram, int stream) {
    char buffer[64]{};
    std::array<std::uint8_t, 16> address{};
    std::uint32_t addressLength = static_cast<std::uint32_t>(address.size());

    for (const int badDescriptor : {-1, -100, 1024, 65535}) {
        *__error_nid_postfix() = 0;
        Require(recvfrom_nid_postfix(badDescriptor, buffer, sizeof(buffer), 0, nullptr, nullptr) == -1);
        Require(*__error_nid_postfix() == BadDescriptor);

        *__error_nid_postfix() = 0;
        Require(_recvfrom_nid_postfix(badDescriptor, buffer, sizeof(buffer), 0, nullptr, nullptr) == -1);
        Require(*__error_nid_postfix() == BadDescriptor);

        *__error_nid_postfix() = 0;
        Require(recvfrom_nid_postfix(badDescriptor, buffer, sizeof(buffer), 0, address.data(), &addressLength) == -1);
        Require(*__error_nid_postfix() == BadDescriptor);

        *__error_nid_postfix() = 0;
        Require(_recvfrom_nid_postfix(badDescriptor, buffer, sizeof(buffer), 0, address.data(), &addressLength) == -1);
        Require(*__error_nid_postfix() == BadDescriptor);
    }

    for (const int socket : {datagram, stream}) {
        *__error_nid_postfix() = 0;
        Require(recvfrom_nid_postfix(socket, nullptr, 64, 0, nullptr, nullptr) == -1);
        Require(*__error_nid_postfix() == Fault);

        *__error_nid_postfix() = 0;
        Require(_recvfrom_nid_postfix(socket, nullptr, 64, 0, nullptr, nullptr) == -1);
        Require(*__error_nid_postfix() == Fault);

        *__error_nid_postfix() = 0;
        Require(recvfrom_nid_postfix(socket, buffer, sizeof(buffer), 0, address.data(), nullptr) == -1);
        Require(*__error_nid_postfix() == Fault);

        *__error_nid_postfix() = 0;
        Require(_recvfrom_nid_postfix(socket, buffer, sizeof(buffer), 0, address.data(), nullptr) == -1);
        Require(*__error_nid_postfix() == Fault);
    }
}

void TestSendtoBounds(int datagram, int stream) {
    char buffer[64]{};
    std::array<std::uint8_t, 16> address{16, 2, 0, 0, 127, 0, 0, 1};

    for (const int badDescriptor : {-1, -100, 1024, 65535}) {
        *__error_nid_postfix() = 0;
        Require(sendto_nid_postfix(badDescriptor, buffer, sizeof(buffer), 0, nullptr, 0) == -1);
        Require(*__error_nid_postfix() == BadDescriptor);

        *__error_nid_postfix() = 0;
        Require(_sendto_nid_postfix(badDescriptor, buffer, sizeof(buffer), 0, nullptr, 0) == -1);
        Require(*__error_nid_postfix() == BadDescriptor);

        *__error_nid_postfix() = 0;
        Require(sendto_nid_postfix(badDescriptor, buffer, sizeof(buffer), 0, address.data(), address.size()) == -1);
        Require(*__error_nid_postfix() == BadDescriptor);

        *__error_nid_postfix() = 0;
        Require(_sendto_nid_postfix(badDescriptor, buffer, sizeof(buffer), 0, address.data(), address.size()) == -1);
        Require(*__error_nid_postfix() == BadDescriptor);
    }

    for (const int socket : {datagram, stream}) {
        *__error_nid_postfix() = 0;
        Require(sendto_nid_postfix(socket, nullptr, 64, 0, nullptr, 0) == -1);
        Require(*__error_nid_postfix() == Fault);

        *__error_nid_postfix() = 0;
        Require(_sendto_nid_postfix(socket, nullptr, 64, 0, nullptr, 0) == -1);
        Require(*__error_nid_postfix() == Fault);

        *__error_nid_postfix() = 0;
        Require(sendto_nid_postfix(socket, nullptr, 64, 0, address.data(), address.size()) == -1);
        Require(*__error_nid_postfix() == Fault);

        *__error_nid_postfix() = 0;
        Require(_sendto_nid_postfix(socket, nullptr, 64, 0, address.data(), address.size()) == -1);
        Require(*__error_nid_postfix() == Fault);

        *__error_nid_postfix() = 0;
        Require(sendto_nid_postfix(socket, buffer, sizeof(buffer), 0, nullptr, 16) == -1);
        Require(*__error_nid_postfix() == Invalid);

        *__error_nid_postfix() = 0;
        Require(_sendto_nid_postfix(socket, buffer, sizeof(buffer), 0, nullptr, 16) == -1);
        Require(*__error_nid_postfix() == Invalid);
    }
}

void TestSocketOptionsBounds(int datagram, int stream) {
    int value = 1;
    std::uint32_t length = sizeof(value);

    for (const int badDescriptor : {-1, -100, 1024, 65535}) {
        *__error_nid_postfix() = 0;
        Require(setsockopt_nid_postfix(badDescriptor, SolSocket, SoKeepAlive, &value, sizeof(value)) == -1);
        Require(*__error_nid_postfix() == BadDescriptor);

        *__error_nid_postfix() = 0;
        Require(_setsockopt_nid_postfix(badDescriptor, SolSocket, SoKeepAlive, &value, sizeof(value)) == -1);
        Require(*__error_nid_postfix() == BadDescriptor);

        *__error_nid_postfix() = 0;
        Require(getsockopt_nid_postfix(badDescriptor, SolSocket, SoKeepAlive, &value, &length) == -1);
        Require(*__error_nid_postfix() == BadDescriptor);

        *__error_nid_postfix() = 0;
        Require(_getsockopt_nid_postfix(badDescriptor, SolSocket, SoKeepAlive, &value, &length) == -1);
        Require(*__error_nid_postfix() == BadDescriptor);
    }

    for (const int socket : {datagram, stream}) {
        *__error_nid_postfix() = 0;
        Require(setsockopt_nid_postfix(socket, SolSocket, SoKeepAlive, nullptr, sizeof(value)) == -1);
        Require(*__error_nid_postfix() == Fault);

        *__error_nid_postfix() = 0;
        Require(_setsockopt_nid_postfix(socket, SolSocket, SoKeepAlive, nullptr, sizeof(value)) == -1);
        Require(*__error_nid_postfix() == Fault);

        *__error_nid_postfix() = 0;
        Require(setsockopt_nid_postfix(socket, SolSocket, SoSendBuffer, nullptr, sizeof(value)) == -1);
        Require(*__error_nid_postfix() == Fault);

        *__error_nid_postfix() = 0;
        Require(_setsockopt_nid_postfix(socket, SolSocket, SoSendBuffer, nullptr, sizeof(value)) == -1);
        Require(*__error_nid_postfix() == Fault);

        *__error_nid_postfix() = 0;
        Require(getsockopt_nid_postfix(socket, SolSocket, SoKeepAlive, &value, nullptr) == -1);
        Require(*__error_nid_postfix() == Invalid);

        *__error_nid_postfix() = 0;
        Require(_getsockopt_nid_postfix(socket, SolSocket, SoKeepAlive, &value, nullptr) == -1);
        Require(*__error_nid_postfix() == Invalid);

        *__error_nid_postfix() = 0;
        Require(getsockopt_nid_postfix(socket, SolSocket, SoReceiveBuffer, &value, nullptr) == -1);
        Require(*__error_nid_postfix() == Invalid);

        *__error_nid_postfix() = 0;
        Require(_getsockopt_nid_postfix(socket, SolSocket, SoReceiveBuffer, &value, nullptr) == -1);
        Require(*__error_nid_postfix() == Invalid);

        *__error_nid_postfix() = 0;
        Require(setsockopt_nid_postfix(socket, 9999, SoKeepAlive, &value, sizeof(value)) == -1);
        Require(*__error_nid_postfix() == Invalid);

        *__error_nid_postfix() = 0;
        Require(_setsockopt_nid_postfix(socket, 9999, SoKeepAlive, &value, sizeof(value)) == -1);
        Require(*__error_nid_postfix() == Invalid);

        *__error_nid_postfix() = 0;
        Require(getsockopt_nid_postfix(socket, 9999, SoKeepAlive, &value, &length) == -1);
        Require(*__error_nid_postfix() == Invalid);

        *__error_nid_postfix() = 0;
        Require(_getsockopt_nid_postfix(socket, 9999, SoKeepAlive, &value, &length) == -1);
        Require(*__error_nid_postfix() == Invalid);

        *__error_nid_postfix() = 0;
        Require(setsockopt_nid_postfix(socket, SolSocket, 0x7fff, &value, sizeof(value)) == -1);
        Require(*__error_nid_postfix() == NoProtocolOption);

        *__error_nid_postfix() = 0;
        Require(_setsockopt_nid_postfix(socket, SolSocket, 0x7fff, &value, sizeof(value)) == -1);
        Require(*__error_nid_postfix() == NoProtocolOption);

        *__error_nid_postfix() = 0;
        Require(getsockopt_nid_postfix(socket, SolSocket, 0x7fff, &value, &length) == -1);
        Require(*__error_nid_postfix() == NoProtocolOption);

        *__error_nid_postfix() = 0;
        Require(_getsockopt_nid_postfix(socket, SolSocket, 0x7fff, &value, &length) == -1);
        Require(*__error_nid_postfix() == NoProtocolOption);
    }
}

}

int main() {
    const int datagram = socket_nid_postfix(2, Datagram, 0);
    Require(datagram >= 0);
    const int stream = socket_nid_postfix(2, Stream, 0);
    Require(stream >= 0);

    TestRecvfromBounds(datagram, stream);
    TestSendtoBounds(datagram, stream);
    TestSocketOptionsBounds(datagram, stream);

    Require(close_nid_postfix(datagram) == 0);
    Require(close_nid_postfix(stream) == 0);
    return 0;
}
