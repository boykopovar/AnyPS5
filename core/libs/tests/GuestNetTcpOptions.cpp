#include <algorithm>
#include <array>
#include <climits>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include "prx/libSceNet/libSceNet.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"

constexpr int TcpLevel = 6;
constexpr int TcpMaximumSegment = 2;
constexpr int TcpNoPush = 4;
constexpr int TcpNoOptions = 8;
constexpr int TcpOption80 = 0x80;
constexpr int TcpKeepIdle = 0x100;
constexpr int TcpKeepInterval = 0x200;
constexpr int TcpKeepCount = 0x400;
constexpr int DefaultIpv4MaximumSegment = 536;
constexpr int DefaultIpv6MaximumSegment = 1220;
constexpr int MinimumMaximumSegmentInput = 176;
constexpr int MinimumMaximumSegment = 216;
constexpr int DefaultKeepIdle = 7200;
constexpr int DefaultKeepInterval = 75;
constexpr int DefaultKeepCount = 8;
constexpr int MaximumKeepSeconds = static_cast<int>(UINT32_MAX / 1000u);
constexpr int BadFileDescriptor = 9;
constexpr int Fault = 14;
constexpr int Invalid = 22;
constexpr int NoProtocolOption = 42;
constexpr int NetErrorBase = static_cast<int>(0x80410100u);

static void Require(bool condition, std::source_location where = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "check failed at line %u, net errno %d, errno %d\n",
            where.line(), *sceNetErrnoLoc(), *__error_nid_postfix());
        std::abort();
    }
}

static bool NetFails(int result, int error) {
    return result == (NetErrorBase | error) && *sceNetErrnoLoc() == error && *__error_nid_postfix() == error;
}

static bool KernelFails(int result, int error) {
    return result == -1 && *__error_nid_postfix() == error;
}

static bool SetNet(int socket, int option, int value) {
    return sceNetSetsockopt(socket, TcpLevel, option, &value, sizeof(value)) == 0;
}

static bool SetKernel(int socket, int option, int value) {
    return setsockopt_nid_postfix(socket, TcpLevel, option, &value, sizeof(value)) == 0;
}

static void CheckOption(int socket, int option, int expected) {
    for (const std::uint32_t capacity : {0u, 1u, 2u, 3u, 4u, 8u}) {
        std::array<std::uint8_t, 8> bytes;
        bytes.fill(0xa5);
        auto size = capacity;
        Require(sceNetGetsockopt(socket, TcpLevel, option, bytes.data(), &size) == 0);
        Require(size == std::min(capacity, 4u));
        Require(std::memcmp(bytes.data(), &expected, size) == 0);
        Require(std::all_of(bytes.begin() + size, bytes.end(), [](auto value) { return value == 0xa5; }));
    }
}

static void CheckState(int socket, int maxseg, int keepIdle, int keepInterval, int keepCount) {
    CheckOption(socket, TcpMaximumSegment, maxseg);
    CheckOption(socket, TcpKeepIdle, keepIdle);
    CheckOption(socket, TcpKeepInterval, keepInterval);
    CheckOption(socket, TcpKeepCount, keepCount);
}

static void CheckGetFailure(int socket, int option, int smallError, int fullError) {
    for (const std::uint32_t capacity : {0u, 1u, 2u, 3u, 4u, 8u}) {
        std::array<std::uint8_t, 8> bytes;
        bytes.fill(0xa5);
        auto size = capacity;
        const int expectedError = capacity < 4u ? smallError : fullError;
        Require(NetFails(sceNetGetsockopt(socket, TcpLevel, option, bytes.data(), &size), expectedError));
        Require(size == capacity);
        Require(std::all_of(bytes.begin(), bytes.end(), [](auto value) { return value == 0xa5; }));
    }
}

int main() {
    Require(sceNetInit_nid_postfix() == 0);
    for (const int family : {2, 28}) {
        const int defaultMaximumSegment = family == 28 ? DefaultIpv6MaximumSegment : DefaultIpv4MaximumSegment;
        for (const int protocol : {0, 6}) {
            const int socket = sceNetSocket("tcp-options", family, 1, protocol);
            const int other = sceNetSocket("tcp-other", family, 1, protocol == 0 ? 6 : 0);
            const int datagram = sceNetSocket("tcp-datagram", family, 2, 0);
            const int datagramProto = sceNetSocket("tcp-datagram-proto", family, 2, 17);
            Require(socket >= 0 && other >= 0 && datagram >= 0 && datagramProto >= 0);
            CheckState(socket, defaultMaximumSegment, DefaultKeepIdle, DefaultKeepInterval, DefaultKeepCount);
            CheckState(other, defaultMaximumSegment, DefaultKeepIdle, DefaultKeepInterval, DefaultKeepCount);

            for (const int option : {TcpMaximumSegment, TcpKeepIdle, TcpKeepInterval, TcpKeepCount}) {
                std::uint32_t size = 4;
                std::array<std::uint8_t, 8> bytes;
                bytes.fill(0xa5);
                Require(NetFails(sceNetGetsockopt(socket, TcpLevel, option, nullptr, &size), Invalid) && size == 4);
                Require(NetFails(sceNetGetsockopt(socket, TcpLevel, option, bytes.data(), nullptr), Invalid));
                Require(std::all_of(bytes.begin(), bytes.end(), [](auto value) { return value == 0xa5; }));
                Require(NetFails(sceNetSetsockopt(socket, TcpLevel, option, nullptr, sizeof(int)), Invalid));
                Require(NetFails(sceNetSetsockopt(socket, TcpLevel, option, nullptr, 0), Invalid));
                Require(KernelFails(setsockopt_nid_postfix(socket, TcpLevel, option, nullptr, sizeof(int)), Fault));
                Require(KernelFails(setsockopt_nid_postfix(socket, TcpLevel, option, nullptr, 0), Invalid) && *sceNetErrnoLoc() == Invalid);
                const int validValue = option == TcpMaximumSegment ? 400 : 30;
                for (const std::uint32_t optlen : {0u, 1u, 2u, 3u}) {
                    Require(NetFails(sceNetSetsockopt(socket, TcpLevel, option, &validValue, optlen), Invalid));
                    Require(KernelFails(setsockopt_nid_postfix(socket, TcpLevel, option, &validValue, optlen), Invalid) && *sceNetErrnoLoc() == Invalid);
                }
                for (const int udp : {datagram, datagramProto}) {
                    Require(NetFails(sceNetSetsockopt(udp, TcpLevel, option, &validValue, sizeof(validValue)), Invalid));
                    Require(KernelFails(setsockopt_nid_postfix(udp, TcpLevel, option, &validValue, sizeof(validValue)), Invalid) && *sceNetErrnoLoc() == Invalid);
                    CheckGetFailure(udp, option, Invalid, Invalid);
                }
            }
            CheckState(socket, defaultMaximumSegment, DefaultKeepIdle, DefaultKeepInterval, DefaultKeepCount);

            for (const int unsupported : {TcpNoPush, TcpNoOptions, TcpOption80, 0x7777, 0x7fffffff}) {
                const int one = 1;
                Require(NetFails(sceNetSetsockopt(socket, TcpLevel, unsupported, &one, sizeof(one)), NoProtocolOption));
                Require(KernelFails(setsockopt_nid_postfix(socket, TcpLevel, unsupported, &one, sizeof(one)), NoProtocolOption) &&
                    *sceNetErrnoLoc() == NoProtocolOption);
                CheckGetFailure(socket, unsupported, Invalid, NoProtocolOption);
            }

            Require(SetNet(socket, TcpKeepIdle, 1));
            CheckOption(socket, TcpKeepIdle, 1);
            Require(SetNet(socket, TcpKeepIdle, MaximumKeepSeconds));
            CheckOption(socket, TcpKeepIdle, MaximumKeepSeconds);
            const std::array<int, 2> idleWords{60, -1};
            for (const std::uint32_t optlen : {5u, 6u, 7u, 8u}) {
                Require(sceNetSetsockopt(socket, TcpLevel, TcpKeepIdle, idleWords.data(), optlen) == 0);
                CheckOption(socket, TcpKeepIdle, 60);
            }
            const std::array<int, 2> badIdleWords{-1, 60};
            Require(NetFails(sceNetSetsockopt(socket, TcpLevel, TcpKeepIdle, badIdleWords.data(), sizeof(badIdleWords)), Invalid));
            CheckOption(socket, TcpKeepIdle, 60);
            for (const int invalid : {MaximumKeepSeconds + 1, INT_MAX, -1, INT_MIN}) {
                Require(NetFails(sceNetSetsockopt(socket, TcpLevel, TcpKeepIdle, &invalid, sizeof(invalid)), Invalid));
                Require(KernelFails(setsockopt_nid_postfix(socket, TcpLevel, TcpKeepIdle, &invalid, sizeof(invalid)), Invalid) && *sceNetErrnoLoc() == Invalid);
                CheckOption(socket, TcpKeepIdle, 60);
            }
            Require(SetNet(socket, TcpKeepIdle, 0));
            CheckOption(socket, TcpKeepIdle, DefaultKeepIdle);
            Require(SetKernel(socket, TcpKeepIdle, 180));
            CheckOption(socket, TcpKeepIdle, 180);
            Require(SetKernel(socket, TcpKeepIdle, 0));
            CheckOption(socket, TcpKeepIdle, DefaultKeepIdle);

            Require(SetNet(socket, TcpKeepInterval, 1));
            CheckOption(socket, TcpKeepInterval, 1);
            Require(SetNet(socket, TcpKeepInterval, MaximumKeepSeconds));
            CheckOption(socket, TcpKeepInterval, MaximumKeepSeconds);
            const std::array<int, 2> intervalWords{10, -1};
            for (const std::uint32_t optlen : {5u, 6u, 7u, 8u}) {
                Require(sceNetSetsockopt(socket, TcpLevel, TcpKeepInterval, intervalWords.data(), optlen) == 0);
                CheckOption(socket, TcpKeepInterval, 10);
            }
            const std::array<int, 2> badIntervalWords{MaximumKeepSeconds + 1, 10};
            Require(NetFails(sceNetSetsockopt(socket, TcpLevel, TcpKeepInterval, badIntervalWords.data(), sizeof(badIntervalWords)), Invalid));
            CheckOption(socket, TcpKeepInterval, 10);
            for (const int invalid : {MaximumKeepSeconds + 1, INT_MAX, -1, INT_MIN}) {
                Require(NetFails(sceNetSetsockopt(socket, TcpLevel, TcpKeepInterval, &invalid, sizeof(invalid)), Invalid));
                Require(KernelFails(setsockopt_nid_postfix(socket, TcpLevel, TcpKeepInterval, &invalid, sizeof(invalid)), Invalid) && *sceNetErrnoLoc() == Invalid);
                CheckOption(socket, TcpKeepInterval, 10);
            }
            Require(SetNet(socket, TcpKeepInterval, 0));
            CheckOption(socket, TcpKeepInterval, DefaultKeepInterval);
            Require(SetKernel(socket, TcpKeepInterval, 25));
            CheckOption(socket, TcpKeepInterval, 25);
            Require(SetKernel(socket, TcpKeepInterval, 0));
            CheckOption(socket, TcpKeepInterval, DefaultKeepInterval);

            for (const int count : {1, 3, INT_MAX, INT_MIN, -2, -1, 0x12345678, static_cast<int>(0x80000001u)}) {
                Require(SetNet(socket, TcpKeepCount, count));
                CheckOption(socket, TcpKeepCount, count);
            }
            const std::array<int, 2> countWords{4, 99};
            for (const std::uint32_t optlen : {5u, 6u, 7u, 8u}) {
                Require(sceNetSetsockopt(socket, TcpLevel, TcpKeepCount, countWords.data(), optlen) == 0);
                CheckOption(socket, TcpKeepCount, 4);
            }
            Require(SetNet(socket, TcpKeepCount, 0));
            CheckOption(socket, TcpKeepCount, DefaultKeepCount);
            Require(SetKernel(socket, TcpKeepCount, -1));
            CheckOption(socket, TcpKeepCount, -1);
            Require(SetKernel(socket, TcpKeepCount, 0));
            CheckOption(socket, TcpKeepCount, DefaultKeepCount);

            for (const int candidate : {175, MinimumMaximumSegmentInput, 200, MinimumMaximumSegment, 400,
                     DefaultIpv4MaximumSegment, DefaultIpv4MaximumSegment + 1, DefaultIpv6MaximumSegment,
                     DefaultIpv6MaximumSegment + 1, 1460, INT_MAX, 1, 0, -1, INT_MIN}) {
                Require(NetFails(sceNetSetsockopt(socket, TcpLevel, TcpMaximumSegment, &candidate, sizeof(candidate)), Invalid));
                Require(KernelFails(setsockopt_nid_postfix(socket, TcpLevel, TcpMaximumSegment, &candidate, sizeof(candidate)), Invalid) &&
                    *sceNetErrnoLoc() == Invalid);
                CheckOption(socket, TcpMaximumSegment, defaultMaximumSegment);
            }
            const std::array<int, 2> freshMssWords{400, 175};
            for (const std::uint32_t optlen : {5u, 6u, 7u, 8u}) {
                Require(NetFails(sceNetSetsockopt(socket, TcpLevel, TcpMaximumSegment, freshMssWords.data(), optlen), Invalid));
                Require(KernelFails(setsockopt_nid_postfix(socket, TcpLevel, TcpMaximumSegment, freshMssWords.data(), optlen), Invalid) &&
                    *sceNetErrnoLoc() == Invalid);
                CheckOption(socket, TcpMaximumSegment, defaultMaximumSegment);
            }

            CheckState(other, defaultMaximumSegment, DefaultKeepIdle, DefaultKeepInterval, DefaultKeepCount);
            for (const int descriptor : {socket, other, datagram, datagramProto}) Require(sceNetSocketClose(descriptor) == 0);
            for (const int option : {TcpMaximumSegment, TcpKeepIdle, TcpKeepInterval, TcpKeepCount}) {
                const int value = 200;
                Require(NetFails(sceNetSetsockopt(socket, TcpLevel, option, &value, sizeof(value)), BadFileDescriptor));
                Require(KernelFails(setsockopt_nid_postfix(socket, TcpLevel, option, &value, sizeof(value)), BadFileDescriptor));
                CheckGetFailure(socket, option, BadFileDescriptor, BadFileDescriptor);
            }
        }

        const int listener = sceNetSocket("tcp-listener", family, 1, 0);
        const int firstClient = sceNetSocket("tcp-client-1", family, 1, 6);
        const int secondClient = sceNetSocket("tcp-client-2", family, 1, 0);
        Require(listener >= 0 && firstClient >= 0 && secondClient >= 0);
        std::array<std::uint8_t, 28> address{};
        std::uint32_t addressSize = family == 2 ? 16 : 28;
        address[0] = addressSize;
        address[1] = family;
        if (family == 2) {
            address[4] = 127;
            address[7] = 1;
        } else {
            address[23] = 1;
        }
        Require(sceNetBind_nid_postfix(listener, address.data(), addressSize) == 0);
        for (const int candidate : {MinimumMaximumSegmentInput, 200, 300, defaultMaximumSegment}) {
            Require(NetFails(sceNetSetsockopt(listener, TcpLevel, TcpMaximumSegment, &candidate, sizeof(candidate)), Invalid));
            Require(KernelFails(setsockopt_nid_postfix(listener, TcpLevel, TcpMaximumSegment, &candidate, sizeof(candidate)), Invalid) &&
                *sceNetErrnoLoc() == Invalid);
            CheckOption(listener, TcpMaximumSegment, defaultMaximumSegment);
        }
        Require(sceNetListen(listener, 2) == 0);
        for (const int candidate : {MinimumMaximumSegmentInput, 200, 300, defaultMaximumSegment}) {
            Require(NetFails(sceNetSetsockopt(listener, TcpLevel, TcpMaximumSegment, &candidate, sizeof(candidate)), Invalid));
            Require(KernelFails(setsockopt_nid_postfix(listener, TcpLevel, TcpMaximumSegment, &candidate, sizeof(candidate)), Invalid) &&
                *sceNetErrnoLoc() == Invalid);
            CheckOption(listener, TcpMaximumSegment, defaultMaximumSegment);
        }
        Require(sceNetGetsockname(listener, address.data(), &addressSize) == 0);
        Require(sceNetConnect(firstClient, address.data(), addressSize) == 0);
        const int firstAccepted = sceNetAccept(listener, nullptr, nullptr);
        Require(firstAccepted >= 0);
        CheckState(firstClient, defaultMaximumSegment, DefaultKeepIdle, DefaultKeepInterval, DefaultKeepCount);
        CheckState(firstAccepted, defaultMaximumSegment, DefaultKeepIdle, DefaultKeepInterval, DefaultKeepCount);

        const int belowMinimum = MinimumMaximumSegmentInput - 1;
        Require(NetFails(sceNetSetsockopt(firstAccepted, TcpLevel, TcpMaximumSegment, &belowMinimum, sizeof(belowMinimum)), Invalid));
        Require(KernelFails(setsockopt_nid_postfix(firstAccepted, TcpLevel, TcpMaximumSegment, &belowMinimum, sizeof(belowMinimum)), Invalid) &&
            *sceNetErrnoLoc() == Invalid);
        CheckOption(firstAccepted, TcpMaximumSegment, defaultMaximumSegment);
        Require(SetNet(firstAccepted, TcpMaximumSegment, MinimumMaximumSegmentInput));
        CheckOption(firstAccepted, TcpMaximumSegment, MinimumMaximumSegment);
        Require(SetNet(firstAccepted, TcpMaximumSegment, 200));
        CheckOption(firstAccepted, TcpMaximumSegment, MinimumMaximumSegment);
        for (const int invalid : {MinimumMaximumSegment + 1, 400, DefaultIpv4MaximumSegment, DefaultIpv4MaximumSegment + 1,
                 DefaultIpv6MaximumSegment, 1460, belowMinimum, 0, -1}) {
            Require(NetFails(sceNetSetsockopt(firstAccepted, TcpLevel, TcpMaximumSegment, &invalid, sizeof(invalid)), Invalid));
            Require(KernelFails(setsockopt_nid_postfix(firstAccepted, TcpLevel, TcpMaximumSegment, &invalid, sizeof(invalid)), Invalid) &&
                *sceNetErrnoLoc() == Invalid);
            CheckOption(firstAccepted, TcpMaximumSegment, MinimumMaximumSegment);
        }
        CheckState(firstClient, defaultMaximumSegment, DefaultKeepIdle, DefaultKeepInterval, DefaultKeepCount);

        for (const int invalid : {defaultMaximumSegment + 1, 1460, INT_MAX, belowMinimum, 1, 0, -1, INT_MIN}) {
            Require(NetFails(sceNetSetsockopt(firstClient, TcpLevel, TcpMaximumSegment, &invalid, sizeof(invalid)), Invalid));
            Require(KernelFails(setsockopt_nid_postfix(firstClient, TcpLevel, TcpMaximumSegment, &invalid, sizeof(invalid)), Invalid) &&
                *sceNetErrnoLoc() == Invalid);
            CheckOption(firstClient, TcpMaximumSegment, defaultMaximumSegment);
        }
        Require(SetNet(firstClient, TcpMaximumSegment, defaultMaximumSegment));
        CheckOption(firstClient, TcpMaximumSegment, defaultMaximumSegment);
        const std::array<int, 2> mssWords{400, 175};
        for (const std::uint32_t optlen : {5u, 6u, 7u, 8u}) {
            Require(sceNetSetsockopt(firstClient, TcpLevel, TcpMaximumSegment, mssWords.data(), optlen) == 0);
            CheckOption(firstClient, TcpMaximumSegment, 400);
        }
        const std::array<int, 2> badMssWords{401, 300};
        Require(NetFails(sceNetSetsockopt(firstClient, TcpLevel, TcpMaximumSegment, badMssWords.data(), sizeof(badMssWords)), Invalid));
        CheckOption(firstClient, TcpMaximumSegment, 400);
        Require(SetNet(firstClient, TcpMaximumSegment, 300));
        CheckOption(firstClient, TcpMaximumSegment, 300);
        for (const int invalid : {301, 400, DefaultIpv4MaximumSegment, DefaultIpv4MaximumSegment + 1, 1460, belowMinimum, 0, -1}) {
            Require(NetFails(sceNetSetsockopt(firstClient, TcpLevel, TcpMaximumSegment, &invalid, sizeof(invalid)), Invalid));
            Require(KernelFails(setsockopt_nid_postfix(firstClient, TcpLevel, TcpMaximumSegment, &invalid, sizeof(invalid)), Invalid) &&
                *sceNetErrnoLoc() == Invalid);
            CheckOption(firstClient, TcpMaximumSegment, 300);
        }
        Require(SetKernel(firstClient, TcpMaximumSegment, 220));
        CheckOption(firstClient, TcpMaximumSegment, 220);
        Require(KernelFails(setsockopt_nid_postfix(firstClient, TcpLevel, TcpMaximumSegment, &mssWords[0], sizeof(mssWords[0])), Invalid) &&
            *sceNetErrnoLoc() == Invalid);
        CheckOption(firstClient, TcpMaximumSegment, 220);
        Require(SetKernel(firstClient, TcpMaximumSegment, MinimumMaximumSegmentInput));
        CheckOption(firstClient, TcpMaximumSegment, MinimumMaximumSegment);
        Require(SetKernel(firstClient, TcpMaximumSegment, 200));
        CheckOption(firstClient, TcpMaximumSegment, MinimumMaximumSegment);
        for (const int invalid : {MinimumMaximumSegment + 1, 220, 300, DefaultIpv4MaximumSegment, belowMinimum, 0, -1}) {
            Require(NetFails(sceNetSetsockopt(firstClient, TcpLevel, TcpMaximumSegment, &invalid, sizeof(invalid)), Invalid));
            Require(KernelFails(setsockopt_nid_postfix(firstClient, TcpLevel, TcpMaximumSegment, &invalid, sizeof(invalid)), Invalid) &&
                *sceNetErrnoLoc() == Invalid);
            CheckOption(firstClient, TcpMaximumSegment, MinimumMaximumSegment);
        }

        const int listenerMss = 300;
        Require(NetFails(sceNetSetsockopt(listener, TcpLevel, TcpMaximumSegment, &listenerMss, sizeof(listenerMss)), Invalid));
        Require(SetNet(listener, TcpKeepIdle, 120));
        Require(SetKernel(listener, TcpKeepInterval, 15));
        Require(SetNet(listener, TcpKeepCount, -1));
        CheckState(listener, defaultMaximumSegment, 120, 15, -1);
        CheckState(firstAccepted, MinimumMaximumSegment, DefaultKeepIdle, DefaultKeepInterval, DefaultKeepCount);
        CheckState(firstClient, MinimumMaximumSegment, DefaultKeepIdle, DefaultKeepInterval, DefaultKeepCount);

        Require(sceNetConnect(secondClient, address.data(), addressSize) == 0);
        const int secondAccepted = sceNetAccept(listener, nullptr, nullptr);
        Require(secondAccepted >= 0);
        CheckState(secondAccepted, defaultMaximumSegment, 120, 15, -1);
        CheckState(secondClient, defaultMaximumSegment, DefaultKeepIdle, DefaultKeepInterval, DefaultKeepCount);

        Require(SetNet(listener, TcpKeepIdle, 0));
        Require(SetNet(listener, TcpKeepInterval, 25));
        Require(SetNet(listener, TcpKeepCount, 5));
        CheckState(listener, defaultMaximumSegment, DefaultKeepIdle, 25, 5);
        CheckState(secondAccepted, defaultMaximumSegment, 120, 15, -1);

        Require(SetNet(secondAccepted, TcpMaximumSegment, 400));
        Require(SetNet(secondAccepted, TcpKeepIdle, 300));
        Require(SetNet(secondAccepted, TcpKeepInterval, 0));
        Require(SetNet(secondAccepted, TcpKeepCount, 0));
        CheckState(secondAccepted, 400, 300, DefaultKeepInterval, DefaultKeepCount);
        CheckState(secondClient, defaultMaximumSegment, DefaultKeepIdle, DefaultKeepInterval, DefaultKeepCount);
        CheckState(listener, defaultMaximumSegment, DefaultKeepIdle, 25, 5);
        CheckState(firstAccepted, MinimumMaximumSegment, DefaultKeepIdle, DefaultKeepInterval, DefaultKeepCount);
        CheckState(firstClient, MinimumMaximumSegment, DefaultKeepIdle, DefaultKeepInterval, DefaultKeepCount);

        for (const int descriptor : {secondAccepted, firstAccepted, secondClient, firstClient, listener}) {
            Require(sceNetSocketClose(descriptor) == 0);
        }
    }
}
