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
constexpr int IpLevel = 0;
constexpr int SolSocket = 0xffff;
constexpr int SoBroadcast = 0x20;
constexpr int IpMulticastInterface = 9;
constexpr int IpMulticastTimeToLive = 10;
constexpr int BadDescriptor = 9;
constexpr int Fault = 14;
constexpr int Invalid = 22;
constexpr int NoProtocolOption = 42;
constexpr int OperationNotSupported = 45;

static void Check(bool condition, int line) {
    if (!condition) {
        std::fprintf(stderr, "assertion failed at line %d, errno %d\n", line, *__error_nid_postfix());
        std::abort();
    }
}
#define Require(condition) Check((condition), __LINE__)

void TestBadDescriptorBounds() {
    int broadcast = 1;
    std::uint32_t address = 0;
    int ttl = 5;
    std::uint32_t intLength = sizeof(broadcast);
    std::uint32_t addressLength = sizeof(address);

    for (const int badDescriptor : {-1, -100, 1024, 65535}) {
        *__error_nid_postfix() = 0;
        Require(setsockopt_nid_postfix(badDescriptor, SolSocket, SoBroadcast, &broadcast, sizeof(broadcast)) == -1);
        Require(*__error_nid_postfix() == BadDescriptor);

        *__error_nid_postfix() = 0;
        Require(_setsockopt_nid_postfix(badDescriptor, SolSocket, SoBroadcast, &broadcast, sizeof(broadcast)) == -1);
        Require(*__error_nid_postfix() == BadDescriptor);

        *__error_nid_postfix() = 0;
        Require(getsockopt_nid_postfix(badDescriptor, SolSocket, SoBroadcast, &broadcast, &intLength) == -1);
        Require(*__error_nid_postfix() == BadDescriptor);

        *__error_nid_postfix() = 0;
        Require(_getsockopt_nid_postfix(badDescriptor, SolSocket, SoBroadcast, &broadcast, &intLength) == -1);
        Require(*__error_nid_postfix() == BadDescriptor);

        *__error_nid_postfix() = 0;
        Require(setsockopt_nid_postfix(badDescriptor, IpLevel, IpMulticastInterface, &address, sizeof(address)) == -1);
        Require(*__error_nid_postfix() == BadDescriptor);

        *__error_nid_postfix() = 0;
        Require(_setsockopt_nid_postfix(badDescriptor, IpLevel, IpMulticastInterface, &address, sizeof(address)) == -1);
        Require(*__error_nid_postfix() == BadDescriptor);

        *__error_nid_postfix() = 0;
        Require(getsockopt_nid_postfix(badDescriptor, IpLevel, IpMulticastInterface, &address, &addressLength) == -1);
        Require(*__error_nid_postfix() == BadDescriptor);

        *__error_nid_postfix() = 0;
        Require(_getsockopt_nid_postfix(badDescriptor, IpLevel, IpMulticastInterface, &address, &addressLength) == -1);
        Require(*__error_nid_postfix() == BadDescriptor);

        *__error_nid_postfix() = 0;
        Require(setsockopt_nid_postfix(badDescriptor, IpLevel, IpMulticastTimeToLive, &ttl, sizeof(ttl)) == -1);
        Require(*__error_nid_postfix() == BadDescriptor);

        *__error_nid_postfix() = 0;
        Require(_setsockopt_nid_postfix(badDescriptor, IpLevel, IpMulticastTimeToLive, &ttl, sizeof(ttl)) == -1);
        Require(*__error_nid_postfix() == BadDescriptor);

        *__error_nid_postfix() = 0;
        Require(getsockopt_nid_postfix(badDescriptor, IpLevel, IpMulticastTimeToLive, &ttl, &intLength) == -1);
        Require(*__error_nid_postfix() == BadDescriptor);

        *__error_nid_postfix() = 0;
        Require(_getsockopt_nid_postfix(badDescriptor, IpLevel, IpMulticastTimeToLive, &ttl, &intLength) == -1);
        Require(*__error_nid_postfix() == BadDescriptor);
    }
}

void TestStreamMulticastUnsupported(int stream) {
    std::uint32_t address = 0;
    int ttl = 5;
    std::uint32_t addressLength = sizeof(address);
    std::uint32_t ttlLength = sizeof(ttl);

    *__error_nid_postfix() = 0;
    Require(setsockopt_nid_postfix(stream, IpLevel, IpMulticastInterface, &address, sizeof(address)) == -1);
    Require(*__error_nid_postfix() == OperationNotSupported);

    *__error_nid_postfix() = 0;
    Require(_setsockopt_nid_postfix(stream, IpLevel, IpMulticastInterface, &address, sizeof(address)) == -1);
    Require(*__error_nid_postfix() == OperationNotSupported);

    *__error_nid_postfix() = 0;
    Require(getsockopt_nid_postfix(stream, IpLevel, IpMulticastInterface, &address, &addressLength) == -1);
    Require(*__error_nid_postfix() == OperationNotSupported);

    *__error_nid_postfix() = 0;
    Require(_getsockopt_nid_postfix(stream, IpLevel, IpMulticastInterface, &address, &addressLength) == -1);
    Require(*__error_nid_postfix() == OperationNotSupported);

    *__error_nid_postfix() = 0;
    Require(setsockopt_nid_postfix(stream, IpLevel, IpMulticastTimeToLive, &ttl, sizeof(ttl)) == -1);
    Require(*__error_nid_postfix() == OperationNotSupported);

    *__error_nid_postfix() = 0;
    Require(_setsockopt_nid_postfix(stream, IpLevel, IpMulticastTimeToLive, &ttl, sizeof(ttl)) == -1);
    Require(*__error_nid_postfix() == OperationNotSupported);

    *__error_nid_postfix() = 0;
    Require(getsockopt_nid_postfix(stream, IpLevel, IpMulticastTimeToLive, &ttl, &ttlLength) == -1);
    Require(*__error_nid_postfix() == OperationNotSupported);

    *__error_nid_postfix() = 0;
    Require(_getsockopt_nid_postfix(stream, IpLevel, IpMulticastTimeToLive, &ttl, &ttlLength) == -1);
    Require(*__error_nid_postfix() == OperationNotSupported);
}

void TestNullOptvalBounds(int datagram, int stream) {
    int broadcast = 1;
    std::uint32_t address = 0;
    int ttl = 5;

    for (const int socket : {datagram, stream}) {
        *__error_nid_postfix() = 0;
        Require(setsockopt_nid_postfix(socket, SolSocket, SoBroadcast, nullptr, sizeof(broadcast)) == -1);
        Require(*__error_nid_postfix() == Fault);

        *__error_nid_postfix() = 0;
        Require(_setsockopt_nid_postfix(socket, SolSocket, SoBroadcast, nullptr, sizeof(broadcast)) == -1);
        Require(*__error_nid_postfix() == Fault);

        *__error_nid_postfix() = 0;
        Require(setsockopt_nid_postfix(socket, SolSocket, SoBroadcast, nullptr, 1) == -1);
        Require(*__error_nid_postfix() == Fault);

        *__error_nid_postfix() = 0;
        Require(_setsockopt_nid_postfix(socket, SolSocket, SoBroadcast, nullptr, 1) == -1);
        Require(*__error_nid_postfix() == Fault);

        *__error_nid_postfix() = 0;
        Require(setsockopt_nid_postfix(socket, SolSocket, SoBroadcast, nullptr, 0) == -1);
        Require(*__error_nid_postfix() == Invalid);

        *__error_nid_postfix() = 0;
        Require(_setsockopt_nid_postfix(socket, SolSocket, SoBroadcast, nullptr, 0) == -1);
        Require(*__error_nid_postfix() == Invalid);

        *__error_nid_postfix() = 0;
        Require(getsockopt_nid_postfix(socket, SolSocket, SoBroadcast, &broadcast, nullptr) == -1);
        Require(*__error_nid_postfix() == Invalid);

        *__error_nid_postfix() = 0;
        Require(_getsockopt_nid_postfix(socket, SolSocket, SoBroadcast, &broadcast, nullptr) == -1);
        Require(*__error_nid_postfix() == Invalid);

        *__error_nid_postfix() = 0;
        Require(getsockopt_nid_postfix(socket, SolSocket, SoBroadcast, nullptr, nullptr) == -1);
        Require(*__error_nid_postfix() == Invalid);

        *__error_nid_postfix() = 0;
        Require(_getsockopt_nid_postfix(socket, SolSocket, SoBroadcast, nullptr, nullptr) == -1);
        Require(*__error_nid_postfix() == Invalid);
    }

    *__error_nid_postfix() = 0;
    Require(setsockopt_nid_postfix(datagram, IpLevel, IpMulticastInterface, nullptr, sizeof(address)) == -1);
    Require(*__error_nid_postfix() == Fault);

    *__error_nid_postfix() = 0;
    Require(_setsockopt_nid_postfix(datagram, IpLevel, IpMulticastInterface, nullptr, sizeof(address)) == -1);
    Require(*__error_nid_postfix() == Fault);

    *__error_nid_postfix() = 0;
    Require(setsockopt_nid_postfix(datagram, IpLevel, IpMulticastInterface, nullptr, 1) == -1);
    Require(*__error_nid_postfix() == Fault);

    *__error_nid_postfix() = 0;
    Require(_setsockopt_nid_postfix(datagram, IpLevel, IpMulticastInterface, nullptr, 1) == -1);
    Require(*__error_nid_postfix() == Fault);

    *__error_nid_postfix() = 0;
    Require(setsockopt_nid_postfix(datagram, IpLevel, IpMulticastInterface, nullptr, 0) == -1);
    Require(*__error_nid_postfix() == Invalid);

    *__error_nid_postfix() = 0;
    Require(_setsockopt_nid_postfix(datagram, IpLevel, IpMulticastInterface, nullptr, 0) == -1);
    Require(*__error_nid_postfix() == Invalid);

    *__error_nid_postfix() = 0;
    Require(getsockopt_nid_postfix(datagram, IpLevel, IpMulticastInterface, &address, nullptr) == -1);
    Require(*__error_nid_postfix() == Invalid);

    *__error_nid_postfix() = 0;
    Require(_getsockopt_nid_postfix(datagram, IpLevel, IpMulticastInterface, &address, nullptr) == -1);
    Require(*__error_nid_postfix() == Invalid);

    *__error_nid_postfix() = 0;
    Require(getsockopt_nid_postfix(datagram, IpLevel, IpMulticastInterface, nullptr, nullptr) == -1);
    Require(*__error_nid_postfix() == Invalid);

    *__error_nid_postfix() = 0;
    Require(_getsockopt_nid_postfix(datagram, IpLevel, IpMulticastInterface, nullptr, nullptr) == -1);
    Require(*__error_nid_postfix() == Invalid);

    *__error_nid_postfix() = 0;
    Require(setsockopt_nid_postfix(datagram, IpLevel, IpMulticastTimeToLive, nullptr, sizeof(ttl)) == -1);
    Require(*__error_nid_postfix() == Fault);

    *__error_nid_postfix() = 0;
    Require(_setsockopt_nid_postfix(datagram, IpLevel, IpMulticastTimeToLive, nullptr, sizeof(ttl)) == -1);
    Require(*__error_nid_postfix() == Fault);

    *__error_nid_postfix() = 0;
    Require(setsockopt_nid_postfix(datagram, IpLevel, IpMulticastTimeToLive, nullptr, 1) == -1);
    Require(*__error_nid_postfix() == Fault);

    *__error_nid_postfix() = 0;
    Require(_setsockopt_nid_postfix(datagram, IpLevel, IpMulticastTimeToLive, nullptr, 1) == -1);
    Require(*__error_nid_postfix() == Fault);

    *__error_nid_postfix() = 0;
    Require(setsockopt_nid_postfix(datagram, IpLevel, IpMulticastTimeToLive, nullptr, 0) == -1);
    Require(*__error_nid_postfix() == Invalid);

    *__error_nid_postfix() = 0;
    Require(_setsockopt_nid_postfix(datagram, IpLevel, IpMulticastTimeToLive, nullptr, 0) == -1);
    Require(*__error_nid_postfix() == Invalid);

    *__error_nid_postfix() = 0;
    Require(getsockopt_nid_postfix(datagram, IpLevel, IpMulticastTimeToLive, &ttl, nullptr) == -1);
    Require(*__error_nid_postfix() == Invalid);

    *__error_nid_postfix() = 0;
    Require(_getsockopt_nid_postfix(datagram, IpLevel, IpMulticastTimeToLive, &ttl, nullptr) == -1);
    Require(*__error_nid_postfix() == Invalid);

    *__error_nid_postfix() = 0;
    Require(getsockopt_nid_postfix(datagram, IpLevel, IpMulticastTimeToLive, nullptr, nullptr) == -1);
    Require(*__error_nid_postfix() == Invalid);

    *__error_nid_postfix() = 0;
    Require(_getsockopt_nid_postfix(datagram, IpLevel, IpMulticastTimeToLive, nullptr, nullptr) == -1);
    Require(*__error_nid_postfix() == Invalid);
}

void TestInvalidOptlenBounds(int datagram, int stream) {
    int broadcast = 1;
    std::uint32_t address = 0;
    int ttl = 5;

    for (const int socket : {datagram, stream}) {
        for (const std::uint32_t invalidLength : {0u, 1u, 2u, 3u}) {
            *__error_nid_postfix() = 0;
            Require(setsockopt_nid_postfix(socket, SolSocket, SoBroadcast, &broadcast, invalidLength) == -1);
            Require(*__error_nid_postfix() == Invalid);

            *__error_nid_postfix() = 0;
            Require(_setsockopt_nid_postfix(socket, SolSocket, SoBroadcast, &broadcast, invalidLength) == -1);
            Require(*__error_nid_postfix() == Invalid);
        }
    }

    for (const std::uint32_t invalidLength : {0u, 1u, 2u, 3u}) {
        *__error_nid_postfix() = 0;
        Require(setsockopt_nid_postfix(datagram, IpLevel, IpMulticastInterface, &address, invalidLength) == -1);
        Require(*__error_nid_postfix() == Invalid);

        *__error_nid_postfix() = 0;
        Require(_setsockopt_nid_postfix(datagram, IpLevel, IpMulticastInterface, &address, invalidLength) == -1);
        Require(*__error_nid_postfix() == Invalid);
    }

    for (const std::uint32_t invalidLength : {0u, 2u, 3u}) {
        *__error_nid_postfix() = 0;
        Require(setsockopt_nid_postfix(datagram, IpLevel, IpMulticastTimeToLive, &ttl, invalidLength) == -1);
        Require(*__error_nid_postfix() == Invalid);

        *__error_nid_postfix() = 0;
        Require(_setsockopt_nid_postfix(datagram, IpLevel, IpMulticastTimeToLive, &ttl, invalidLength) == -1);
        Require(*__error_nid_postfix() == Invalid);
    }
}

void TestInvalidMulticastTtlValues(int datagram) {
    for (const unsigned int invalidTtl : {256u, 300u, 1000u, 65535u}) {
        *__error_nid_postfix() = 0;
        Require(setsockopt_nid_postfix(datagram, IpLevel, IpMulticastTimeToLive, &invalidTtl, sizeof(invalidTtl)) == -1);
        Require(*__error_nid_postfix() == Invalid);

        *__error_nid_postfix() = 0;
        Require(_setsockopt_nid_postfix(datagram, IpLevel, IpMulticastTimeToLive, &invalidTtl, sizeof(invalidTtl)) == -1);
        Require(*__error_nid_postfix() == Invalid);
    }
}

void TestInvalidLevelAndOptionBounds(int datagram, int stream) {
    int broadcast = 1;
    std::uint32_t address = 0;
    int ttl = 5;
    std::uint32_t intLength = sizeof(broadcast);
    std::uint32_t addressLength = sizeof(address);

    for (const int socket : {datagram, stream}) {
        for (const int invalidLevel : {-1, 9999, 12345}) {
            *__error_nid_postfix() = 0;
            Require(setsockopt_nid_postfix(socket, invalidLevel, SoBroadcast, &broadcast, sizeof(broadcast)) == -1);
            Require(*__error_nid_postfix() == Invalid);

            *__error_nid_postfix() = 0;
            Require(_setsockopt_nid_postfix(socket, invalidLevel, SoBroadcast, &broadcast, sizeof(broadcast)) == -1);
            Require(*__error_nid_postfix() == Invalid);

            *__error_nid_postfix() = 0;
            Require(getsockopt_nid_postfix(socket, invalidLevel, SoBroadcast, &broadcast, &intLength) == -1);
            Require(*__error_nid_postfix() == Invalid);

            *__error_nid_postfix() = 0;
            Require(_getsockopt_nid_postfix(socket, invalidLevel, SoBroadcast, &broadcast, &intLength) == -1);
            Require(*__error_nid_postfix() == Invalid);

            *__error_nid_postfix() = 0;
            Require(setsockopt_nid_postfix(socket, invalidLevel, IpMulticastInterface, &address, sizeof(address)) == -1);
            Require(*__error_nid_postfix() == Invalid);

            *__error_nid_postfix() = 0;
            Require(_setsockopt_nid_postfix(socket, invalidLevel, IpMulticastInterface, &address, sizeof(address)) == -1);
            Require(*__error_nid_postfix() == Invalid);

            *__error_nid_postfix() = 0;
            Require(getsockopt_nid_postfix(socket, invalidLevel, IpMulticastInterface, &address, &addressLength) == -1);
            Require(*__error_nid_postfix() == Invalid);

            *__error_nid_postfix() = 0;
            Require(_getsockopt_nid_postfix(socket, invalidLevel, IpMulticastInterface, &address, &addressLength) == -1);
            Require(*__error_nid_postfix() == Invalid);

            *__error_nid_postfix() = 0;
            Require(setsockopt_nid_postfix(socket, invalidLevel, IpMulticastTimeToLive, &ttl, sizeof(ttl)) == -1);
            Require(*__error_nid_postfix() == Invalid);

            *__error_nid_postfix() = 0;
            Require(_setsockopt_nid_postfix(socket, invalidLevel, IpMulticastTimeToLive, &ttl, sizeof(ttl)) == -1);
            Require(*__error_nid_postfix() == Invalid);

            *__error_nid_postfix() = 0;
            Require(getsockopt_nid_postfix(socket, invalidLevel, IpMulticastTimeToLive, &ttl, &intLength) == -1);
            Require(*__error_nid_postfix() == Invalid);

            *__error_nid_postfix() = 0;
            Require(_getsockopt_nid_postfix(socket, invalidLevel, IpMulticastTimeToLive, &ttl, &intLength) == -1);
            Require(*__error_nid_postfix() == Invalid);
        }

        for (const int invalidOption : {0x7fff, 9999}) {
            *__error_nid_postfix() = 0;
            Require(setsockopt_nid_postfix(socket, SolSocket, invalidOption, &broadcast, sizeof(broadcast)) == -1);
            Require(*__error_nid_postfix() == NoProtocolOption);

            *__error_nid_postfix() = 0;
            Require(_setsockopt_nid_postfix(socket, SolSocket, invalidOption, &broadcast, sizeof(broadcast)) == -1);
            Require(*__error_nid_postfix() == NoProtocolOption);

            *__error_nid_postfix() = 0;
            Require(getsockopt_nid_postfix(socket, SolSocket, invalidOption, &broadcast, &intLength) == -1);
            Require(*__error_nid_postfix() == NoProtocolOption);

            *__error_nid_postfix() = 0;
            Require(_getsockopt_nid_postfix(socket, SolSocket, invalidOption, &broadcast, &intLength) == -1);
            Require(*__error_nid_postfix() == NoProtocolOption);

            *__error_nid_postfix() = 0;
            Require(setsockopt_nid_postfix(socket, IpLevel, invalidOption, &broadcast, sizeof(broadcast)) == -1);
            Require(*__error_nid_postfix() == NoProtocolOption);

            *__error_nid_postfix() = 0;
            Require(_setsockopt_nid_postfix(socket, IpLevel, invalidOption, &broadcast, sizeof(broadcast)) == -1);
            Require(*__error_nid_postfix() == NoProtocolOption);

            *__error_nid_postfix() = 0;
            Require(getsockopt_nid_postfix(socket, IpLevel, invalidOption, &broadcast, &intLength) == -1);
            Require(*__error_nid_postfix() == NoProtocolOption);

            *__error_nid_postfix() = 0;
            Require(_getsockopt_nid_postfix(socket, IpLevel, invalidOption, &broadcast, &intLength) == -1);
            Require(*__error_nid_postfix() == NoProtocolOption);
        }
    }
}

void TestSocketOptionRoundtrips(int datagram, int stream) {
    for (const int socket : {datagram, stream}) {
        int broadcastValue = 1;
        Require(setsockopt_nid_postfix(socket, SolSocket, SoBroadcast, &broadcastValue, sizeof(broadcastValue)) == 0);
        int readBroadcast = 0;
        std::uint32_t length = sizeof(readBroadcast);
        Require(getsockopt_nid_postfix(socket, SolSocket, SoBroadcast, &readBroadcast, &length) == 0);
        Require(length == sizeof(readBroadcast));
        Require(readBroadcast == SoBroadcast);

        broadcastValue = 0;
        Require(_setsockopt_nid_postfix(socket, SolSocket, SoBroadcast, &broadcastValue, sizeof(broadcastValue)) == 0);
        Require(_getsockopt_nid_postfix(socket, SolSocket, SoBroadcast, &readBroadcast, &length) == 0);
        Require(readBroadcast == 0);
    }

    int setTtl = 10;
    Require(setsockopt_nid_postfix(datagram, IpLevel, IpMulticastTimeToLive, &setTtl, sizeof(setTtl)) == 0);
    int readTtl = 0;
    std::uint32_t length = sizeof(readTtl);
    Require(getsockopt_nid_postfix(datagram, IpLevel, IpMulticastTimeToLive, &readTtl, &length) == 0);
    Require(length == sizeof(readTtl));
    Require(readTtl == 10);

    unsigned char byteTtl = 5;
    Require(_setsockopt_nid_postfix(datagram, IpLevel, IpMulticastTimeToLive, &byteTtl, 1) == 0);
    unsigned char readByteTtl = 0;
    std::uint32_t byteLength = 1;
    Require(_getsockopt_nid_postfix(datagram, IpLevel, IpMulticastTimeToLive, &readByteTtl, &byteLength) == 0);
    Require(byteLength == 1);
    Require(readByteTtl == 5);

    std::uint32_t address = 0;
    Require(setsockopt_nid_postfix(datagram, IpLevel, IpMulticastInterface, &address, sizeof(address)) == 0);
    std::uint32_t readAddress = 0xffffffff;
    length = sizeof(readAddress);
    Require(getsockopt_nid_postfix(datagram, IpLevel, IpMulticastInterface, &readAddress, &length) == 0);
    Require(length == sizeof(readAddress));
    Require(readAddress == 0);
}

}

int main() {
    const int datagram = socket_nid_postfix(2, Datagram, 0);
    Require(datagram >= 0);
    const int stream = socket_nid_postfix(2, Stream, 0);
    Require(stream >= 0);

    TestBadDescriptorBounds();
    TestStreamMulticastUnsupported(stream);
    TestNullOptvalBounds(datagram, stream);
    TestInvalidOptlenBounds(datagram, stream);
    TestInvalidMulticastTtlValues(datagram);
    TestInvalidLevelAndOptionBounds(datagram, stream);
    TestSocketOptionRoundtrips(datagram, stream);

    Require(close_nid_postfix(datagram) == 0);
    Require(close_nid_postfix(stream) == 0);
    return 0;
}
