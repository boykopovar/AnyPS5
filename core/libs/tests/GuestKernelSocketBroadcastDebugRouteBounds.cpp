#include "prx/libc/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace {

constexpr int AfInet = 2;
constexpr int AfInet6 = 28;

constexpr int SockStream = 1;
constexpr int SockDgram = 2;

constexpr int SolSocket = 0xffff;

constexpr int SoDebug = 0x1;
constexpr int SoDontRoute = 0x10;
constexpr int SoBroadcast = 0x20;

constexpr int PosixEbadf = 9;
constexpr int PosixEfault = 14;
constexpr int PosixEinval = 22;
constexpr int PosixEnotsock = 38;
constexpr int PosixEnoprotoopt = 42;

#ifdef _WIN32
constexpr bool StreamRefusesBroadcast = true;
#else
constexpr bool StreamRefusesBroadcast = false;
#endif

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

void TestErrorPrecedenceAndDescriptorValidation() {
    int pipeFds[2]{-1, -1};
    Require(pipe_nid_postfix(pipeFds) == 0);

    const int sock = socket_nid_postfix(AfInet, SockStream, 0);
    Require(sock >= 0);

    const int validValue = 1;
    int readValue = 0;
    std::uint32_t len = sizeof(readValue);

    for (const int opt : {SoDebug, SoDontRoute, SoBroadcast}) {
        Require(setsockopt_nid_postfix(-1, SolSocket, opt, nullptr, sizeof(int)) == -1 && *__error_nid_postfix() == PosixEfault);
        Require(_setsockopt_nid_postfix(-1, SolSocket, opt, nullptr, sizeof(int)) == -1 && *__error_nid_postfix() == PosixEfault);
        Require(setsockopt_nid_postfix(99999, SolSocket, opt, nullptr, sizeof(int)) == -1 && *__error_nid_postfix() == PosixEfault);
        Require(_setsockopt_nid_postfix(99999, SolSocket, opt, nullptr, sizeof(int)) == -1 && *__error_nid_postfix() == PosixEfault);
        Require(setsockopt_nid_postfix(pipeFds[0], SolSocket, opt, nullptr, sizeof(int)) == -1 && *__error_nid_postfix() == PosixEfault);
        Require(_setsockopt_nid_postfix(pipeFds[0], SolSocket, opt, nullptr, sizeof(int)) == -1 && *__error_nid_postfix() == PosixEfault);
        Require(setsockopt_nid_postfix(sock, SolSocket, opt, nullptr, sizeof(int)) == -1 && *__error_nid_postfix() == PosixEfault);
        Require(_setsockopt_nid_postfix(sock, SolSocket, opt, nullptr, sizeof(int)) == -1 && *__error_nid_postfix() == PosixEfault);

        Require(getsockopt_nid_postfix(-1, SolSocket, opt, &readValue, nullptr) == -1 && *__error_nid_postfix() == PosixEfault);
        Require(_getsockopt_nid_postfix(-1, SolSocket, opt, &readValue, nullptr) == -1 && *__error_nid_postfix() == PosixEfault);
        Require(getsockopt_nid_postfix(99999, SolSocket, opt, &readValue, nullptr) == -1 && *__error_nid_postfix() == PosixEfault);
        Require(_getsockopt_nid_postfix(99999, SolSocket, opt, &readValue, nullptr) == -1 && *__error_nid_postfix() == PosixEfault);
        Require(getsockopt_nid_postfix(pipeFds[0], SolSocket, opt, &readValue, nullptr) == -1 && *__error_nid_postfix() == PosixEfault);
        Require(_getsockopt_nid_postfix(pipeFds[0], SolSocket, opt, &readValue, nullptr) == -1 && *__error_nid_postfix() == PosixEfault);
        Require(getsockopt_nid_postfix(sock, SolSocket, opt, &readValue, nullptr) == -1 && *__error_nid_postfix() == PosixEfault);
        Require(_getsockopt_nid_postfix(sock, SolSocket, opt, &readValue, nullptr) == -1 && *__error_nid_postfix() == PosixEfault);

        const std::uint32_t negativeLen = 0x80000000u;
        Require(setsockopt_nid_postfix(-1, SolSocket, opt, &validValue, negativeLen) == -1 && *__error_nid_postfix() == PosixEinval);
        Require(_setsockopt_nid_postfix(-1, SolSocket, opt, &validValue, negativeLen) == -1 && *__error_nid_postfix() == PosixEinval);
        Require(setsockopt_nid_postfix(99999, SolSocket, opt, &validValue, negativeLen) == -1 && *__error_nid_postfix() == PosixEinval);
        Require(_setsockopt_nid_postfix(99999, SolSocket, opt, &validValue, negativeLen) == -1 && *__error_nid_postfix() == PosixEinval);
        Require(setsockopt_nid_postfix(pipeFds[0], SolSocket, opt, &validValue, negativeLen) == -1 && *__error_nid_postfix() == PosixEinval);
        Require(_setsockopt_nid_postfix(pipeFds[0], SolSocket, opt, &validValue, negativeLen) == -1 && *__error_nid_postfix() == PosixEinval);
        Require(setsockopt_nid_postfix(sock, SolSocket, opt, &validValue, negativeLen) == -1 && *__error_nid_postfix() == PosixEinval);
        Require(_setsockopt_nid_postfix(sock, SolSocket, opt, &validValue, negativeLen) == -1 && *__error_nid_postfix() == PosixEinval);

        len = sizeof(readValue);
        Require(setsockopt_nid_postfix(-1, SolSocket, opt, &validValue, sizeof(validValue)) == -1 && *__error_nid_postfix() == PosixEbadf);
        Require(_setsockopt_nid_postfix(-1, SolSocket, opt, &validValue, sizeof(validValue)) == -1 && *__error_nid_postfix() == PosixEbadf);
        Require(getsockopt_nid_postfix(-1, SolSocket, opt, &readValue, &len) == -1 && *__error_nid_postfix() == PosixEbadf);
        Require(_getsockopt_nid_postfix(-1, SolSocket, opt, &readValue, &len) == -1 && *__error_nid_postfix() == PosixEbadf);

        len = sizeof(readValue);
        Require(setsockopt_nid_postfix(99999, SolSocket, opt, &validValue, sizeof(validValue)) == -1 && *__error_nid_postfix() == PosixEbadf);
        Require(_setsockopt_nid_postfix(99999, SolSocket, opt, &validValue, sizeof(validValue)) == -1 && *__error_nid_postfix() == PosixEbadf);
        Require(getsockopt_nid_postfix(99999, SolSocket, opt, &readValue, &len) == -1 && *__error_nid_postfix() == PosixEbadf);
        Require(_getsockopt_nid_postfix(99999, SolSocket, opt, &readValue, &len) == -1 && *__error_nid_postfix() == PosixEbadf);

        len = sizeof(readValue);
        Require(setsockopt_nid_postfix(pipeFds[0], SolSocket, opt, &validValue, sizeof(validValue)) == -1 && *__error_nid_postfix() == PosixEnotsock);
        Require(_setsockopt_nid_postfix(pipeFds[0], SolSocket, opt, &validValue, sizeof(validValue)) == -1 && *__error_nid_postfix() == PosixEnotsock);
        Require(getsockopt_nid_postfix(pipeFds[0], SolSocket, opt, &readValue, &len) == -1 && *__error_nid_postfix() == PosixEnotsock);
        Require(_getsockopt_nid_postfix(pipeFds[0], SolSocket, opt, &readValue, &len) == -1 && *__error_nid_postfix() == PosixEnotsock);

        len = sizeof(readValue);
        Require(setsockopt_nid_postfix(sock, 12345, opt, &validValue, sizeof(validValue)) == -1 && *__error_nid_postfix() == PosixEinval);
        Require(_setsockopt_nid_postfix(sock, 12345, opt, &validValue, sizeof(validValue)) == -1 && *__error_nid_postfix() == PosixEinval);
        Require(getsockopt_nid_postfix(sock, 12345, opt, &readValue, &len) == -1 && *__error_nid_postfix() == PosixEinval);
        Require(_getsockopt_nid_postfix(sock, 12345, opt, &readValue, &len) == -1 && *__error_nid_postfix() == PosixEinval);
    }

    len = sizeof(readValue);
    Require(setsockopt_nid_postfix(sock, SolSocket, 0x7fff, &validValue, sizeof(validValue)) == -1 && *__error_nid_postfix() == PosixEnoprotoopt);
    Require(_setsockopt_nid_postfix(sock, SolSocket, 0x7fff, &validValue, sizeof(validValue)) == -1 && *__error_nid_postfix() == PosixEnoprotoopt);
    Require(getsockopt_nid_postfix(sock, SolSocket, 0x7fff, &readValue, &len) == -1 && *__error_nid_postfix() == PosixEnoprotoopt);
    Require(_getsockopt_nid_postfix(sock, SolSocket, 0x7fff, &readValue, &len) == -1 && *__error_nid_postfix() == PosixEnoprotoopt);

    Require(close_nid_postfix(sock) == 0);
    Require(close_nid_postfix(pipeFds[0]) == 0);
    Require(close_nid_postfix(pipeFds[1]) == 0);

    for (const int opt : {SoDebug, SoDontRoute, SoBroadcast}) {
        len = sizeof(readValue);
        Require(setsockopt_nid_postfix(sock, SolSocket, opt, &validValue, sizeof(validValue)) == -1 && *__error_nid_postfix() == PosixEbadf);
        Require(_setsockopt_nid_postfix(sock, SolSocket, opt, &validValue, sizeof(validValue)) == -1 && *__error_nid_postfix() == PosixEbadf);
        Require(getsockopt_nid_postfix(sock, SolSocket, opt, &readValue, &len) == -1 && *__error_nid_postfix() == PosixEbadf);
        Require(_getsockopt_nid_postfix(sock, SolSocket, opt, &readValue, &len) == -1 && *__error_nid_postfix() == PosixEbadf);
    }
}

void TestSetsockoptLengthBoundaries() {
    const int stream = socket_nid_postfix(AfInet, SockStream, 0);
    const int dgram = socket_nid_postfix(AfInet, SockDgram, 0);
    Require(stream >= 0 && dgram >= 0);

    const std::array<std::uint8_t, 32> wideBuffer{1, 0, 0, 0, 0xaa, 0xbb, 0xcc, 0xdd};

    for (const int fd : {stream, dgram}) {
        for (const int opt : {SoDebug, SoDontRoute, SoBroadcast}) {
            const int val = 1;
            Require(setsockopt_nid_postfix(fd, SolSocket, opt, &val, 0) == -1 && *__error_nid_postfix() == PosixEinval);
            Require(_setsockopt_nid_postfix(fd, SolSocket, opt, &val, 0) == -1 && *__error_nid_postfix() == PosixEinval);

            for (std::uint32_t shortLen = 1; shortLen < sizeof(int); ++shortLen) {
                Require(setsockopt_nid_postfix(fd, SolSocket, opt, &val, shortLen) == -1 && *__error_nid_postfix() == PosixEinval);
                Require(_setsockopt_nid_postfix(fd, SolSocket, opt, &val, shortLen) == -1 && *__error_nid_postfix() == PosixEinval);
            }

            if (fd == stream && opt == SoBroadcast && StreamRefusesBroadcast) {
                Require(setsockopt_nid_postfix(fd, SolSocket, opt, &val, sizeof(val)) == -1 && *__error_nid_postfix() == PosixEnoprotoopt);
                Require(setsockopt_nid_postfix(fd, SolSocket, opt, wideBuffer.data(), 8) == -1 && *__error_nid_postfix() == PosixEnoprotoopt);
                Require(setsockopt_nid_postfix(fd, SolSocket, opt, wideBuffer.data(), wideBuffer.size()) == -1 && *__error_nid_postfix() == PosixEnoprotoopt);
                Require(_setsockopt_nid_postfix(fd, SolSocket, opt, wideBuffer.data(), wideBuffer.size()) == -1 && *__error_nid_postfix() == PosixEnoprotoopt);

                const int zero = 0;
                Require(setsockopt_nid_postfix(fd, SolSocket, opt, &zero, sizeof(zero)) == -1 && *__error_nid_postfix() == PosixEnoprotoopt);
            } else {
                Require(setsockopt_nid_postfix(fd, SolSocket, opt, &val, sizeof(val)) == 0);
                Require(setsockopt_nid_postfix(fd, SolSocket, opt, wideBuffer.data(), 8) == 0);
                Require(setsockopt_nid_postfix(fd, SolSocket, opt, wideBuffer.data(), wideBuffer.size()) == 0);
                Require(_setsockopt_nid_postfix(fd, SolSocket, opt, wideBuffer.data(), wideBuffer.size()) == 0);

                const int zero = 0;
                Require(setsockopt_nid_postfix(fd, SolSocket, opt, &zero, sizeof(zero)) == 0);
            }
        }
    }

    Require(close_nid_postfix(stream) == 0);
    Require(close_nid_postfix(dgram) == 0);
}

void TestGetsockoptLengthAndTruncation() {
    const int sock = socket_nid_postfix(AfInet, SockDgram, 0);
    Require(sock >= 0);

    std::array<std::uint8_t, 32> buffer{};

    for (const int opt : {SoDebug, SoDontRoute, SoBroadcast}) {
        const int enableVal = 1;
        Require(setsockopt_nid_postfix(sock, SolSocket, opt, &enableVal, sizeof(enableVal)) == 0);

        int fullVal = 0;
        std::uint32_t fullLen = sizeof(fullVal);
        Require(getsockopt_nid_postfix(sock, SolSocket, opt, &fullVal, &fullLen) == 0);
        Require(fullLen == sizeof(int));
        Require(fullVal == opt);

        std::uint32_t len = 0;
        Require(getsockopt_nid_postfix(sock, SolSocket, opt, nullptr, &len) == 0);
        Require(len == 0);

        len = 0;
        Require(_getsockopt_nid_postfix(sock, SolSocket, opt, nullptr, &len) == 0);
        Require(len == 0);

        len = 0;
        buffer.fill(0xee);
        Require(getsockopt_nid_postfix(sock, SolSocket, opt, buffer.data(), &len) == 0);
        Require(len == 0);
        for (const std::uint8_t byte : buffer) {
            Require(byte == 0xee);
        }

        for (std::uint32_t capacity = 1; capacity <= 16; ++capacity) {
            buffer.fill(0xee);
            len = capacity;

            Require(getsockopt_nid_postfix(sock, SolSocket, opt, buffer.data(), &len) == 0);
            const std::uint32_t expectedLen = capacity < sizeof(int) ? capacity : sizeof(int);
            Require(len == expectedLen);

            const auto* fullBytes = reinterpret_cast<const std::uint8_t*>(&fullVal);
            for (std::uint32_t i = 0; i < expectedLen; ++i) {
                Require(buffer[i] == fullBytes[i]);
            }
            for (std::uint32_t i = expectedLen; i < buffer.size(); ++i) {
                Require(buffer[i] == 0xee);
            }
        }

        for (std::uint32_t capacity = 1; capacity <= 8; ++capacity) {
            buffer.fill(0xdd);
            len = capacity;

            Require(_getsockopt_nid_postfix(sock, SolSocket, opt, buffer.data(), &len) == 0);
            const std::uint32_t expectedLen = capacity < sizeof(int) ? capacity : sizeof(int);
            Require(len == expectedLen);

            const auto* fullBytes = reinterpret_cast<const std::uint8_t*>(&fullVal);
            for (std::uint32_t i = 0; i < expectedLen; ++i) {
                Require(buffer[i] == fullBytes[i]);
            }
            for (std::uint32_t i = expectedLen; i < buffer.size(); ++i) {
                Require(buffer[i] == 0xdd);
            }
        }

        const int disableVal = 0;
        Require(setsockopt_nid_postfix(sock, SolSocket, opt, &disableVal, sizeof(disableVal)) == 0);
    }

    Require(close_nid_postfix(sock) == 0);
}

void TestOptionTogglingAndReadback() {
    const int inetStream = socket_nid_postfix(AfInet, SockStream, 0);
    const int inetDgram = socket_nid_postfix(AfInet, SockDgram, 0);
    Require(inetStream >= 0 && inetDgram >= 0);

    const int v6Stream = socket_nid_postfix(AfInet6, SockStream, 0);
    const int v6Dgram = socket_nid_postfix(AfInet6, SockDgram, 0);

    const int descriptors[4]{inetStream, inetDgram, v6Stream, v6Dgram};
    const int count = (v6Stream >= 0 && v6Dgram >= 0) ? 4 : 2;

    for (int i = 0; i < count; ++i) {
        const int fd = descriptors[i];
        const bool isStream = (i % 2 == 0);
        for (const int opt : {SoDebug, SoDontRoute, SoBroadcast}) {
            if (isStream && opt == SoBroadcast && StreamRefusesBroadcast) {
                int readValue = -1;
                std::uint32_t len = sizeof(readValue);
                Require(getsockopt_nid_postfix(fd, SolSocket, opt, &readValue, &len) == -1 && *__error_nid_postfix() == PosixEnoprotoopt);
                const int enableOne = 1;
                Require(setsockopt_nid_postfix(fd, SolSocket, opt, &enableOne, sizeof(enableOne)) == -1 && *__error_nid_postfix() == PosixEnoprotoopt);
                continue;
            }

            int readValue = -1;
            std::uint32_t len = sizeof(readValue);

            Require(getsockopt_nid_postfix(fd, SolSocket, opt, &readValue, &len) == 0);
            Require(len == sizeof(int));
            Require(readValue == 0);

            const int enableOne = 1;
            Require(setsockopt_nid_postfix(fd, SolSocket, opt, &enableOne, sizeof(enableOne)) == 0);

            readValue = -1;
            len = sizeof(readValue);
            Require(getsockopt_nid_postfix(fd, SolSocket, opt, &readValue, &len) == 0);
            Require(len == sizeof(int));
            Require(readValue == opt);

            const int disableZero = 0;
            Require(setsockopt_nid_postfix(fd, SolSocket, opt, &disableZero, sizeof(disableZero)) == 0);

            readValue = -1;
            len = sizeof(readValue);
            Require(getsockopt_nid_postfix(fd, SolSocket, opt, &readValue, &len) == 0);
            Require(len == sizeof(int));
            Require(readValue == 0);

            const int arbitraryNonZero = 0x5a3c;
            Require(_setsockopt_nid_postfix(fd, SolSocket, opt, &arbitraryNonZero, sizeof(arbitraryNonZero)) == 0);

            readValue = -1;
            len = sizeof(readValue);
            Require(_getsockopt_nid_postfix(fd, SolSocket, opt, &readValue, &len) == 0);
            Require(len == sizeof(int));
            Require(readValue == opt);

            Require(_setsockopt_nid_postfix(fd, SolSocket, opt, &disableZero, sizeof(disableZero)) == 0);

            readValue = -1;
            len = sizeof(readValue);
            Require(_getsockopt_nid_postfix(fd, SolSocket, opt, &readValue, &len) == 0);
            Require(len == sizeof(int));
            Require(readValue == 0);
        }
    }

    for (int i = 0; i < count; ++i) {
        Require(close_nid_postfix(descriptors[i]) == 0);
    }
}

void TestIndependentOptionStates() {
    const int sock = socket_nid_postfix(AfInet, SockDgram, 0);
    Require(sock >= 0);

    int debugVal = 0;
    int routeVal = 0;
    int bcastVal = 0;
    std::uint32_t len = sizeof(int);

    auto ReadAll = [&](int& d, int& r, int& b) {
        len = sizeof(int);
        Require(getsockopt_nid_postfix(sock, SolSocket, SoDebug, &d, &len) == 0 && len == sizeof(int));
        len = sizeof(int);
        Require(getsockopt_nid_postfix(sock, SolSocket, SoDontRoute, &r, &len) == 0 && len == sizeof(int));
        len = sizeof(int);
        Require(getsockopt_nid_postfix(sock, SolSocket, SoBroadcast, &b, &len) == 0 && len == sizeof(int));
    };

    ReadAll(debugVal, routeVal, bcastVal);
    Require(debugVal == 0 && routeVal == 0 && bcastVal == 0);

    const int on = 1;
    const int off = 0;

    Require(setsockopt_nid_postfix(sock, SolSocket, SoDebug, &on, sizeof(on)) == 0);
    ReadAll(debugVal, routeVal, bcastVal);
    Require(debugVal == SoDebug && routeVal == 0 && bcastVal == 0);

    Require(setsockopt_nid_postfix(sock, SolSocket, SoDontRoute, &on, sizeof(on)) == 0);
    ReadAll(debugVal, routeVal, bcastVal);
    Require(debugVal == SoDebug && routeVal == SoDontRoute && bcastVal == 0);

    Require(setsockopt_nid_postfix(sock, SolSocket, SoBroadcast, &on, sizeof(on)) == 0);
    ReadAll(debugVal, routeVal, bcastVal);
    Require(debugVal == SoDebug && routeVal == SoDontRoute && bcastVal == SoBroadcast);

    Require(setsockopt_nid_postfix(sock, SolSocket, SoDebug, &off, sizeof(off)) == 0);
    ReadAll(debugVal, routeVal, bcastVal);
    Require(debugVal == 0 && routeVal == SoDontRoute && bcastVal == SoBroadcast);

    Require(setsockopt_nid_postfix(sock, SolSocket, SoDontRoute, &off, sizeof(off)) == 0);
    ReadAll(debugVal, routeVal, bcastVal);
    Require(debugVal == 0 && routeVal == 0 && bcastVal == SoBroadcast);

    Require(setsockopt_nid_postfix(sock, SolSocket, SoBroadcast, &off, sizeof(off)) == 0);
    ReadAll(debugVal, routeVal, bcastVal);
    Require(debugVal == 0 && routeVal == 0 && bcastVal == 0);

    Require(close_nid_postfix(sock) == 0);
}

} // namespace

int main() {
    TestErrorPrecedenceAndDescriptorValidation();
    TestSetsockoptLengthBoundaries();
    TestGetsockoptLengthAndTruncation();
    TestOptionTogglingAndReadback();
    TestIndependentOptionStates();
    return 0;
}
