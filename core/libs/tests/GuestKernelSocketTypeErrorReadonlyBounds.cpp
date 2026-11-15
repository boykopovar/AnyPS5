#include "prx/libc/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace {

constexpr int AfUnix = 1;
constexpr int AfInet = 2;
constexpr int AfInet6 = 28;

constexpr int SockStream = 1;
constexpr int SockDgram = 2;

constexpr int SolSocket = 0xffff;

constexpr int SoReuseAddr = 0x0004;
constexpr int SoKeepAlive = 0x0008;
constexpr int SoReusePort = 0x0200;
constexpr int SoError = 0x1007;
constexpr int SoType = 0x1008;

constexpr int PosixEbadf = 9;
constexpr int PosixEfault = 14;
constexpr int PosixEinval = 22;
constexpr int PosixEnotsock = 38;
constexpr int PosixEnoprotoopt = 42;

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

int GetIntOption(int descriptor, int option) {
    int value = -1;
    std::uint32_t length = sizeof(value);
    Require(getsockopt_nid_postfix(descriptor, SolSocket, option, &value, &length) == 0);
    Require(length == sizeof(value));
    return value;
}

int GetIntOptionAlt(int descriptor, int option) {
    int value = -1;
    std::uint32_t length = sizeof(value);
    Require(_getsockopt_nid_postfix(descriptor, SolSocket, option, &value, &length) == 0);
    Require(length == sizeof(value));
    return value;
}

void SetIntOption(int descriptor, int option, int value) {
    Require(setsockopt_nid_postfix(descriptor, SolSocket, option, &value, sizeof(value)) == 0);
}

void SetIntOptionAlt(int descriptor, int option, int value) {
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

void TestReadonlyOptionsFailOnSetsockopt() {
    int pipeFds[2]{-1, -1};
    Require(pipe_nid_postfix(pipeFds) == 0);

    const int sock = socket_nid_postfix(AfInet, SockStream, 0);
    Require(sock >= 0);

    const int readonlyOptions[] = {SoType, SoError};

    for (const int opt : readonlyOptions) {
        int val = 1;

        Require(SetsockoptFails(-1, SolSocket, opt, nullptr, sizeof(val), PosixEfault));
        Require(SetsockoptAltFails(-1, SolSocket, opt, nullptr, sizeof(val), PosixEfault));
        Require(SetsockoptFails(sock, SolSocket, opt, nullptr, sizeof(val), PosixEfault));

        Require(SetsockoptFails(-1, SolSocket, opt, &val, static_cast<std::uint32_t>(-1), PosixEinval));
        Require(SetsockoptFails(sock, SolSocket, opt, &val, static_cast<std::uint32_t>(-1), PosixEinval));

        Require(SetsockoptFails(-1, SolSocket, opt, &val, sizeof(val), PosixEbadf));
        Require(SetsockoptAltFails(-1, SolSocket, opt, &val, sizeof(val), PosixEbadf));
        Require(SetsockoptFails(99999, SolSocket, opt, &val, sizeof(val), PosixEbadf));

        Require(SetsockoptFails(pipeFds[0], SolSocket, opt, &val, sizeof(val), PosixEnotsock));
        Require(SetsockoptAltFails(pipeFds[0], SolSocket, opt, &val, sizeof(val), PosixEnotsock));

        Require(SetsockoptFails(sock, SolSocket, opt, &val, sizeof(val), PosixEnoprotoopt));
        Require(SetsockoptAltFails(sock, SolSocket, opt, &val, sizeof(val), PosixEnoprotoopt));

        for (const int testVal : {0, 1, 2, 42, -1}) {
            Require(SetsockoptFails(sock, SolSocket, opt, &testVal, sizeof(testVal), PosixEnoprotoopt));
            Require(SetsockoptAltFails(sock, SolSocket, opt, &testVal, sizeof(testVal), PosixEnoprotoopt));
        }
    }

    Require(close_nid_postfix(sock) == 0);
    Require(close_nid_postfix(pipeFds[0]) == 0);
    Require(close_nid_postfix(pipeFds[1]) == 0);
}

void TestSocketTypeReadback() {
    int pipeFds[2]{-1, -1};
    Require(pipe_nid_postfix(pipeFds) == 0);

    const int v4Stream = socket_nid_postfix(AfInet, SockStream, 0);
    const int v4Dgram = socket_nid_postfix(AfInet, SockDgram, 0);
    Require(v4Stream >= 0 && v4Dgram >= 0);

    Require(GetIntOption(v4Stream, SoType) == SockStream);
    Require(GetIntOptionAlt(v4Stream, SoType) == SockStream);
    Require(GetIntOption(v4Dgram, SoType) == SockDgram);
    Require(GetIntOptionAlt(v4Dgram, SoType) == SockDgram);

    int unixPair[2]{-1, -1};
    Require(socketpair_nid_postfix(AfUnix, SockStream, 0, unixPair) == 0);
    Require(GetIntOption(unixPair[0], SoType) == SockStream);
    Require(GetIntOption(unixPair[1], SoType) == SockStream);
    Require(close_nid_postfix(unixPair[0]) == 0);
    Require(close_nid_postfix(unixPair[1]) == 0);

    Require(socketpair_nid_postfix(AfUnix, SockDgram, 0, unixPair) == 0);
    Require(GetIntOption(unixPair[0], SoType) == SockDgram);
    Require(GetIntOption(unixPair[1], SoType) == SockDgram);
    Require(close_nid_postfix(unixPair[0]) == 0);
    Require(close_nid_postfix(unixPair[1]) == 0);

    const int v6Stream = socket_nid_postfix(AfInet6, SockStream, 0);
    if (v6Stream >= 0) {
        Require(GetIntOption(v6Stream, SoType) == SockStream);
        Require(close_nid_postfix(v6Stream) == 0);
    }

    const int v6Dgram = socket_nid_postfix(AfInet6, SockDgram, 0);
    if (v6Dgram >= 0) {
        Require(GetIntOption(v6Dgram, SoType) == SockDgram);
        Require(close_nid_postfix(v6Dgram) == 0);
    }

    int outVal = 0;
    std::uint32_t len = sizeof(outVal);
    Require(GetsockoptFails(v4Stream, SolSocket, SoType, &outVal, nullptr, PosixEfault));
    Require(GetsockoptAltFails(v4Stream, SolSocket, SoType, &outVal, nullptr, PosixEfault));
    Require(GetsockoptFails(-1, SolSocket, SoType, &outVal, nullptr, PosixEfault));

    len = sizeof(outVal);
    Require(GetsockoptFails(-1, SolSocket, SoType, &outVal, &len, PosixEbadf));
    Require(GetsockoptFails(pipeFds[0], SolSocket, SoType, &outVal, &len, PosixEnotsock));

    len = 0;
    int canary = 0x12345678;
    Require(getsockopt_nid_postfix(v4Stream, SolSocket, SoType, &canary, &len) == 0 && len == 0 && canary == 0x12345678);

    len = sizeof(outVal);
    Require(getsockopt_nid_postfix(v4Stream, SolSocket, SoType, nullptr, &len) == 0 && len == 0);

    std::uint8_t shortByte = 0xff;
    len = 1;
    Require(getsockopt_nid_postfix(v4Stream, SolSocket, SoType, &shortByte, &len) == 0 && len == 1 && shortByte == SockStream);

    shortByte = 0xff;
    len = 1;
    Require(getsockopt_nid_postfix(v4Dgram, SolSocket, SoType, &shortByte, &len) == 0 && len == 1 && shortByte == SockDgram);

    std::uint16_t shortWord = 0xffff;
    len = 2;
    Require(getsockopt_nid_postfix(v4Stream, SolSocket, SoType, &shortWord, &len) == 0 && len == 2 && shortWord == SockStream);

    std::array<std::uint8_t, 16> largeBuf{};
    largeBuf.fill(0xaa);
    len = largeBuf.size();
    Require(getsockopt_nid_postfix(v4Stream, SolSocket, SoType, largeBuf.data(), &len) == 0 && len == sizeof(int));
    int readLarge = 0;
    std::memcpy(&readLarge, largeBuf.data(), sizeof(readLarge));
    Require(readLarge == SockStream);

    Require(close_nid_postfix(v4Stream) == 0);
    Require(close_nid_postfix(v4Dgram) == 0);
    Require(close_nid_postfix(pipeFds[0]) == 0);
    Require(close_nid_postfix(pipeFds[1]) == 0);
}

void TestSocketErrorOptionReadbackAndClear() {
    int pipeFds[2]{-1, -1};
    Require(pipe_nid_postfix(pipeFds) == 0);

    const int sock = socket_nid_postfix(AfInet, SockStream, 0);
    Require(sock >= 0);

    Require(GetIntOption(sock, SoError) == 0);
    Require(GetIntOptionAlt(sock, SoError) == 0);

    const int dgram = socket_nid_postfix(AfInet, SockDgram, 0);
    Require(dgram >= 0);
    Require(GetIntOption(dgram, SoError) == 0);

    int outVal = 0;
    std::uint32_t len = sizeof(outVal);
    Require(GetsockoptFails(sock, SolSocket, SoError, &outVal, nullptr, PosixEfault));
    Require(GetsockoptAltFails(sock, SolSocket, SoError, &outVal, nullptr, PosixEfault));
    Require(GetsockoptFails(-1, SolSocket, SoError, &outVal, nullptr, PosixEfault));

    len = sizeof(outVal);
    Require(GetsockoptFails(-1, SolSocket, SoError, &outVal, &len, PosixEbadf));
    Require(GetsockoptFails(pipeFds[0], SolSocket, SoError, &outVal, &len, PosixEnotsock));

    len = 0;
    int canary = 0x77777777;
    Require(getsockopt_nid_postfix(sock, SolSocket, SoError, &canary, &len) == 0 && len == 0 && canary == 0x77777777);

    len = sizeof(outVal);
    Require(getsockopt_nid_postfix(sock, SolSocket, SoError, nullptr, &len) == 0 && len == 0);

    std::uint8_t shortByte = 0xff;
    len = 1;
    Require(getsockopt_nid_postfix(sock, SolSocket, SoError, &shortByte, &len) == 0 && len == 1 && shortByte == 0);

    Require(close_nid_postfix(sock) == 0);
    Require(close_nid_postfix(dgram) == 0);
    Require(close_nid_postfix(pipeFds[0]) == 0);
    Require(close_nid_postfix(pipeFds[1]) == 0);
}

void TestBooleanOptionToggling() {
    int pipeFds[2]{-1, -1};
    Require(pipe_nid_postfix(pipeFds) == 0);

    const int stream = socket_nid_postfix(AfInet, SockStream, 0);
    Require(stream >= 0);

    const int booleanOptions[] = {SoReuseAddr, SoKeepAlive, SoReusePort};

    for (const int opt : booleanOptions) {
        Require(GetIntOption(stream, opt) == 0);
        Require(GetIntOptionAlt(stream, opt) == 0);

        SetIntOption(stream, opt, 1);
        Require(GetIntOption(stream, opt) == opt);
        Require(GetIntOptionAlt(stream, opt) == opt);

        SetIntOption(stream, opt, 0);
        Require(GetIntOption(stream, opt) == 0);
        Require(GetIntOptionAlt(stream, opt) == 0);

        SetIntOptionAlt(stream, opt, 5);
        Require(GetIntOption(stream, opt) == opt);

        SetIntOptionAlt(stream, opt, 0);
        Require(GetIntOption(stream, opt) == 0);

        SetIntOption(stream, opt, -1);
        Require(GetIntOption(stream, opt) == opt);

        SetIntOption(stream, opt, 0);
        Require(GetIntOption(stream, opt) == 0);

        for (std::uint32_t shortLen = 0; shortLen < sizeof(int); ++shortLen) {
            int val = 1;
            Require(SetsockoptFails(stream, SolSocket, opt, &val, shortLen, PosixEinval));
            Require(SetsockoptAltFails(stream, SolSocket, opt, &val, shortLen, PosixEinval));
        }

        int val = 1;
        Require(SetsockoptFails(-1, SolSocket, opt, &val, sizeof(val), PosixEbadf));
        Require(SetsockoptFails(pipeFds[0], SolSocket, opt, &val, sizeof(val), PosixEnotsock));

        int out = 0;
        std::uint32_t len = sizeof(out);
        Require(GetsockoptFails(stream, SolSocket, opt, &out, nullptr, PosixEfault));
        Require(GetsockoptFails(-1, SolSocket, opt, &out, &len, PosixEbadf));
        Require(GetsockoptFails(pipeFds[0], SolSocket, opt, &out, &len, PosixEnotsock));

        len = 0;
        int canary = 0x22222222;
        Require(getsockopt_nid_postfix(stream, SolSocket, opt, &canary, &len) == 0 && len == 0 && canary == 0x22222222);
    }

    SetIntOption(stream, SoReuseAddr, 1);
    Require(GetIntOption(stream, SoReuseAddr) == SoReuseAddr);
    Require(GetIntOption(stream, SoReusePort) == 0);

    SetIntOption(stream, SoReusePort, 1);
    Require(GetIntOption(stream, SoReuseAddr) == SoReuseAddr);
    Require(GetIntOption(stream, SoReusePort) == SoReusePort);

    SetIntOption(stream, SoReuseAddr, 0);
    Require(GetIntOption(stream, SoReuseAddr) == 0);
    Require(GetIntOption(stream, SoReusePort) == SoReusePort);

    SetIntOption(stream, SoReusePort, 0);
    Require(GetIntOption(stream, SoReusePort) == 0);

    Require(close_nid_postfix(stream) == 0);
    Require(close_nid_postfix(pipeFds[0]) == 0);
    Require(close_nid_postfix(pipeFds[1]) == 0);
}

} // namespace

int main() {
    TestReadonlyOptionsFailOnSetsockopt();
    TestSocketTypeReadback();
    TestSocketErrorOptionReadbackAndClear();
    TestBooleanOptionToggling();
    return 0;
}
