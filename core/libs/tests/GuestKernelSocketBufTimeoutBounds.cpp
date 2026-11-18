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

constexpr int SoSndBuf = 0x1001;
constexpr int SoRcvBuf = 0x1002;
constexpr int SoSndTimeo = 0x1005;
constexpr int SoRcvTimeo = 0x1006;

constexpr int PosixEbadf = 9;
constexpr int PosixEfault = 14;
constexpr int PosixEinval = 22;
constexpr int PosixEdom = 33;
constexpr int PosixEnotsock = 38;
constexpr int PosixEnobufs = 55;

constexpr int MaximumBuffer = 524288;
constexpr int MaximumReservation = 5592405;

struct GuestTimeval {
    std::int64_t seconds;
    std::int64_t microseconds;
};

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

GuestTimeval GetTimeoutOption(int descriptor, int option) {
    GuestTimeval time{-1, -1};
    std::uint32_t length = sizeof(time);
    Require(getsockopt_nid_postfix(descriptor, SolSocket, option, &time, &length) == 0);
    Require(length == sizeof(time));
    return time;
}

GuestTimeval GetTimeoutOptionAlt(int descriptor, int option) {
    GuestTimeval time{-1, -1};
    std::uint32_t length = sizeof(time);
    Require(_getsockopt_nid_postfix(descriptor, SolSocket, option, &time, &length) == 0);
    Require(length == sizeof(time));
    return time;
}

void SetIntOption(int descriptor, int option, int value) {
    Require(setsockopt_nid_postfix(descriptor, SolSocket, option, &value, sizeof(value)) == 0);
}

void SetIntOptionAlt(int descriptor, int option, int value) {
    Require(_setsockopt_nid_postfix(descriptor, SolSocket, option, &value, sizeof(value)) == 0);
}

void SetTimeoutOption(int descriptor, int option, GuestTimeval value) {
    Require(setsockopt_nid_postfix(descriptor, SolSocket, option, &value, sizeof(value)) == 0);
}

void SetTimeoutOptionAlt(int descriptor, int option, GuestTimeval value) {
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

void TestBufferOptionBoundsAndClamping() {
    int pipeFds[2]{-1, -1};
    Require(pipe_nid_postfix(pipeFds) == 0);

    const int stream = socket_nid_postfix(AfInet, SockStream, 0);
    const int dgram = socket_nid_postfix(AfInet, SockDgram, 0);
    Require(stream >= 0 && dgram >= 0);

    Require(GetIntOption(stream, SoSndBuf) == 32768);
    Require(GetIntOption(stream, SoRcvBuf) == 65536);
    Require(GetIntOption(dgram, SoSndBuf) == 9216);
    Require(GetIntOption(dgram, SoRcvBuf) == 42080);

    int unixStream[2]{-1, -1};
    Require(socketpair_nid_postfix(AfUnix, SockStream, 0, unixStream) == 0);
    Require(GetIntOption(unixStream[0], SoSndBuf) == 8192);
    Require(GetIntOption(unixStream[0], SoRcvBuf) == 8192);
    Require(close_nid_postfix(unixStream[0]) == 0);
    Require(close_nid_postfix(unixStream[1]) == 0);

    int unixDgram[2]{-1, -1};
    Require(socketpair_nid_postfix(AfUnix, SockDgram, 0, unixDgram) == 0);
    Require(GetIntOption(unixDgram[0], SoSndBuf) == 2048);
    Require(GetIntOption(unixDgram[0], SoRcvBuf) == 4096);
    Require(close_nid_postfix(unixDgram[0]) == 0);
    Require(close_nid_postfix(unixDgram[1]) == 0);

    const int v6Dgram = socket_nid_postfix(AfInet6, SockDgram, 0);
    if (v6Dgram >= 0) {
        Require(GetIntOption(v6Dgram, SoSndBuf) == 9216);
        Require(GetIntOption(v6Dgram, SoRcvBuf) == 42080);
        Require(close_nid_postfix(v6Dgram) == 0);
    }

    const int bufferOptions[] = {SoSndBuf, SoRcvBuf};

    for (const int opt : bufferOptions) {
        int val = 1024;
        Require(SetsockoptFails(-1, SolSocket, opt, nullptr, sizeof(val), PosixEfault));
        Require(SetsockoptAltFails(-1, SolSocket, opt, nullptr, sizeof(val), PosixEfault));
        Require(SetsockoptFails(stream, SolSocket, opt, nullptr, sizeof(val), PosixEfault));

        Require(SetsockoptFails(-1, SolSocket, opt, &val, static_cast<std::uint32_t>(-1), PosixEinval));
        Require(SetsockoptFails(stream, SolSocket, opt, &val, static_cast<std::uint32_t>(-1), PosixEinval));

        Require(SetsockoptFails(-1, SolSocket, opt, &val, sizeof(val), PosixEbadf));
        Require(SetsockoptAltFails(-1, SolSocket, opt, &val, sizeof(val), PosixEbadf));
        Require(SetsockoptFails(99999, SolSocket, opt, &val, sizeof(val), PosixEbadf));

        Require(SetsockoptFails(pipeFds[0], SolSocket, opt, &val, sizeof(val), PosixEnotsock));
        Require(SetsockoptAltFails(pipeFds[0], SolSocket, opt, &val, sizeof(val), PosixEnotsock));

        Require(SetsockoptFails(stream, 77777, opt, &val, sizeof(val), PosixEinval));
        Require(SetsockoptAltFails(stream, 77777, opt, &val, sizeof(val), PosixEinval));

        for (std::uint32_t shortLen = 0; shortLen < sizeof(int); ++shortLen) {
            Require(SetsockoptFails(stream, SolSocket, opt, &val, shortLen, PosixEinval));
            Require(SetsockoptAltFails(stream, SolSocket, opt, &val, shortLen, PosixEinval));
        }

        for (const int invalidVal : {0, -1, -50, -32768, -2147483647}) {
            Require(SetsockoptFails(stream, SolSocket, opt, &invalidVal, sizeof(invalidVal), PosixEinval));
            Require(SetsockoptAltFails(stream, SolSocket, opt, &invalidVal, sizeof(invalidVal), PosixEinval));
        }

        for (const int overflowVal : {MaximumReservation + 1, 6000000, 10000000}) {
            Require(SetsockoptFails(stream, SolSocket, opt, &overflowVal, sizeof(overflowVal), PosixEnobufs));
            Require(SetsockoptAltFails(stream, SolSocket, opt, &overflowVal, sizeof(overflowVal), PosixEnobufs));
        }

        SetIntOption(stream, opt, 1);
        Require(GetIntOption(stream, opt) == 1);
        Require(GetIntOptionAlt(stream, opt) == 1);

        SetIntOptionAlt(stream, opt, 16384);
        Require(GetIntOption(stream, opt) == 16384);

        SetIntOption(stream, opt, MaximumBuffer);
        Require(GetIntOption(stream, opt) == MaximumBuffer);

        SetIntOption(stream, opt, MaximumBuffer + 500);
        Require(GetIntOption(stream, opt) == MaximumBuffer);

        SetIntOptionAlt(stream, opt, MaximumReservation);
        Require(GetIntOption(stream, opt) == MaximumBuffer);

        int out = 0;
        std::uint32_t len = sizeof(out);
        Require(GetsockoptFails(stream, SolSocket, opt, &out, nullptr, PosixEfault));
        Require(GetsockoptAltFails(stream, SolSocket, opt, &out, nullptr, PosixEfault));
        Require(GetsockoptFails(-1, SolSocket, opt, &out, nullptr, PosixEfault));

        len = sizeof(out);
        Require(GetsockoptFails(-1, SolSocket, opt, &out, &len, PosixEbadf));
        Require(GetsockoptFails(pipeFds[0], SolSocket, opt, &out, &len, PosixEnotsock));
        Require(GetsockoptFails(stream, 77777, opt, &out, &len, PosixEinval));

        len = 0;
        int canary = 0x44444444;
        Require(getsockopt_nid_postfix(stream, SolSocket, opt, &canary, &len) == 0 && len == 0 && canary == 0x44444444);
        len = 0;
        canary = 0x55555555;
        Require(_getsockopt_nid_postfix(stream, SolSocket, opt, &canary, &len) == 0 && len == 0 && canary == 0x55555555);

        len = sizeof(out);
        Require(getsockopt_nid_postfix(stream, SolSocket, opt, nullptr, &len) == 0 && len == 0);
        len = sizeof(out);
        Require(_getsockopt_nid_postfix(stream, SolSocket, opt, nullptr, &len) == 0 && len == 0);

        std::uint8_t byteBuf = 0xee;
        len = 1;
        Require(getsockopt_nid_postfix(stream, SolSocket, opt, &byteBuf, &len) == 0 && len == 1);
        std::uint16_t wordBuf = 0xeeee;
        len = 2;
        Require(getsockopt_nid_postfix(stream, SolSocket, opt, &wordBuf, &len) == 0 && len == 2);
    }

    Require(close_nid_postfix(stream) == 0);
    Require(close_nid_postfix(dgram) == 0);
    Require(close_nid_postfix(pipeFds[0]) == 0);
    Require(close_nid_postfix(pipeFds[1]) == 0);
}

void TestTimeoutOptionBoundsAndTimevalValidation() {
    int pipeFds[2]{-1, -1};
    Require(pipe_nid_postfix(pipeFds) == 0);

    const int sock = socket_nid_postfix(AfInet, SockStream, 0);
    Require(sock >= 0);

    const int timeoutOptions[] = {SoSndTimeo, SoRcvTimeo};

    for (const int opt : timeoutOptions) {
        const GuestTimeval initial = GetTimeoutOption(sock, opt);
        Require(initial.seconds == 0 && initial.microseconds == 0);
        const GuestTimeval initialAlt = GetTimeoutOptionAlt(sock, opt);
        Require(initialAlt.seconds == 0 && initialAlt.microseconds == 0);

        GuestTimeval validTime{5, 250000};
        Require(SetsockoptFails(-1, SolSocket, opt, nullptr, sizeof(validTime), PosixEfault));
        Require(SetsockoptAltFails(-1, SolSocket, opt, nullptr, sizeof(validTime), PosixEfault));
        Require(SetsockoptFails(sock, SolSocket, opt, nullptr, sizeof(validTime), PosixEfault));

        Require(SetsockoptFails(-1, SolSocket, opt, &validTime, static_cast<std::uint32_t>(-1), PosixEinval));
        Require(SetsockoptFails(sock, SolSocket, opt, &validTime, static_cast<std::uint32_t>(-1), PosixEinval));

        Require(SetsockoptFails(-1, SolSocket, opt, &validTime, sizeof(validTime), PosixEbadf));
        Require(SetsockoptAltFails(-1, SolSocket, opt, &validTime, sizeof(validTime), PosixEbadf));
        Require(SetsockoptFails(99999, SolSocket, opt, &validTime, sizeof(validTime), PosixEbadf));

        Require(SetsockoptFails(pipeFds[0], SolSocket, opt, &validTime, sizeof(validTime), PosixEnotsock));
        Require(SetsockoptAltFails(pipeFds[0], SolSocket, opt, &validTime, sizeof(validTime), PosixEnotsock));

        Require(SetsockoptFails(sock, 88888, opt, &validTime, sizeof(validTime), PosixEinval));
        Require(SetsockoptAltFails(sock, 88888, opt, &validTime, sizeof(validTime), PosixEinval));

        for (std::uint32_t shortLen = 0; shortLen < sizeof(GuestTimeval); ++shortLen) {
            Require(SetsockoptFails(sock, SolSocket, opt, &validTime, shortLen, PosixEinval));
            Require(SetsockoptAltFails(sock, SolSocket, opt, &validTime, shortLen, PosixEinval));
        }

        const GuestTimeval invalidTimes[] = {
            {-1, 0},
            {-100, 500000},
            {0, -1},
            {10, -500},
            {0, 1000000},
            {0, 1000001},
            {15, 2000000},
            {-5, -1},
            {-5, 1000000}
        };

        for (const auto& badTime : invalidTimes) {
            Require(SetsockoptFails(sock, SolSocket, opt, &badTime, sizeof(badTime), PosixEdom));
            Require(SetsockoptAltFails(sock, SolSocket, opt, &badTime, sizeof(badTime), PosixEdom));
        }

        SetTimeoutOption(sock, opt, {1, 500000});
        GuestTimeval readBack = GetTimeoutOption(sock, opt);
        Require(readBack.seconds == 1);
        Require(readBack.microseconds >= 499000 && readBack.microseconds <= 501000);

        SetTimeoutOptionAlt(sock, opt, {10, 0});
        readBack = GetTimeoutOptionAlt(sock, opt);
        Require(readBack.seconds == 10 && readBack.microseconds == 0);

        SetTimeoutOption(sock, opt, {0, 0});
        readBack = GetTimeoutOption(sock, opt);
        Require(readBack.seconds == 0 && readBack.microseconds == 0);

        std::array<std::uint8_t, 32> wideBuffer{};
        GuestTimeval wideTime{3, 0};
        std::memcpy(wideBuffer.data(), &wideTime, sizeof(wideTime));
        Require(setsockopt_nid_postfix(sock, SolSocket, opt, wideBuffer.data(), wideBuffer.size()) == 0);
        readBack = GetTimeoutOption(sock, opt);
        Require(readBack.seconds == 3 && readBack.microseconds == 0);

        GuestTimeval outTime{-1, -1};
        std::uint32_t len = sizeof(outTime);
        Require(GetsockoptFails(sock, SolSocket, opt, &outTime, nullptr, PosixEfault));
        Require(GetsockoptAltFails(sock, SolSocket, opt, &outTime, nullptr, PosixEfault));
        Require(GetsockoptFails(-1, SolSocket, opt, &outTime, nullptr, PosixEfault));

        len = sizeof(outTime);
        Require(GetsockoptFails(-1, SolSocket, opt, &outTime, &len, PosixEbadf));
        Require(GetsockoptFails(pipeFds[0], SolSocket, opt, &outTime, &len, PosixEnotsock));
        Require(GetsockoptFails(sock, 88888, opt, &outTime, &len, PosixEinval));

        len = 0;
        GuestTimeval canary{-123, -456};
        Require(getsockopt_nid_postfix(sock, SolSocket, opt, &canary, &len) == 0 && len == 0);
        Require(canary.seconds == -123 && canary.microseconds == -456);

        len = sizeof(outTime);
        Require(getsockopt_nid_postfix(sock, SolSocket, opt, nullptr, &len) == 0 && len == 0);

        std::array<std::uint8_t, 8> truncatedBuf{};
        len = truncatedBuf.size();
        Require(getsockopt_nid_postfix(sock, SolSocket, opt, truncatedBuf.data(), &len) == 0 && len == 8);
        std::int64_t readSeconds = 0;
        std::memcpy(&readSeconds, truncatedBuf.data(), sizeof(readSeconds));
        Require(readSeconds == 3);

        SetTimeoutOption(sock, opt, {0, 0});
    }

    Require(close_nid_postfix(sock) == 0);
    Require(close_nid_postfix(pipeFds[0]) == 0);
    Require(close_nid_postfix(pipeFds[1]) == 0);

    for (const int opt : timeoutOptions) {
        GuestTimeval validTime{1, 0};
        Require(SetsockoptFails(sock, SolSocket, opt, &validTime, sizeof(validTime), PosixEbadf));
        GuestTimeval outTime{};
        std::uint32_t len = sizeof(outTime);
        Require(GetsockoptFails(sock, SolSocket, opt, &outTime, &len, PosixEbadf));
    }
}

} // namespace

int main() {
    TestBufferOptionBoundsAndClamping();
    TestTimeoutOptionBoundsAndTimevalValidation();
    return 0;
}
