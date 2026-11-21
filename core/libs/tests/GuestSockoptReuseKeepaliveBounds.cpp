#include "prx/libc/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>

namespace {

constexpr int ErrorBadDescriptor = 9;
constexpr int ErrorFault = 14;
constexpr int ErrorInvalid = 22;
constexpr int ErrorNotSocket = 38;
constexpr int ErrorNoProtocolOption = 42;

constexpr int SolSocket = 0xffff;
constexpr int SoReuseAddr = 0x04;
constexpr int SoKeepAlive = 0x08;
constexpr int SoReusePort = 0x200;

constexpr int FamilyInet = 2;
constexpr int FamilyInet6 = 28;
constexpr int SocketStream = 1;
constexpr int SocketDatagram = 2;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "check failed at line %u, errno %d\n", location.line(), *__error_nid_postfix());
    std::abort();
}

bool Fails(int result, int error) {
    return result == -1 && *__error_nid_postfix() == error;
}

std::array<std::uint8_t, 16> MakeV4(std::uint16_t port = 0) {
    std::array<std::uint8_t, 16> address{};
    address[0] = 16;
    address[1] = 2;
    address[2] = static_cast<std::uint8_t>(port >> 8);
    address[3] = static_cast<std::uint8_t>(port);
    address[4] = 127;
    address[7] = 1;
    return address;
}

void TestOptionTogglingAndReadback() {
    const int stream4 = socket_nid_postfix(FamilyInet, SocketStream, 0);
    const int dgram4 = socket_nid_postfix(FamilyInet, SocketDatagram, 0);
    const int stream6 = socket_nid_postfix(FamilyInet6, SocketStream, 0);
    const int dgram6 = socket_nid_postfix(FamilyInet6, SocketDatagram, 0);
    Require(stream4 >= 0 && dgram4 >= 0 && stream6 >= 0 && dgram6 >= 0);

    const int allDescriptors[] = {stream4, dgram4, stream6, dgram6};
    for (const int fd : allDescriptors) {
        int outVal = -1;
        std::uint32_t len = sizeof(outVal);
        Require(getsockopt_nid_postfix(fd, SolSocket, SoReuseAddr, &outVal, &len) == 0 && len == 4 && outVal == 0);

        outVal = -1;
        len = sizeof(outVal);
        Require(getsockopt_nid_postfix(fd, SolSocket, SoReusePort, &outVal, &len) == 0 && len == 4 && outVal == 0);

        int setVal = 1;
        Require(setsockopt_nid_postfix(fd, SolSocket, SoReuseAddr, &setVal, sizeof(setVal)) == 0);
        outVal = 0;
        len = sizeof(outVal);
        Require(getsockopt_nid_postfix(fd, SolSocket, SoReuseAddr, &outVal, &len) == 0 && len == 4 && outVal == SoReuseAddr);

        setVal = 0;
        Require(setsockopt_nid_postfix(fd, SolSocket, SoReuseAddr, &setVal, sizeof(setVal)) == 0);
        outVal = -1;
        len = sizeof(outVal);
        Require(getsockopt_nid_postfix(fd, SolSocket, SoReuseAddr, &outVal, &len) == 0 && len == 4 && outVal == 0);

        setVal = -1;
        Require(setsockopt_nid_postfix(fd, SolSocket, SoReuseAddr, &setVal, sizeof(setVal)) == 0);
        outVal = 0;
        len = sizeof(outVal);
        Require(getsockopt_nid_postfix(fd, SolSocket, SoReuseAddr, &outVal, &len) == 0 && len == 4 && outVal == SoReuseAddr);
        setVal = 0;
        Require(setsockopt_nid_postfix(fd, SolSocket, SoReuseAddr, &setVal, sizeof(setVal)) == 0);

        setVal = 1;
        Require(setsockopt_nid_postfix(fd, SolSocket, SoReusePort, &setVal, sizeof(setVal)) == 0);
        outVal = 0;
        len = sizeof(outVal);
        Require(getsockopt_nid_postfix(fd, SolSocket, SoReusePort, &outVal, &len) == 0 && len == 4 && outVal == SoReusePort);

        setVal = 0;
        Require(setsockopt_nid_postfix(fd, SolSocket, SoReusePort, &setVal, sizeof(setVal)) == 0);
        outVal = -1;
        len = sizeof(outVal);
        Require(getsockopt_nid_postfix(fd, SolSocket, SoReusePort, &outVal, &len) == 0 && len == 4 && outVal == 0);

        setVal = 1;
        Require(setsockopt_nid_postfix(fd, SolSocket, SoReuseAddr, &setVal, sizeof(setVal)) == 0);
        Require(setsockopt_nid_postfix(fd, SolSocket, SoReusePort, &setVal, sizeof(setVal)) == 0);

        int ra = 0;
        int rp = 0;
        len = sizeof(int);
        Require(getsockopt_nid_postfix(fd, SolSocket, SoReuseAddr, &ra, &len) == 0 && ra == SoReuseAddr);
        len = sizeof(int);
        Require(getsockopt_nid_postfix(fd, SolSocket, SoReusePort, &rp, &len) == 0 && rp == SoReusePort);

        setVal = 0;
        Require(setsockopt_nid_postfix(fd, SolSocket, SoReuseAddr, &setVal, sizeof(setVal)) == 0);
        len = sizeof(int);
        Require(getsockopt_nid_postfix(fd, SolSocket, SoReuseAddr, &ra, &len) == 0 && ra == 0);
        len = sizeof(int);
        Require(getsockopt_nid_postfix(fd, SolSocket, SoReusePort, &rp, &len) == 0 && rp == SoReusePort);

        Require(setsockopt_nid_postfix(fd, SolSocket, SoReusePort, &setVal, sizeof(setVal)) == 0);
        len = sizeof(int);
        Require(getsockopt_nid_postfix(fd, SolSocket, SoReusePort, &rp, &len) == 0 && rp == 0);
    }

    const int streamDescriptors[] = {stream4, stream6};
    for (const int fd : streamDescriptors) {
        int outVal = -1;
        std::uint32_t len = sizeof(outVal);
        Require(getsockopt_nid_postfix(fd, SolSocket, SoKeepAlive, &outVal, &len) == 0 && len == 4 && outVal == 0);

        int setVal = 1;
        Require(setsockopt_nid_postfix(fd, SolSocket, SoKeepAlive, &setVal, sizeof(setVal)) == 0);
        outVal = 0;
        len = sizeof(outVal);
        Require(getsockopt_nid_postfix(fd, SolSocket, SoKeepAlive, &outVal, &len) == 0 && len == 4 && outVal == SoKeepAlive);

        setVal = 0;
        Require(setsockopt_nid_postfix(fd, SolSocket, SoKeepAlive, &setVal, sizeof(setVal)) == 0);
        outVal = -1;
        len = sizeof(outVal);
        Require(getsockopt_nid_postfix(fd, SolSocket, SoKeepAlive, &outVal, &len) == 0 && len == 4 && outVal == 0);

        setVal = 1;
        Require(setsockopt_nid_postfix(fd, SolSocket, SoReuseAddr, &setVal, sizeof(setVal)) == 0);
        Require(setsockopt_nid_postfix(fd, SolSocket, SoReusePort, &setVal, sizeof(setVal)) == 0);
        Require(setsockopt_nid_postfix(fd, SolSocket, SoKeepAlive, &setVal, sizeof(setVal)) == 0);

        int ra = 0;
        int rp = 0;
        int ka = 0;
        len = sizeof(int);
        Require(getsockopt_nid_postfix(fd, SolSocket, SoReuseAddr, &ra, &len) == 0 && ra == SoReuseAddr);
        len = sizeof(int);
        Require(getsockopt_nid_postfix(fd, SolSocket, SoReusePort, &rp, &len) == 0 && rp == SoReusePort);
        len = sizeof(int);
        Require(getsockopt_nid_postfix(fd, SolSocket, SoKeepAlive, &ka, &len) == 0 && ka == SoKeepAlive);

        setVal = 0;
        Require(setsockopt_nid_postfix(fd, SolSocket, SoReuseAddr, &setVal, sizeof(setVal)) == 0);
        Require(setsockopt_nid_postfix(fd, SolSocket, SoReusePort, &setVal, sizeof(setVal)) == 0);
        Require(setsockopt_nid_postfix(fd, SolSocket, SoKeepAlive, &setVal, sizeof(setVal)) == 0);
    }

    int testVal = 1;
    Require(_setsockopt_nid_postfix(stream4, SolSocket, SoReuseAddr, &testVal, sizeof(testVal)) == 0);
    int readVal = 0;
    std::uint32_t readLen = sizeof(readVal);
    Require(_getsockopt_nid_postfix(stream4, SolSocket, SoReuseAddr, &readVal, &readLen) == 0 && readVal == SoReuseAddr);
    testVal = 0;
    Require(_setsockopt_nid_postfix(stream4, SolSocket, SoReuseAddr, &testVal, sizeof(testVal)) == 0);

    Require(close_nid_postfix(stream4) == 0);
    Require(close_nid_postfix(dgram4) == 0);
    Require(close_nid_postfix(stream6) == 0);
    Require(close_nid_postfix(dgram6) == 0);
}

void TestLengthValidationAndTruncation() {
    const int socket = socket_nid_postfix(FamilyInet, SocketStream, 0);
    Require(socket >= 0);

    const int options[] = {SoReuseAddr, SoReusePort, SoKeepAlive};
    const int value = 1;

    for (const int opt : options) {
        Require(Fails(setsockopt_nid_postfix(socket, SolSocket, opt, &value, 0), ErrorInvalid));
        Require(Fails(setsockopt_nid_postfix(socket, SolSocket, opt, &value, 1), ErrorInvalid));
        Require(Fails(setsockopt_nid_postfix(socket, SolSocket, opt, &value, 2), ErrorInvalid));
        Require(Fails(setsockopt_nid_postfix(socket, SolSocket, opt, &value, 3), ErrorInvalid));

        Require(setsockopt_nid_postfix(socket, SolSocket, opt, &value, 4) == 0);
        const std::uint64_t largeVal = 1;
        Require(setsockopt_nid_postfix(socket, SolSocket, opt, &largeVal, sizeof(largeVal)) == 0);

        std::uint32_t len = sizeof(int);
        int intOutput = 0;
        Require(getsockopt_nid_postfix(socket, SolSocket, opt, &intOutput, &len) == 0 && len == 4 && intOutput == opt);

        std::uint32_t lenNull = 4;
        Require(getsockopt_nid_postfix(socket, SolSocket, opt, nullptr, &lenNull) == -1 && *__error_nid_postfix() == ErrorFault);

        const int zero = 0;
        Require(setsockopt_nid_postfix(socket, SolSocket, opt, &zero, sizeof(zero)) == 0);
    }

    Require(close_nid_postfix(socket) == 0);
}

void TestErrorPrecedenceAndDescriptorValidation() {
    const int val = 1;
    for (const int opt : {SoReuseAddr, SoReusePort, SoKeepAlive}) {
        Require(Fails(setsockopt_nid_postfix(-1, SolSocket, opt, nullptr, 4), ErrorFault));
        Require(Fails(setsockopt_nid_postfix(-10, SolSocket, opt, nullptr, 4), ErrorFault));

        Require(Fails(setsockopt_nid_postfix(-1, SolSocket, opt, &val, static_cast<std::uint32_t>(-1)), ErrorInvalid));
        Require(Fails(setsockopt_nid_postfix(-1, SolSocket, opt, &val, 0x80000000u), ErrorInvalid));

        Require(Fails(setsockopt_nid_postfix(-1, SolSocket, opt, &val, sizeof(val)), ErrorBadDescriptor));
        Require(Fails(setsockopt_nid_postfix(-100, SolSocket, opt, &val, sizeof(val)), ErrorBadDescriptor));
        Require(Fails(setsockopt_nid_postfix(1024, SolSocket, opt, &val, sizeof(val)), ErrorBadDescriptor));

        int out = 0;
        Require(Fails(getsockopt_nid_postfix(-1, SolSocket, opt, &out, nullptr), ErrorFault));
        Require(Fails(getsockopt_nid_postfix(-100, SolSocket, opt, &out, nullptr), ErrorFault));

        std::uint32_t len = sizeof(out);
        Require(Fails(getsockopt_nid_postfix(-1, SolSocket, opt, &out, &len), ErrorBadDescriptor));
        Require(Fails(getsockopt_nid_postfix(-100, SolSocket, opt, &out, &len), ErrorBadDescriptor));
        Require(Fails(getsockopt_nid_postfix(1024, SolSocket, opt, &out, &len), ErrorBadDescriptor));
    }

    const int closedSocket = socket_nid_postfix(FamilyInet, SocketStream, 0);
    Require(closedSocket >= 0);
    Require(close_nid_postfix(closedSocket) == 0);

    for (const int opt : {SoReuseAddr, SoReusePort, SoKeepAlive}) {
        Require(Fails(setsockopt_nid_postfix(closedSocket, SolSocket, opt, &val, sizeof(val)), ErrorBadDescriptor));
        int out = 0;
        std::uint32_t len = sizeof(out);
        Require(Fails(getsockopt_nid_postfix(closedSocket, SolSocket, opt, &out, &len), ErrorBadDescriptor));
    }

    int pipefds[2]{-1, -1};
    Require(pipe_nid_postfix(pipefds) == 0);
    Require(pipefds[0] >= 0 && pipefds[1] >= 0);

    for (const int pipefd : pipefds) {
        for (const int opt : {SoReuseAddr, SoReusePort, SoKeepAlive}) {
            Require(Fails(setsockopt_nid_postfix(pipefd, SolSocket, opt, &val, sizeof(val)), ErrorNotSocket));
            int out = 0;
            std::uint32_t len = sizeof(out);
            Require(Fails(getsockopt_nid_postfix(pipefd, SolSocket, opt, &out, &len), ErrorNotSocket));
        }
    }

    Require(close_nid_postfix(pipefds[0]) == 0);
    Require(close_nid_postfix(pipefds[1]) == 0);

    for (const int pipefd : pipefds) {
        for (const int opt : {SoReuseAddr, SoReusePort, SoKeepAlive}) {
            Require(Fails(setsockopt_nid_postfix(pipefd, SolSocket, opt, &val, sizeof(val)), ErrorBadDescriptor));
            int out = 0;
            std::uint32_t len = sizeof(out);
            Require(Fails(getsockopt_nid_postfix(pipefd, SolSocket, opt, &out, &len), ErrorBadDescriptor));
        }
    }

    const int liveSocket = socket_nid_postfix(FamilyInet, SocketStream, 0);
    Require(liveSocket >= 0);

    for (const int badLevel : {-1, 1, 2, 3, 5, 7, 40, 42, 99999}) {
        Require(Fails(setsockopt_nid_postfix(liveSocket, badLevel, SoReuseAddr, &val, sizeof(val)), ErrorInvalid));
        int out = 0;
        std::uint32_t len = sizeof(out);
        Require(Fails(getsockopt_nid_postfix(liveSocket, badLevel, SoReuseAddr, &out, &len), ErrorInvalid));
    }

    for (const int badOption : {-1, 0, 0x7fff, 99999}) {
        Require(Fails(setsockopt_nid_postfix(liveSocket, SolSocket, badOption, &val, sizeof(val)), ErrorNoProtocolOption));
        int out = 0;
        std::uint32_t len = sizeof(out);
        Require(Fails(getsockopt_nid_postfix(liveSocket, SolSocket, badOption, &out, &len), ErrorNoProtocolOption));
    }

    Require(close_nid_postfix(liveSocket) == 0);
}

void TestBindingWithReuse() {
    const int dgram1 = socket_nid_postfix(FamilyInet, SocketDatagram, 0);
    const int dgram2 = socket_nid_postfix(FamilyInet, SocketDatagram, 0);
    Require(dgram1 >= 0 && dgram2 >= 0);

    const int on = 1;
    Require(setsockopt_nid_postfix(dgram1, SolSocket, SoReuseAddr, &on, sizeof(on)) == 0);
    Require(setsockopt_nid_postfix(dgram1, SolSocket, SoReusePort, &on, sizeof(on)) == 0);
    Require(setsockopt_nid_postfix(dgram2, SolSocket, SoReuseAddr, &on, sizeof(on)) == 0);
    Require(setsockopt_nid_postfix(dgram2, SolSocket, SoReusePort, &on, sizeof(on)) == 0);

    auto addr1 = MakeV4(0);
    Require(bind_nid_postfix(dgram1, addr1.data(), addr1.size()) == 0);

    std::uint32_t size = addr1.size();
    Require(getsockname_nid_postfix(dgram1, addr1.data(), &size) == 0 && size == 16);

    auto addr2 = MakeV4(0);
    addr2[2] = addr1[2];
    addr2[3] = addr1[3];
    Require(bind_nid_postfix(dgram2, addr2.data(), addr2.size()) == 0);

    Require(close_nid_postfix(dgram1) == 0);
    Require(close_nid_postfix(dgram2) == 0);
}

} // namespace

int main() {
    TestOptionTogglingAndReadback();
    TestLengthValidationAndTruncation();
    TestErrorPrecedenceAndDescriptorValidation();
    TestBindingWithReuse();
    return 0;
}
