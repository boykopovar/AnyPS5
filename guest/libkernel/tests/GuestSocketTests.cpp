#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {

struct Timeval {
    std::int64_t seconds;
    std::int64_t microseconds;
};

constexpr int Stream = 1;
constexpr int Datagram = 2;
constexpr int SolSocket = 0xffff;
constexpr int SoKeepAlive = 0x8;
constexpr int SoSendTimeout = 0x1005;
constexpr int SoReceiveTimeout = 0x1006;
constexpr int BadDescriptor = 9;
constexpr int Fault = 14;
constexpr int Invalid = 22;
constexpr int Domain = 33;
constexpr int NoProtocolOption = 42;

static void Check(bool condition, int line) {
    if (!condition) {
        std::fprintf(stderr, "assertion failed at line %d, errno %d\n", line, *__error_nid_postfix());
        std::abort();
    }
}
#define Require(condition) Check((condition), __LINE__)

void TestBadDescriptorBounds() {
    int keepAlive = 1;
    Timeval timeout{1, 0};
    std::uint32_t intLength = sizeof(keepAlive);
    std::uint32_t timevalLength = sizeof(timeout);

    for (const int badDescriptor : {-1, -100, 1024, 65535}) {
        *__error_nid_postfix() = 0;
        Require(setsockopt_nid_postfix(badDescriptor, SolSocket, SoKeepAlive, &keepAlive, sizeof(keepAlive)) == -1);
        Require(*__error_nid_postfix() == BadDescriptor);

        *__error_nid_postfix() = 0;
        Require(_setsockopt_nid_postfix(badDescriptor, SolSocket, SoKeepAlive, &keepAlive, sizeof(keepAlive)) == -1);
        Require(*__error_nid_postfix() == BadDescriptor);

        *__error_nid_postfix() = 0;
        Require(getsockopt_nid_postfix(badDescriptor, SolSocket, SoKeepAlive, &keepAlive, &intLength) == -1);
        Require(*__error_nid_postfix() == BadDescriptor);

        *__error_nid_postfix() = 0;
        Require(_getsockopt_nid_postfix(badDescriptor, SolSocket, SoKeepAlive, &keepAlive, &intLength) == -1);
        Require(*__error_nid_postfix() == BadDescriptor);

        for (const int timeoutOption : {SoSendTimeout, SoReceiveTimeout}) {
            *__error_nid_postfix() = 0;
            Require(setsockopt_nid_postfix(badDescriptor, SolSocket, timeoutOption, &timeout, sizeof(timeout)) == -1);
            Require(*__error_nid_postfix() == BadDescriptor);

            *__error_nid_postfix() = 0;
            Require(_setsockopt_nid_postfix(badDescriptor, SolSocket, timeoutOption, &timeout, sizeof(timeout)) == -1);
            Require(*__error_nid_postfix() == BadDescriptor);

            *__error_nid_postfix() = 0;
            Require(getsockopt_nid_postfix(badDescriptor, SolSocket, timeoutOption, &timeout, &timevalLength) == -1);
            Require(*__error_nid_postfix() == BadDescriptor);

            *__error_nid_postfix() = 0;
            Require(_getsockopt_nid_postfix(badDescriptor, SolSocket, timeoutOption, &timeout, &timevalLength) == -1);
            Require(*__error_nid_postfix() == BadDescriptor);
        }
    }
}

void TestNullOptvalBounds(int datagram, int stream) {
    int keepAlive = 1;
    Timeval timeout{1, 0};

    for (const int socket : {datagram, stream}) {
        *__error_nid_postfix() = 0;
        Require(setsockopt_nid_postfix(socket, SolSocket, SoKeepAlive, nullptr, sizeof(keepAlive)) == -1);
        Require(*__error_nid_postfix() == Fault);

        *__error_nid_postfix() = 0;
        Require(_setsockopt_nid_postfix(socket, SolSocket, SoKeepAlive, nullptr, sizeof(keepAlive)) == -1);
        Require(*__error_nid_postfix() == Fault);

        *__error_nid_postfix() = 0;
        Require(setsockopt_nid_postfix(socket, SolSocket, SoKeepAlive, nullptr, 1) == -1);
        Require(*__error_nid_postfix() == Fault);

        *__error_nid_postfix() = 0;
        Require(_setsockopt_nid_postfix(socket, SolSocket, SoKeepAlive, nullptr, 1) == -1);
        Require(*__error_nid_postfix() == Fault);

        *__error_nid_postfix() = 0;
        Require(setsockopt_nid_postfix(socket, SolSocket, SoKeepAlive, nullptr, 0) == -1);
        Require(*__error_nid_postfix() == Invalid);

        *__error_nid_postfix() = 0;
        Require(_setsockopt_nid_postfix(socket, SolSocket, SoKeepAlive, nullptr, 0) == -1);
        Require(*__error_nid_postfix() == Invalid);

        *__error_nid_postfix() = 0;
        Require(getsockopt_nid_postfix(socket, SolSocket, SoKeepAlive, &keepAlive, nullptr) == -1);
        Require(*__error_nid_postfix() == Invalid);

        *__error_nid_postfix() = 0;
        Require(_getsockopt_nid_postfix(socket, SolSocket, SoKeepAlive, &keepAlive, nullptr) == -1);
        Require(*__error_nid_postfix() == Invalid);

        *__error_nid_postfix() = 0;
        Require(getsockopt_nid_postfix(socket, SolSocket, SoKeepAlive, nullptr, nullptr) == -1);
        Require(*__error_nid_postfix() == Invalid);

        *__error_nid_postfix() = 0;
        Require(_getsockopt_nid_postfix(socket, SolSocket, SoKeepAlive, nullptr, nullptr) == -1);
        Require(*__error_nid_postfix() == Invalid);

        for (const int timeoutOption : {SoSendTimeout, SoReceiveTimeout}) {
            *__error_nid_postfix() = 0;
            Require(setsockopt_nid_postfix(socket, SolSocket, timeoutOption, nullptr, sizeof(timeout)) == -1);
            Require(*__error_nid_postfix() == Fault);

            *__error_nid_postfix() = 0;
            Require(_setsockopt_nid_postfix(socket, SolSocket, timeoutOption, nullptr, sizeof(timeout)) == -1);
            Require(*__error_nid_postfix() == Fault);

            *__error_nid_postfix() = 0;
            Require(setsockopt_nid_postfix(socket, SolSocket, timeoutOption, nullptr, 1) == -1);
            Require(*__error_nid_postfix() == Fault);

            *__error_nid_postfix() = 0;
            Require(_setsockopt_nid_postfix(socket, SolSocket, timeoutOption, nullptr, 1) == -1);
            Require(*__error_nid_postfix() == Fault);

            *__error_nid_postfix() = 0;
            Require(setsockopt_nid_postfix(socket, SolSocket, timeoutOption, nullptr, 0) == -1);
            Require(*__error_nid_postfix() == Invalid);

            *__error_nid_postfix() = 0;
            Require(_setsockopt_nid_postfix(socket, SolSocket, timeoutOption, nullptr, 0) == -1);
            Require(*__error_nid_postfix() == Invalid);

            *__error_nid_postfix() = 0;
            Require(getsockopt_nid_postfix(socket, SolSocket, timeoutOption, &timeout, nullptr) == -1);
            Require(*__error_nid_postfix() == Invalid);

            *__error_nid_postfix() = 0;
            Require(_getsockopt_nid_postfix(socket, SolSocket, timeoutOption, &timeout, nullptr) == -1);
            Require(*__error_nid_postfix() == Invalid);

            *__error_nid_postfix() = 0;
            Require(getsockopt_nid_postfix(socket, SolSocket, timeoutOption, nullptr, nullptr) == -1);
            Require(*__error_nid_postfix() == Invalid);

            *__error_nid_postfix() = 0;
            Require(_getsockopt_nid_postfix(socket, SolSocket, timeoutOption, nullptr, nullptr) == -1);
            Require(*__error_nid_postfix() == Invalid);
        }
    }
}

void TestInvalidOptlenBounds(int datagram, int stream) {
    int keepAlive = 1;
    Timeval timeout{1, 0};

    for (const int socket : {datagram, stream}) {
        for (const std::uint32_t invalidLength : {0u, 1u, 2u, 3u}) {
            *__error_nid_postfix() = 0;
            Require(setsockopt_nid_postfix(socket, SolSocket, SoKeepAlive, &keepAlive, invalidLength) == -1);
            Require(*__error_nid_postfix() == Invalid);

            *__error_nid_postfix() = 0;
            Require(_setsockopt_nid_postfix(socket, SolSocket, SoKeepAlive, &keepAlive, invalidLength) == -1);
            Require(*__error_nid_postfix() == Invalid);
        }

        for (const int timeoutOption : {SoSendTimeout, SoReceiveTimeout}) {
            for (const std::uint32_t invalidLength : {0u, 1u, 4u, 8u, 12u, 15u}) {
                *__error_nid_postfix() = 0;
                Require(setsockopt_nid_postfix(socket, SolSocket, timeoutOption, &timeout, invalidLength) == -1);
                Require(*__error_nid_postfix() == Invalid);

                *__error_nid_postfix() = 0;
                Require(_setsockopt_nid_postfix(socket, SolSocket, timeoutOption, &timeout, invalidLength) == -1);
                Require(*__error_nid_postfix() == Invalid);
            }
        }
    }
}

void TestInvalidTimeoutValueBounds(int datagram, int stream) {
    const std::array<Timeval, 8> invalidTimes{{
        {-1, 0},
        {-100, 0},
        {-1, 500000},
        {-1, -1},
        {0, -1},
        {0, -100},
        {0, 1000000},
        {5, 2000000}
    }};

    for (const int socket : {datagram, stream}) {
        for (const int timeoutOption : {SoSendTimeout, SoReceiveTimeout}) {
            for (const auto& invalidTime : invalidTimes) {
                *__error_nid_postfix() = 0;
                Require(setsockopt_nid_postfix(socket, SolSocket, timeoutOption, &invalidTime, sizeof(invalidTime)) == -1);
                Require(*__error_nid_postfix() == Domain);

                *__error_nid_postfix() = 0;
                Require(_setsockopt_nid_postfix(socket, SolSocket, timeoutOption, &invalidTime, sizeof(invalidTime)) == -1);
                Require(*__error_nid_postfix() == Domain);
            }
        }
    }
}

void TestInvalidLevelAndOptionBounds(int datagram, int stream) {
    int keepAlive = 1;
    Timeval timeout{1, 0};
    std::uint32_t intLength = sizeof(keepAlive);
    std::uint32_t timevalLength = sizeof(timeout);

    for (const int socket : {datagram, stream}) {
        for (const int invalidLevel : {-1, 9999, 12345}) {
            *__error_nid_postfix() = 0;
            Require(setsockopt_nid_postfix(socket, invalidLevel, SoKeepAlive, &keepAlive, sizeof(keepAlive)) == -1);
            Require(*__error_nid_postfix() == Invalid);

            *__error_nid_postfix() = 0;
            Require(_setsockopt_nid_postfix(socket, invalidLevel, SoKeepAlive, &keepAlive, sizeof(keepAlive)) == -1);
            Require(*__error_nid_postfix() == Invalid);

            *__error_nid_postfix() = 0;
            Require(getsockopt_nid_postfix(socket, invalidLevel, SoKeepAlive, &keepAlive, &intLength) == -1);
            Require(*__error_nid_postfix() == Invalid);

            *__error_nid_postfix() = 0;
            Require(_getsockopt_nid_postfix(socket, invalidLevel, SoKeepAlive, &keepAlive, &intLength) == -1);
            Require(*__error_nid_postfix() == Invalid);

            for (const int timeoutOption : {SoSendTimeout, SoReceiveTimeout}) {
                *__error_nid_postfix() = 0;
                Require(setsockopt_nid_postfix(socket, invalidLevel, timeoutOption, &timeout, sizeof(timeout)) == -1);
                Require(*__error_nid_postfix() == Invalid);

                *__error_nid_postfix() = 0;
                Require(_setsockopt_nid_postfix(socket, invalidLevel, timeoutOption, &timeout, sizeof(timeout)) == -1);
                Require(*__error_nid_postfix() == Invalid);

                *__error_nid_postfix() = 0;
                Require(getsockopt_nid_postfix(socket, invalidLevel, timeoutOption, &timeout, &timevalLength) == -1);
                Require(*__error_nid_postfix() == Invalid);

                *__error_nid_postfix() = 0;
                Require(_getsockopt_nid_postfix(socket, invalidLevel, timeoutOption, &timeout, &timevalLength) == -1);
                Require(*__error_nid_postfix() == Invalid);
            }
        }

        for (const int invalidOption : {0x7fff, 9999}) {
            *__error_nid_postfix() = 0;
            Require(setsockopt_nid_postfix(socket, SolSocket, invalidOption, &keepAlive, sizeof(keepAlive)) == -1);
            Require(*__error_nid_postfix() == NoProtocolOption);

            *__error_nid_postfix() = 0;
            Require(_setsockopt_nid_postfix(socket, SolSocket, invalidOption, &keepAlive, sizeof(keepAlive)) == -1);
            Require(*__error_nid_postfix() == NoProtocolOption);

            *__error_nid_postfix() = 0;
            Require(getsockopt_nid_postfix(socket, SolSocket, invalidOption, &keepAlive, &intLength) == -1);
            Require(*__error_nid_postfix() == NoProtocolOption);

            *__error_nid_postfix() = 0;
            Require(_getsockopt_nid_postfix(socket, SolSocket, invalidOption, &keepAlive, &intLength) == -1);
            Require(*__error_nid_postfix() == NoProtocolOption);
        }
    }
}

void TestSocketOptionRoundtrips(int datagram, int stream) {
    for (const int socket : {datagram, stream}) {
        int keepAliveValue = 1;
        Require(setsockopt_nid_postfix(socket, SolSocket, SoKeepAlive, &keepAliveValue, sizeof(keepAliveValue)) == 0);
        int readKeepAlive = 0;
        std::uint32_t length = sizeof(readKeepAlive);
        Require(getsockopt_nid_postfix(socket, SolSocket, SoKeepAlive, &readKeepAlive, &length) == 0);
        Require(length == sizeof(readKeepAlive));
        Require(readKeepAlive == SoKeepAlive);

        keepAliveValue = 0;
        Require(_setsockopt_nid_postfix(socket, SolSocket, SoKeepAlive, &keepAliveValue, sizeof(keepAliveValue)) == 0);
        Require(_getsockopt_nid_postfix(socket, SolSocket, SoKeepAlive, &readKeepAlive, &length) == 0);
        Require(readKeepAlive == 0);

        for (const int timeoutOption : {SoSendTimeout, SoReceiveTimeout}) {
            const Timeval setTime{2, 500000};
            Require(setsockopt_nid_postfix(socket, SolSocket, timeoutOption, &setTime, sizeof(setTime)) == 0);
            Timeval readTime{-1, -1};
            length = sizeof(readTime);
            Require(getsockopt_nid_postfix(socket, SolSocket, timeoutOption, &readTime, &length) == 0);
            Require(length == sizeof(readTime));
            Require(readTime.seconds == 2);
            Require(readTime.microseconds == 499999);

            const Timeval resetTime{0, 0};
            Require(_setsockopt_nid_postfix(socket, SolSocket, timeoutOption, &resetTime, sizeof(resetTime)) == 0);
            Require(_getsockopt_nid_postfix(socket, SolSocket, timeoutOption, &readTime, &length) == 0);
            Require(readTime.seconds == 0);
            Require(readTime.microseconds == 0);
        }
    }
}

}

int main() {
    const int datagram = socket_nid_postfix(2, Datagram, 0);
    Require(datagram >= 0);
    const int stream = socket_nid_postfix(2, Stream, 0);
    Require(stream >= 0);

    TestBadDescriptorBounds();
    TestNullOptvalBounds(datagram, stream);
    TestInvalidOptlenBounds(datagram, stream);
    TestInvalidTimeoutValueBounds(datagram, stream);
    TestInvalidLevelAndOptionBounds(datagram, stream);
    TestSocketOptionRoundtrips(datagram, stream);

    Require(close_nid_postfix(datagram) == 0);
    Require(close_nid_postfix(stream) == 0);
    return 0;
}
