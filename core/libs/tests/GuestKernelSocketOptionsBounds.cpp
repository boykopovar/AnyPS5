#include "prx/libc/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>

namespace {

constexpr int AfUnix = 1;
constexpr int AfInet = 2;
constexpr int AfInet6 = 28;

constexpr int SockStream = 1;
constexpr int SockDgram = 2;

constexpr int SolSocket = 0xffff;

constexpr int SoReuseAddr = 0x0004;
constexpr int SoKeepAlive = 0x0008;
constexpr int SoSndBuf = 0x1001;
constexpr int SoRcvBuf = 0x1002;

constexpr int PosixEbadf = 9;
constexpr int PosixEfault = 14;
constexpr int PosixEinval = 22;
constexpr int PosixEnotsock = 38;
constexpr int PosixEnoprotoopt = 42;
constexpr int PosixEnobufs = 55;

constexpr int MaximumBuffer = 524288;
constexpr int MaximumReservation = 5592405;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "Check failed at %s:%u\n", location.file_name(), location.line());
        std::abort();
    }
}

int GetOption(int descriptor, int option) {
    int value = -1;
    std::uint32_t length = sizeof(value);
    Require(getsockopt_nid_postfix(descriptor, SolSocket, option, &value, &length) == 0);
    Require(length == sizeof(value));
    return value;
}

int GetOptionAlt(int descriptor, int option) {
    int value = -1;
    std::uint32_t length = sizeof(value);
    Require(_getsockopt_nid_postfix(descriptor, SolSocket, option, &value, &length) == 0);
    Require(length == sizeof(value));
    return value;
}

void SetOption(int descriptor, int option, int value) {
    Require(setsockopt_nid_postfix(descriptor, SolSocket, option, &value, sizeof(value)) == 0);
}

void SetOptionAlt(int descriptor, int option, int value) {
    Require(_setsockopt_nid_postfix(descriptor, SolSocket, option, &value, sizeof(value)) == 0);
}

bool SetsockoptFails(int descriptor, int level, int option, const void* value, std::uint32_t length, int expectedErrno) {
    const int result = setsockopt_nid_postfix(descriptor, level, option, value, length);
    return result == -1 && *__error_nid_postfix() == expectedErrno;
}

bool SetsockoptAltFails(int descriptor, int level, int option, const void* value, std::uint32_t length, int expectedErrno) {
    const int result = _setsockopt_nid_postfix(descriptor, level, option, value, length);
    return result == -1 && *__error_nid_postfix() == expectedErrno;
}

bool GetsockoptFails(int descriptor, int level, int option, void* value, std::uint32_t* length, int expectedErrno) {
    const int result = getsockopt_nid_postfix(descriptor, level, option, value, length);
    return result == -1 && *__error_nid_postfix() == expectedErrno;
}

bool GetsockoptAltFails(int descriptor, int level, int option, void* value, std::uint32_t* length, int expectedErrno) {
    const int result = _getsockopt_nid_postfix(descriptor, level, option, value, length);
    return result == -1 && *__error_nid_postfix() == expectedErrno;
}

std::array<std::uint8_t, 16> MakeLoopbackAddress() {
    std::array<std::uint8_t, 16> address{16, 2, 0, 0, 127, 0, 0, 1};
    return address;
}

void TestPrecedenceAndInputValidation() {
    int pipeFds[2]{-1, -1};
    Require(pipe_nid_postfix(pipeFds) == 0);

    const int sock = socket_nid_postfix(AfInet, SockStream, 0);
    Require(sock >= 0);

    const int options[] = {SoReuseAddr, SoKeepAlive, SoRcvBuf, SoSndBuf};

    for (const int opt : options) {
        int validVal = 1;

        Require(SetsockoptFails(-1, SolSocket, opt, nullptr, sizeof(int), PosixEfault));
        Require(SetsockoptAltFails(-1, SolSocket, opt, nullptr, sizeof(int), PosixEfault));
        Require(SetsockoptFails(pipeFds[0], SolSocket, opt, nullptr, sizeof(int), PosixEfault));
        Require(SetsockoptFails(sock, SolSocket, opt, nullptr, sizeof(int), PosixEfault));

        Require(SetsockoptFails(-1, SolSocket, opt, &validVal, static_cast<std::uint32_t>(-1), PosixEinval));
        Require(SetsockoptAltFails(-1, SolSocket, opt, &validVal, static_cast<std::uint32_t>(-1), PosixEinval));
        Require(SetsockoptFails(pipeFds[0], SolSocket, opt, &validVal, static_cast<std::uint32_t>(-1), PosixEinval));
        Require(SetsockoptFails(sock, SolSocket, opt, &validVal, static_cast<std::uint32_t>(-1), PosixEinval));

        Require(SetsockoptFails(-1, SolSocket, opt, &validVal, sizeof(validVal), PosixEbadf));
        Require(SetsockoptAltFails(-1, SolSocket, opt, &validVal, sizeof(validVal), PosixEbadf));
        Require(SetsockoptFails(99999, SolSocket, opt, &validVal, sizeof(validVal), PosixEbadf));
        Require(SetsockoptAltFails(99999, SolSocket, opt, &validVal, sizeof(validVal), PosixEbadf));

        Require(SetsockoptFails(pipeFds[0], SolSocket, opt, &validVal, sizeof(validVal), PosixEnotsock));
        Require(SetsockoptAltFails(pipeFds[0], SolSocket, opt, &validVal, sizeof(validVal), PosixEnotsock));
        Require(SetsockoptFails(pipeFds[1], SolSocket, opt, &validVal, sizeof(validVal), PosixEnotsock));
        Require(SetsockoptAltFails(pipeFds[1], SolSocket, opt, &validVal, sizeof(validVal), PosixEnotsock));

        Require(SetsockoptFails(sock, 12345, opt, &validVal, sizeof(validVal), PosixEinval));
        Require(SetsockoptAltFails(sock, 12345, opt, &validVal, sizeof(validVal), PosixEinval));

        Require(SetsockoptFails(sock, SolSocket, opt, &validVal, 0, PosixEinval));
        Require(SetsockoptFails(sock, SolSocket, opt, &validVal, 1, PosixEinval));
        Require(SetsockoptFails(sock, SolSocket, opt, &validVal, 2, PosixEinval));
        Require(SetsockoptFails(sock, SolSocket, opt, &validVal, 3, PosixEinval));
        Require(SetsockoptAltFails(sock, SolSocket, opt, &validVal, 0, PosixEinval));
        Require(SetsockoptAltFails(sock, SolSocket, opt, &validVal, 1, PosixEinval));
        Require(SetsockoptAltFails(sock, SolSocket, opt, &validVal, 2, PosixEinval));
        Require(SetsockoptAltFails(sock, SolSocket, opt, &validVal, 3, PosixEinval));

        const std::int64_t wideVal = 1;
        Require(setsockopt_nid_postfix(sock, SolSocket, opt, &wideVal, sizeof(wideVal)) == 0);
        Require(_setsockopt_nid_postfix(sock, SolSocket, opt, &wideVal, sizeof(wideVal)) == 0);
    }

    Require(close_nid_postfix(sock) == 0);
    Require(close_nid_postfix(pipeFds[0]) == 0);
    Require(close_nid_postfix(pipeFds[1]) == 0);

    for (const int opt : options) {
        int validVal = 1;
        Require(SetsockoptFails(sock, SolSocket, opt, &validVal, sizeof(validVal), PosixEbadf));
        Require(SetsockoptAltFails(sock, SolSocket, opt, &validVal, sizeof(validVal), PosixEbadf));
    }
}

void TestGetsockoptLengthAndPointerBounds() {
    int pipeFds[2]{-1, -1};
    Require(pipe_nid_postfix(pipeFds) == 0);

    const int sock = socket_nid_postfix(AfInet, SockStream, 0);
    Require(sock >= 0);

    const int options[] = {SoReuseAddr, SoKeepAlive, SoRcvBuf, SoSndBuf};

    for (const int opt : options) {
        int outputVal = 0;
        std::uint32_t len = sizeof(outputVal);

        Require(GetsockoptFails(-1, SolSocket, opt, &outputVal, nullptr, PosixEfault));
        Require(GetsockoptAltFails(-1, SolSocket, opt, &outputVal, nullptr, PosixEfault));
        Require(GetsockoptFails(sock, SolSocket, opt, &outputVal, nullptr, PosixEfault));
        Require(GetsockoptAltFails(sock, SolSocket, opt, &outputVal, nullptr, PosixEfault));

        len = sizeof(outputVal);
        Require(GetsockoptFails(-1, SolSocket, opt, &outputVal, &len, PosixEbadf));
        Require(GetsockoptAltFails(-1, SolSocket, opt, &outputVal, &len, PosixEbadf));

        len = sizeof(outputVal);
        Require(GetsockoptFails(pipeFds[0], SolSocket, opt, &outputVal, &len, PosixEnotsock));
        Require(GetsockoptAltFails(pipeFds[0], SolSocket, opt, &outputVal, &len, PosixEnotsock));

        len = sizeof(outputVal);
        Require(GetsockoptFails(sock, 54321, opt, &outputVal, &len, PosixEinval));
        Require(GetsockoptAltFails(sock, 54321, opt, &outputVal, &len, PosixEinval));

        len = 0;
        int canary = 0x5a5a5a5a;
        Require(getsockopt_nid_postfix(sock, SolSocket, opt, &canary, &len) == 0);
        Require(len == 0);
        Require(canary == 0x5a5a5a5a);

        len = 0;
        canary = 0x3c3c3c3c;
        Require(_getsockopt_nid_postfix(sock, SolSocket, opt, &canary, &len) == 0);
        Require(len == 0);
        Require(canary == 0x3c3c3c3c);

        len = sizeof(outputVal);
        Require(getsockopt_nid_postfix(sock, SolSocket, opt, nullptr, &len) == 0);
        Require(len == 0);

        len = sizeof(outputVal);
        Require(_getsockopt_nid_postfix(sock, SolSocket, opt, nullptr, &len) == 0);
        Require(len == 0);

        std::uint8_t byteVal = 0xbb;
        len = sizeof(byteVal);
        Require(getsockopt_nid_postfix(sock, SolSocket, opt, &byteVal, &len) == 0);
        Require(len == sizeof(byteVal));

        std::uint16_t shortVal = 0xcccc;
        len = sizeof(shortVal);
        Require(getsockopt_nid_postfix(sock, SolSocket, opt, &shortVal, &len) == 0);
        Require(len == sizeof(shortVal));

        std::array<std::uint8_t, 16> largeBuf{};
        largeBuf.fill(0xee);
        len = largeBuf.size();
        Require(getsockopt_nid_postfix(sock, SolSocket, opt, largeBuf.data(), &len) == 0);
        Require(len == sizeof(int));
    }

    Require(close_nid_postfix(sock) == 0);
    Require(close_nid_postfix(pipeFds[0]) == 0);
    Require(close_nid_postfix(pipeFds[1]) == 0);

    for (const int opt : options) {
        int outputVal = 0;
        std::uint32_t len = sizeof(outputVal);
        Require(GetsockoptFails(sock, SolSocket, opt, &outputVal, &len, PosixEbadf));
        Require(GetsockoptAltFails(sock, SolSocket, opt, &outputVal, &len, PosixEbadf));
    }
}

void TestReuseAddrBoundsAndSemantics() {
    const int stream = socket_nid_postfix(AfInet, SockStream, 0);
    const int dgram = socket_nid_postfix(AfInet, SockDgram, 0);
    Require(stream >= 0 && dgram >= 0);

    Require(GetOption(stream, SoReuseAddr) == 0);
    Require(GetOptionAlt(stream, SoReuseAddr) == 0);
    Require(GetOption(dgram, SoReuseAddr) == 0);
    Require(GetOptionAlt(dgram, SoReuseAddr) == 0);

    SetOption(stream, SoReuseAddr, 1);
    Require(GetOption(stream, SoReuseAddr) == SoReuseAddr);
    Require(GetOptionAlt(stream, SoReuseAddr) == SoReuseAddr);

    SetOption(stream, SoReuseAddr, 0);
    Require(GetOption(stream, SoReuseAddr) == 0);

    SetOptionAlt(stream, SoReuseAddr, 4);
    Require(GetOption(stream, SoReuseAddr) == SoReuseAddr);

    SetOptionAlt(stream, SoReuseAddr, 0);
    Require(GetOption(stream, SoReuseAddr) == 0);

    SetOption(stream, SoReuseAddr, -1);
    Require(GetOption(stream, SoReuseAddr) == SoReuseAddr);
    SetOption(stream, SoReuseAddr, 0);

    SetOption(stream, SoReuseAddr, 100);
    std::uint8_t byteVal = 0;
    std::uint32_t len = 1;
    Require(getsockopt_nid_postfix(stream, SolSocket, SoReuseAddr, &byteVal, &len) == 0);
    Require(len == 1);
    Require(byteVal == SoReuseAddr);

    SetOption(dgram, SoReuseAddr, 1);
    Require(GetOption(dgram, SoReuseAddr) == SoReuseAddr);
    SetOption(dgram, SoReuseAddr, 0);
    Require(GetOption(dgram, SoReuseAddr) == 0);

    const int v6Stream = socket_nid_postfix(AfInet6, SockStream, 0);
    if (v6Stream >= 0) {
        Require(GetOption(v6Stream, SoReuseAddr) == 0);
        SetOption(v6Stream, SoReuseAddr, 1);
        Require(GetOption(v6Stream, SoReuseAddr) == SoReuseAddr);
        Require(close_nid_postfix(v6Stream) == 0);
    }

    Require(close_nid_postfix(stream) == 0);
    Require(close_nid_postfix(dgram) == 0);
}

void TestKeepAliveBoundsAndSemantics() {
    const int stream = socket_nid_postfix(AfInet, SockStream, 0);
    const int dgram = socket_nid_postfix(AfInet, SockDgram, 0);
    Require(stream >= 0 && dgram >= 0);

    Require(GetOption(stream, SoKeepAlive) == 0);
    Require(GetOptionAlt(stream, SoKeepAlive) == 0);
#ifdef _WIN32
    {
        int outputVal = 0;
        std::uint32_t winLen = sizeof(outputVal);
        Require(GetsockoptFails(dgram, SolSocket, SoKeepAlive, &outputVal, &winLen, PosixEnoprotoopt));
        Require(GetsockoptAltFails(dgram, SolSocket, SoKeepAlive, &outputVal, &winLen, PosixEnoprotoopt));
    }
#else
    Require(GetOption(dgram, SoKeepAlive) == 0);
    Require(GetOptionAlt(dgram, SoKeepAlive) == 0);
#endif

    SetOption(stream, SoKeepAlive, 1);
    Require(GetOption(stream, SoKeepAlive) == SoKeepAlive);
    Require(GetOptionAlt(stream, SoKeepAlive) == SoKeepAlive);

    SetOption(stream, SoKeepAlive, 0);
    Require(GetOption(stream, SoKeepAlive) == 0);

    SetOptionAlt(stream, SoKeepAlive, 8);
    Require(GetOption(stream, SoKeepAlive) == SoKeepAlive);

    SetOptionAlt(stream, SoKeepAlive, 0);
    Require(GetOption(stream, SoKeepAlive) == 0);

    SetOption(stream, SoKeepAlive, 999);
    Require(GetOption(stream, SoKeepAlive) == SoKeepAlive);

    std::uint8_t byteVal = 0;
    std::uint32_t len = 1;
    Require(getsockopt_nid_postfix(stream, SolSocket, SoKeepAlive, &byteVal, &len) == 0);
    Require(len == 1);
    Require(byteVal == SoKeepAlive);

    SetOption(stream, SoKeepAlive, 0);
    len = 1;
    byteVal = 0xff;
    Require(getsockopt_nid_postfix(stream, SolSocket, SoKeepAlive, &byteVal, &len) == 0);
    Require(len == 1);
    Require(byteVal == 0);

#ifdef _WIN32
    const int validVal = 1;
    Require(SetsockoptFails(dgram, SolSocket, SoKeepAlive, &validVal, sizeof(validVal), PosixEnoprotoopt));
    Require(SetsockoptAltFails(dgram, SolSocket, SoKeepAlive, &validVal, sizeof(validVal), PosixEnoprotoopt));
#else
    SetOption(dgram, SoKeepAlive, 1);
    Require(GetOption(dgram, SoKeepAlive) == SoKeepAlive);
    SetOption(dgram, SoKeepAlive, 0);
    Require(GetOption(dgram, SoKeepAlive) == 0);
#endif

    Require(close_nid_postfix(stream) == 0);
    Require(close_nid_postfix(dgram) == 0);
}

void TestReceiveBufferBoundsAndClamping() {
    const int stream = socket_nid_postfix(AfInet, SockStream, 0);
    const int dgram = socket_nid_postfix(AfInet, SockDgram, 0);
    Require(stream >= 0 && dgram >= 0);

    Require(GetOption(stream, SoRcvBuf) == 65536);
    Require(GetOptionAlt(stream, SoRcvBuf) == 65536);
    Require(GetOption(dgram, SoRcvBuf) == 42080);
    Require(GetOptionAlt(dgram, SoRcvBuf) == 42080);

    const int v6Dgram = socket_nid_postfix(AfInet6, SockDgram, 0);
    if (v6Dgram >= 0) {
        Require(GetOption(v6Dgram, SoRcvBuf) == 42080);
        Require(close_nid_postfix(v6Dgram) == 0);
    }

    int unixPair[2]{-1, -1};
    Require(socketpair_nid_postfix(AfUnix, SockStream, 0, unixPair) == 0);
    Require(GetOption(unixPair[0], SoRcvBuf) == 8192);
    Require(GetOption(unixPair[1], SoRcvBuf) == 8192);
    Require(close_nid_postfix(unixPair[0]) == 0);
    Require(close_nid_postfix(unixPair[1]) == 0);

    Require(socketpair_nid_postfix(AfUnix, SockDgram, 0, unixPair) == 0);
    Require(GetOption(unixPair[0], SoRcvBuf) == 4096);
    Require(GetOption(unixPair[1], SoRcvBuf) == 4096);
    Require(close_nid_postfix(unixPair[0]) == 0);
    Require(close_nid_postfix(unixPair[1]) == 0);

    for (const int invalidVal : {0, -1, -500, -32768, -2147483647}) {
        Require(SetsockoptFails(stream, SolSocket, SoRcvBuf, &invalidVal, sizeof(invalidVal), PosixEinval));
        Require(SetsockoptAltFails(stream, SolSocket, SoRcvBuf, &invalidVal, sizeof(invalidVal), PosixEinval));
        Require(GetOption(stream, SoRcvBuf) == 65536);
    }

    for (const int overflowVal : {MaximumReservation + 1, 6000000, 10000000}) {
        Require(SetsockoptFails(stream, SolSocket, SoRcvBuf, &overflowVal, sizeof(overflowVal), PosixEnobufs));
        Require(SetsockoptAltFails(stream, SolSocket, SoRcvBuf, &overflowVal, sizeof(overflowVal), PosixEnobufs));
        Require(GetOption(stream, SoRcvBuf) == 65536);
    }

    SetOption(stream, SoRcvBuf, 1024);
    Require(GetOption(stream, SoRcvBuf) == 1024);

    SetOptionAlt(stream, SoRcvBuf, 131072);
    Require(GetOption(stream, SoRcvBuf) == 131072);
    Require(GetOptionAlt(stream, SoRcvBuf) == 131072);

    SetOption(stream, SoRcvBuf, MaximumBuffer);
    Require(GetOption(stream, SoRcvBuf) == MaximumBuffer);

    SetOption(stream, SoRcvBuf, MaximumBuffer + 1);
    Require(GetOption(stream, SoRcvBuf) == MaximumBuffer);

    SetOption(stream, SoRcvBuf, MaximumReservation);
    Require(GetOption(stream, SoRcvBuf) == MaximumBuffer);

    Require(close_nid_postfix(stream) == 0);
    Require(close_nid_postfix(dgram) == 0);
}

void TestSendBufferBoundsAndClamping() {
    const int stream = socket_nid_postfix(AfInet, SockStream, 0);
    const int dgram = socket_nid_postfix(AfInet, SockDgram, 0);
    Require(stream >= 0 && dgram >= 0);

    Require(GetOption(stream, SoSndBuf) == 32768);
    Require(GetOptionAlt(stream, SoSndBuf) == 32768);
    Require(GetOption(dgram, SoSndBuf) == 9216);
    Require(GetOptionAlt(dgram, SoSndBuf) == 9216);

    const int v6Dgram = socket_nid_postfix(AfInet6, SockDgram, 0);
    if (v6Dgram >= 0) {
        Require(GetOption(v6Dgram, SoSndBuf) == 9216);
        Require(close_nid_postfix(v6Dgram) == 0);
    }

    int unixPair[2]{-1, -1};
    Require(socketpair_nid_postfix(AfUnix, SockStream, 0, unixPair) == 0);
    Require(GetOption(unixPair[0], SoSndBuf) == 8192);
    Require(GetOption(unixPair[1], SoSndBuf) == 8192);
    Require(close_nid_postfix(unixPair[0]) == 0);
    Require(close_nid_postfix(unixPair[1]) == 0);

    Require(socketpair_nid_postfix(AfUnix, SockDgram, 0, unixPair) == 0);
    Require(GetOption(unixPair[0], SoSndBuf) == 2048);
    Require(GetOption(unixPair[1], SoSndBuf) == 2048);
    Require(close_nid_postfix(unixPair[0]) == 0);
    Require(close_nid_postfix(unixPair[1]) == 0);

    for (const int invalidVal : {0, -1, -100, -32768, -2147483647}) {
        Require(SetsockoptFails(stream, SolSocket, SoSndBuf, &invalidVal, sizeof(invalidVal), PosixEinval));
        Require(SetsockoptAltFails(stream, SolSocket, SoSndBuf, &invalidVal, sizeof(invalidVal), PosixEinval));
        Require(GetOption(stream, SoSndBuf) == 32768);
    }

    for (const int overflowVal : {MaximumReservation + 1, 5592406, 8000000}) {
        Require(SetsockoptFails(stream, SolSocket, SoSndBuf, &overflowVal, sizeof(overflowVal), PosixEnobufs));
        Require(SetsockoptAltFails(stream, SolSocket, SoSndBuf, &overflowVal, sizeof(overflowVal), PosixEnobufs));
        Require(GetOption(stream, SoSndBuf) == 32768);
    }

    SetOption(stream, SoSndBuf, 4096);
    Require(GetOption(stream, SoSndBuf) == 4096);

    SetOptionAlt(stream, SoSndBuf, 65536);
    Require(GetOption(stream, SoSndBuf) == 65536);
    Require(GetOptionAlt(stream, SoSndBuf) == 65536);

    SetOption(stream, SoSndBuf, MaximumBuffer);
    Require(GetOption(stream, SoSndBuf) == MaximumBuffer);

    SetOption(stream, SoSndBuf, MaximumBuffer + 100);
    Require(GetOption(stream, SoSndBuf) == MaximumBuffer);

    SetOption(stream, SoSndBuf, MaximumReservation);
    Require(GetOption(stream, SoSndBuf) == MaximumBuffer);

    Require(close_nid_postfix(stream) == 0);
    Require(close_nid_postfix(dgram) == 0);
}

void TestListeningAndAcceptInheritance() {
    const int listener = socket_nid_postfix(AfInet, SockStream, 0);
    Require(listener >= 0);

    auto address = MakeLoopbackAddress();
    Require(bind_nid_postfix(listener, address.data(), address.size()) == 0);
    Require(listen_nid_postfix(listener, 2) == 0);

    std::uint32_t addressSize = address.size();
    Require(getsockname_nid_postfix(listener, address.data(), &addressSize) == 0);

    SetOption(listener, SoReuseAddr, 1);
    SetOption(listener, SoRcvBuf, 16384);

    const int client = socket_nid_postfix(AfInet, SockStream, 0);
    Require(client >= 0);
    Require(connect_nid_postfix(client, address.data(), address.size()) == 0);

    const int accepted = accept_nid_postfix(listener, nullptr, nullptr);
    Require(accepted >= 0);

    Require(GetOption(accepted, SoRcvBuf) == 16384);
    Require(GetOption(accepted, SoSndBuf) == 32768);

    Require(close_nid_postfix(client) == 0);
    Require(close_nid_postfix(accepted) == 0);
    Require(close_nid_postfix(listener) == 0);
}

} // namespace

int main() {
    TestPrecedenceAndInputValidation();
    TestGetsockoptLengthAndPointerBounds();
    TestReuseAddrBoundsAndSemantics();
    TestKeepAliveBoundsAndSemantics();
    TestReceiveBufferBoundsAndClamping();
    TestSendBufferBoundsAndClamping();
    TestListeningAndAcceptInheritance();
    return 0;
}
