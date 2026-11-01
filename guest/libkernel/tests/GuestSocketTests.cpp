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
constexpr int BadDescriptor = 9;
constexpr int Fault = 14;
constexpr int Invalid = 22;
constexpr int NotConnected = 57;

static void Check(bool condition, int line) {
    if (!condition) {
        std::fprintf(stderr, "assertion failed at line %d, errno %d\n", line, *__error_nid_postfix());
        std::abort();
    }
}
#define Require(condition) Check((condition), __LINE__)

void TestGetSockNameBounds(int datagram, int stream) {
    std::array<std::uint8_t, 16> address{};
    std::uint32_t addressLength = static_cast<std::uint32_t>(address.size());

    for (const int badDescriptor : {-1, -100, 1024, 65535}) {
        *__error_nid_postfix() = 0;
        Require(getsockname_nid_postfix(badDescriptor, address.data(), &addressLength) == -1);
        Require(*__error_nid_postfix() == BadDescriptor);

        *__error_nid_postfix() = 0;
        Require(_getsockname_nid_postfix(badDescriptor, address.data(), &addressLength) == -1);
        Require(*__error_nid_postfix() == BadDescriptor);

        *__error_nid_postfix() = 0;
        Require(getsockname_nid_postfix(badDescriptor, nullptr, &addressLength) == -1);
        Require(*__error_nid_postfix() == BadDescriptor);

        *__error_nid_postfix() = 0;
        Require(_getsockname_nid_postfix(badDescriptor, nullptr, &addressLength) == -1);
        Require(*__error_nid_postfix() == BadDescriptor);

        *__error_nid_postfix() = 0;
        Require(getsockname_nid_postfix(badDescriptor, address.data(), nullptr) == -1);
        Require(*__error_nid_postfix() == BadDescriptor);

        *__error_nid_postfix() = 0;
        Require(_getsockname_nid_postfix(badDescriptor, address.data(), nullptr) == -1);
        Require(*__error_nid_postfix() == BadDescriptor);

        *__error_nid_postfix() = 0;
        Require(getsockname_nid_postfix(badDescriptor, nullptr, nullptr) == -1);
        Require(*__error_nid_postfix() == BadDescriptor);

        *__error_nid_postfix() = 0;
        Require(_getsockname_nid_postfix(badDescriptor, nullptr, nullptr) == -1);
        Require(*__error_nid_postfix() == BadDescriptor);
    }

    for (const int socket : {datagram, stream}) {
        *__error_nid_postfix() = 0;
        Require(getsockname_nid_postfix(socket, nullptr, &addressLength) == -1);
        Require(*__error_nid_postfix() == Fault);

        *__error_nid_postfix() = 0;
        Require(_getsockname_nid_postfix(socket, nullptr, &addressLength) == -1);
        Require(*__error_nid_postfix() == Fault);

        *__error_nid_postfix() = 0;
        Require(getsockname_nid_postfix(socket, address.data(), nullptr) == -1);
        Require(*__error_nid_postfix() == Fault);

        *__error_nid_postfix() = 0;
        Require(_getsockname_nid_postfix(socket, address.data(), nullptr) == -1);
        Require(*__error_nid_postfix() == Fault);

        *__error_nid_postfix() = 0;
        Require(getsockname_nid_postfix(socket, nullptr, nullptr) == -1);
        Require(*__error_nid_postfix() == Fault);

        *__error_nid_postfix() = 0;
        Require(_getsockname_nid_postfix(socket, nullptr, nullptr) == -1);
        Require(*__error_nid_postfix() == Fault);
    }
}

void TestGetPeerNameBounds(int datagram, int stream) {
    std::array<std::uint8_t, 16> address{};
    std::uint32_t addressLength = static_cast<std::uint32_t>(address.size());

    for (const int badDescriptor : {-1, -100, 1024, 65535}) {
        *__error_nid_postfix() = 0;
        Require(getpeername_nid_postfix(badDescriptor, address.data(), &addressLength) == -1);
        Require(*__error_nid_postfix() == BadDescriptor);

        *__error_nid_postfix() = 0;
        Require(_getpeername_nid_postfix(badDescriptor, address.data(), &addressLength) == -1);
        Require(*__error_nid_postfix() == BadDescriptor);

        *__error_nid_postfix() = 0;
        Require(getpeername_nid_postfix(badDescriptor, nullptr, &addressLength) == -1);
        Require(*__error_nid_postfix() == BadDescriptor);

        *__error_nid_postfix() = 0;
        Require(_getpeername_nid_postfix(badDescriptor, nullptr, &addressLength) == -1);
        Require(*__error_nid_postfix() == BadDescriptor);

        *__error_nid_postfix() = 0;
        Require(getpeername_nid_postfix(badDescriptor, address.data(), nullptr) == -1);
        Require(*__error_nid_postfix() == BadDescriptor);

        *__error_nid_postfix() = 0;
        Require(_getpeername_nid_postfix(badDescriptor, address.data(), nullptr) == -1);
        Require(*__error_nid_postfix() == BadDescriptor);

        *__error_nid_postfix() = 0;
        Require(getpeername_nid_postfix(badDescriptor, nullptr, nullptr) == -1);
        Require(*__error_nid_postfix() == BadDescriptor);

        *__error_nid_postfix() = 0;
        Require(_getpeername_nid_postfix(badDescriptor, nullptr, nullptr) == -1);
        Require(*__error_nid_postfix() == BadDescriptor);
    }

    for (const int socket : {datagram, stream}) {
        *__error_nid_postfix() = 0;
        Require(getpeername_nid_postfix(socket, nullptr, &addressLength) == -1);
        Require(*__error_nid_postfix() == Fault);

        *__error_nid_postfix() = 0;
        Require(_getpeername_nid_postfix(socket, nullptr, &addressLength) == -1);
        Require(*__error_nid_postfix() == Fault);

        *__error_nid_postfix() = 0;
        Require(getpeername_nid_postfix(socket, address.data(), nullptr) == -1);
        Require(*__error_nid_postfix() == Fault);

        *__error_nid_postfix() = 0;
        Require(_getpeername_nid_postfix(socket, address.data(), nullptr) == -1);
        Require(*__error_nid_postfix() == Fault);

        *__error_nid_postfix() = 0;
        Require(getpeername_nid_postfix(socket, nullptr, nullptr) == -1);
        Require(*__error_nid_postfix() == Fault);

        *__error_nid_postfix() = 0;
        Require(_getpeername_nid_postfix(socket, nullptr, nullptr) == -1);
        Require(*__error_nid_postfix() == Fault);

        *__error_nid_postfix() = 0;
        Require(getpeername_nid_postfix(socket, address.data(), &addressLength) == -1);
        Require(*__error_nid_postfix() == NotConnected);

        *__error_nid_postfix() = 0;
        Require(_getpeername_nid_postfix(socket, address.data(), &addressLength) == -1);
        Require(*__error_nid_postfix() == NotConnected);
    }
}

void TestShutdownBounds(int datagram, int stream) {
    for (const int badDescriptor : {-1, -100, 1024, 65535}) {
        for (const int how : {0, 1, 2}) {
            *__error_nid_postfix() = 0;
            Require(shutdown_nid_postfix(badDescriptor, how) == -1);
            Require(*__error_nid_postfix() == BadDescriptor);
        }
    }

    for (const int socket : {datagram, stream}) {
        for (const int invalidHow : {-1, -10, 3, 4, 100}) {
            *__error_nid_postfix() = 0;
            Require(shutdown_nid_postfix(socket, invalidHow) == -1);
            Require(*__error_nid_postfix() == Invalid);
        }

        for (const int validHow : {0, 1, 2}) {
            *__error_nid_postfix() = 0;
            Require(shutdown_nid_postfix(socket, validHow) == -1);
            Require(*__error_nid_postfix() == NotConnected);
        }
    }
}

}

int main() {
    const int datagram = socket_nid_postfix(2, Datagram, 0);
    Require(datagram >= 0);
    const int stream = socket_nid_postfix(2, Stream, 0);
    Require(stream >= 0);

    TestGetSockNameBounds(datagram, stream);
    TestGetPeerNameBounds(datagram, stream);
    TestShutdownBounds(datagram, stream);

    Require(close_nid_postfix(datagram) == 0);
    Require(close_nid_postfix(stream) == 0);
    return 0;
}
