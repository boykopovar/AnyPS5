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
constexpr int OperationNotSupported = 45;

static void Check(bool condition, int line) {
    if (!condition) {
        std::fprintf(stderr, "assertion failed at line %d, errno %d\n", line, *__error_nid_postfix());
        std::abort();
    }
}
#define Require(condition) Check((condition), __LINE__)

void TestBindBounds(int datagram, int stream) {
    std::array<std::uint8_t, 16> address{16, 2, 0, 0, 127, 0, 0, 1};

    for (const int badDescriptor : {-1, -100, 1024, 65535}) {
        *__error_nid_postfix() = 0;
        Require(bind_nid_postfix(badDescriptor, address.data(), address.size()) == -1);
        Require(*__error_nid_postfix() == BadDescriptor);

        *__error_nid_postfix() = 0;
        Require(_bind_nid_postfix(badDescriptor, address.data(), address.size()) == -1);
        Require(*__error_nid_postfix() == BadDescriptor);

        *__error_nid_postfix() = 0;
        Require(bind_nid_postfix(badDescriptor, nullptr, 16) == -1);
        Require(*__error_nid_postfix() == Fault);

        *__error_nid_postfix() = 0;
        Require(_bind_nid_postfix(badDescriptor, nullptr, 16) == -1);
        Require(*__error_nid_postfix() == Fault);
    }

    for (const int socket : {datagram, stream}) {
        *__error_nid_postfix() = 0;
        Require(bind_nid_postfix(socket, nullptr, 16) == -1);
        Require(*__error_nid_postfix() == Fault);

        *__error_nid_postfix() = 0;
        Require(_bind_nid_postfix(socket, nullptr, 16) == -1);
        Require(*__error_nid_postfix() == Fault);

        *__error_nid_postfix() = 0;
        Require(bind_nid_postfix(socket, nullptr, 0) == -1);
        Require(*__error_nid_postfix() == Invalid);

        *__error_nid_postfix() = 0;
        Require(_bind_nid_postfix(socket, nullptr, 0) == -1);
        Require(*__error_nid_postfix() == Invalid);

        *__error_nid_postfix() = 0;
        Require(bind_nid_postfix(socket, nullptr, 1) == -1);
        Require(*__error_nid_postfix() == Invalid);

        *__error_nid_postfix() = 0;
        Require(_bind_nid_postfix(socket, nullptr, 1) == -1);
        Require(*__error_nid_postfix() == Invalid);
    }
}

void TestListenBounds(int datagram, int stream) {
    for (const int badDescriptor : {-1, -100, 1024, 65535}) {
        *__error_nid_postfix() = 0;
        Require(listen_nid_postfix(badDescriptor, 5) == -1);
        Require(*__error_nid_postfix() == BadDescriptor);

        *__error_nid_postfix() = 0;
        Require(_listen_nid_postfix(badDescriptor, 5) == -1);
        Require(*__error_nid_postfix() == BadDescriptor);
    }

    *__error_nid_postfix() = 0;
    Require(listen_nid_postfix(datagram, 5) == -1);
    Require(*__error_nid_postfix() == OperationNotSupported);

    *__error_nid_postfix() = 0;
    Require(_listen_nid_postfix(datagram, 5) == -1);
    Require(*__error_nid_postfix() == OperationNotSupported);
}

void TestAcceptBounds(int datagram, int stream) {
    std::array<std::uint8_t, 16> address{};
    std::uint32_t addressLength = static_cast<std::uint32_t>(address.size());

    for (const int badDescriptor : {-1, -100, 1024, 65535}) {
        *__error_nid_postfix() = 0;
        Require(accept_nid_postfix(badDescriptor, nullptr, nullptr) == -1);
        Require(*__error_nid_postfix() == BadDescriptor);

        *__error_nid_postfix() = 0;
        Require(_accept_nid_postfix(badDescriptor, nullptr, nullptr) == -1);
        Require(*__error_nid_postfix() == BadDescriptor);

        *__error_nid_postfix() = 0;
        Require(accept_nid_postfix(badDescriptor, address.data(), &addressLength) == -1);
        Require(*__error_nid_postfix() == BadDescriptor);

        *__error_nid_postfix() = 0;
        Require(_accept_nid_postfix(badDescriptor, address.data(), &addressLength) == -1);
        Require(*__error_nid_postfix() == BadDescriptor);

        *__error_nid_postfix() = 0;
        Require(accept_nid_postfix(badDescriptor, address.data(), nullptr) == -1);
        Require(*__error_nid_postfix() == BadDescriptor);

        *__error_nid_postfix() = 0;
        Require(_accept_nid_postfix(badDescriptor, address.data(), nullptr) == -1);
        Require(*__error_nid_postfix() == BadDescriptor);
    }

    *__error_nid_postfix() = 0;
    Require(accept_nid_postfix(stream, address.data(), nullptr) == -1);
    Require(*__error_nid_postfix() == Invalid);

    *__error_nid_postfix() = 0;
    Require(_accept_nid_postfix(stream, address.data(), nullptr) == -1);
    Require(*__error_nid_postfix() == Invalid);

    *__error_nid_postfix() = 0;
    Require(accept_nid_postfix(datagram, nullptr, nullptr) == -1);
    Require(*__error_nid_postfix() == Invalid);

    *__error_nid_postfix() = 0;
    Require(_accept_nid_postfix(datagram, nullptr, nullptr) == -1);
    Require(*__error_nid_postfix() == Invalid);

    *__error_nid_postfix() = 0;
    Require(accept_nid_postfix(datagram, address.data(), &addressLength) == -1);
    Require(*__error_nid_postfix() == Invalid);

    *__error_nid_postfix() = 0;
    Require(_accept_nid_postfix(datagram, address.data(), &addressLength) == -1);
    Require(*__error_nid_postfix() == Invalid);
}

void TestConnectBounds(int datagram, int stream) {
    std::array<std::uint8_t, 16> address{16, 2, 0, 0, 127, 0, 0, 1};

    for (const int badDescriptor : {-1, -100, 1024, 65535}) {
        *__error_nid_postfix() = 0;
        Require(connect_nid_postfix(badDescriptor, address.data(), address.size()) == -1);
        Require(*__error_nid_postfix() == BadDescriptor);

        *__error_nid_postfix() = 0;
        Require(_connect_nid_postfix(badDescriptor, address.data(), address.size()) == -1);
        Require(*__error_nid_postfix() == BadDescriptor);

        *__error_nid_postfix() = 0;
        Require(connect_nid_postfix(badDescriptor, nullptr, 16) == -1);
        Require(*__error_nid_postfix() == Fault);

        *__error_nid_postfix() = 0;
        Require(_connect_nid_postfix(badDescriptor, nullptr, 16) == -1);
        Require(*__error_nid_postfix() == Fault);
    }

    for (const int socket : {datagram, stream}) {
        *__error_nid_postfix() = 0;
        Require(connect_nid_postfix(socket, nullptr, 16) == -1);
        Require(*__error_nid_postfix() == Fault);

        *__error_nid_postfix() = 0;
        Require(_connect_nid_postfix(socket, nullptr, 16) == -1);
        Require(*__error_nid_postfix() == Fault);

        *__error_nid_postfix() = 0;
        Require(connect_nid_postfix(socket, nullptr, 0) == -1);
        Require(*__error_nid_postfix() == Invalid);

        *__error_nid_postfix() = 0;
        Require(_connect_nid_postfix(socket, nullptr, 0) == -1);
        Require(*__error_nid_postfix() == Invalid);
    }
}

}

int main() {
    const int datagram = socket_nid_postfix(2, Datagram, 0);
    Require(datagram >= 0);
    const int stream = socket_nid_postfix(2, Stream, 0);
    Require(stream >= 0);

    TestBindBounds(datagram, stream);
    TestListenBounds(datagram, stream);
    TestAcceptBounds(datagram, stream);
    TestConnectBounds(datagram, stream);

    Require(close_nid_postfix(datagram) == 0);
    Require(close_nid_postfix(stream) == 0);
    return 0;
}
