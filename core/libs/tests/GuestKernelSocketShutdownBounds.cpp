#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libkernel/include/Api.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

constexpr int AfUnix = 1;
constexpr int AfInet = 2;
constexpr int SockStream = 1;
constexpr int SockDgram = 2;

constexpr int SolSocket = 0xffff;
constexpr int SoLinger = 0x80;

constexpr int ShutRd = 0;
constexpr int ShutWr = 1;
constexpr int ShutRdWr = 2;

constexpr int ErrorBadDescriptor = 9;
constexpr int ErrorFault = 14;
constexpr int ErrorInvalid = 22;
constexpr int ErrorPipe = 32;
constexpr int ErrorDomain = 33;
constexpr int ErrorNotSocket = 38;
constexpr int ErrorNotConnected = 57;

struct Linger {
    std::int32_t enabled;
    std::int32_t seconds;
};

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

bool FailsWith(long result, int expectedError) {
    const int actual = *__error_nid_postfix();
    return result == -1 && actual == expectedError && actual >= 1 && actual <= 96;
}

class GuardedPage {
public:
    std::size_t size;
    unsigned char* data;

    GuardedPage() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        size = info.dwPageSize;
        data = static_cast<unsigned char*>(VirtualAlloc(nullptr, size * 2, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        if (data == nullptr) {
            std::abort();
        }
        DWORD previous = 0;
        if (VirtualProtect(data + size, size, PAGE_NOACCESS, &previous) == 0) {
            std::abort();
        }
#else
        const long pageSize = sysconf(_SC_PAGESIZE);
        if (pageSize <= 0) {
            std::abort();
        }
        size = static_cast<std::size_t>(pageSize);
        data = static_cast<unsigned char*>(mmap(nullptr, size * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
        if (data == MAP_FAILED) {
            std::abort();
        }
        if (mprotect(data + size, size, PROT_NONE) != 0) {
            std::abort();
        }
#endif
    }

    void SetFirstPageReadOnly(bool readOnly) {
#ifdef _WIN32
        DWORD previous = 0;
        const DWORD protect = readOnly ? PAGE_READONLY : PAGE_READWRITE;
        if (VirtualProtect(data, size, protect, &previous) == 0) {
            std::abort();
        }
#else
        const int protect = readOnly ? PROT_READ : (PROT_READ | PROT_WRITE);
        if (mprotect(data, size, protect) != 0) {
            std::abort();
        }
#endif
    }

    ~GuardedPage() {
#ifdef _WIN32
        VirtualFree(data, 0, MEM_RELEASE);
#else
        munmap(data, size * 2);
#endif
    }
};

std::array<std::uint8_t, 16> CreateLoopbackAddress(std::uint16_t portNetworkOrder = 0) {
    std::array<std::uint8_t, 16> address{16, AfInet};
    std::memcpy(address.data() + 2, &portNetworkOrder, 2);
    address[4] = 127;
    address[7] = 1;
    return address;
}

int CreateListeningTcpSocket(std::array<std::uint8_t, 16>& boundAddress) {
    const int listener = socket_nid_postfix(AfInet, SockStream, 0);
    Require(listener >= 0);
    boundAddress = CreateLoopbackAddress(0);
    Require(bind_nid_postfix(listener, boundAddress.data(), boundAddress.size()) == 0);
    Require(listen_nid_postfix(listener, 4) == 0);
    std::uint32_t size = boundAddress.size();
    Require(getsockname_nid_postfix(listener, boundAddress.data(), &size) == 0);
    return listener;
}

struct TcpPair {
    int listener = -1;
    int client = -1;
    int server = -1;

    TcpPair() {
        std::array<std::uint8_t, 16> address{};
        listener = CreateListeningTcpSocket(address);
        client = socket_nid_postfix(AfInet, SockStream, 0);
        Require(client >= 0);
        Require(connect_nid_postfix(client, address.data(), address.size()) == 0);
        server = accept_nid_postfix(listener, nullptr, nullptr);
        Require(server >= 0);
    }

    ~TcpPair() {
        if (client >= 0) {
            close_nid_postfix(client);
        }
        if (server >= 0) {
            close_nid_postfix(server);
        }
        if (listener >= 0) {
            close_nid_postfix(listener);
        }
    }
};

void TestLingerParameterBounds() {
    const int stream = socket_nid_postfix(AfInet, SockStream, 0);
    Require(stream >= 0);

    Linger validLinger{1, 5};
    Require(FailsWith(setsockopt_nid_postfix(-1, SolSocket, SoLinger, &validLinger, sizeof(validLinger)), ErrorBadDescriptor));
    Require(FailsWith(setsockopt_nid_postfix(-100, SolSocket, SoLinger, &validLinger, sizeof(validLinger)), ErrorBadDescriptor));
    Require(FailsWith(setsockopt_nid_postfix(65535, SolSocket, SoLinger, &validLinger, sizeof(validLinger)), ErrorBadDescriptor));

    int ends[2]{-1, -1};
    Require(pipe_nid_postfix(ends) == 0);
    Require(FailsWith(setsockopt_nid_postfix(ends[0], SolSocket, SoLinger, &validLinger, sizeof(validLinger)), ErrorNotSocket));
    Require(FailsWith(setsockopt_nid_postfix(ends[1], SolSocket, SoLinger, &validLinger, sizeof(validLinger)), ErrorNotSocket));
    Require(close_nid_postfix(ends[0]) == 0);
    Require(close_nid_postfix(ends[1]) == 0);

    Require(FailsWith(setsockopt_nid_postfix(stream, SolSocket, SoLinger, nullptr, sizeof(validLinger)), ErrorFault));
    Require(FailsWith(setsockopt_nid_postfix(stream, SolSocket, SoLinger, reinterpret_cast<const void*>(std::uintptr_t{0x10}), sizeof(validLinger)), ErrorFault));

    GuardedPage guard;
    Require(FailsWith(setsockopt_nid_postfix(stream, SolSocket, SoLinger, guard.data + guard.size, sizeof(validLinger)), ErrorFault));
    Require(FailsWith(setsockopt_nid_postfix(stream, SolSocket, SoLinger, guard.data + guard.size - 2, sizeof(validLinger)), ErrorFault));

    for (const std::uint32_t shortLen : {0u, 1u, 2u, 3u, 4u, 7u}) {
        Require(FailsWith(setsockopt_nid_postfix(stream, SolSocket, SoLinger, &validLinger, shortLen), ErrorInvalid));
    }

    const int negativeSeconds[] = {-1, -2, -50, -1000, -65535};
    for (const int sec : negativeSeconds) {
        Linger badSec{1, sec};
        Require(FailsWith(setsockopt_nid_postfix(stream, SolSocket, SoLinger, &badSec, sizeof(badSec)), ErrorDomain));
    }

    const int excessiveSeconds[] = {65536, 65537, 100000, 0x7fffffff};
    for (const int sec : excessiveSeconds) {
        Linger badSec{1, sec};
        Require(FailsWith(setsockopt_nid_postfix(stream, SolSocket, SoLinger, &badSec, sizeof(badSec)), ErrorDomain));
    }

    const int validSeconds[] = {0, 1, 10, 60, 327, 65535};
    for (const int sec : validSeconds) {
        Linger testSec{1, sec};
        Require(setsockopt_nid_postfix(stream, SolSocket, SoLinger, &testSec, sizeof(testSec)) == 0);
        Linger readBack{};
        std::uint32_t readSize = sizeof(readBack);
        Require(getsockopt_nid_postfix(stream, SolSocket, SoLinger, &readBack, &readSize) == 0);
        Require(readSize == sizeof(readBack));
        Require(readBack.enabled == SoLinger);
        Require(readBack.seconds == sec);
    }

    const int enabledFlags[] = {1, 2, 42, -1, 0x7fffffff};
    for (const int flag : enabledFlags) {
        Linger testFlag{flag, 12};
        Require(setsockopt_nid_postfix(stream, SolSocket, SoLinger, &testFlag, sizeof(testFlag)) == 0);
        Linger readBack{};
        std::uint32_t readSize = sizeof(readBack);
        Require(getsockopt_nid_postfix(stream, SolSocket, SoLinger, &readBack, &readSize) == 0);
        Require(readBack.enabled == SoLinger);
        Require(readBack.seconds == 12);
    }

    Linger disableLinger{0, 0};
    Require(setsockopt_nid_postfix(stream, SolSocket, SoLinger, &disableLinger, sizeof(disableLinger)) == 0);
    Linger readDisabled{};
    std::uint32_t readSize = sizeof(readDisabled);
    Require(getsockopt_nid_postfix(stream, SolSocket, SoLinger, &readDisabled, &readSize) == 0);
    Require(readDisabled.enabled == 0);

    Require(FailsWith(getsockopt_nid_postfix(-1, SolSocket, SoLinger, &readDisabled, &readSize), ErrorBadDescriptor));
    Require(FailsWith(getsockopt_nid_postfix(stream, SolSocket, SoLinger, nullptr, &readSize), ErrorFault));
    Require(FailsWith(getsockopt_nid_postfix(stream, SolSocket, SoLinger, &readDisabled, nullptr), ErrorFault));
    Require(FailsWith(getsockopt_nid_postfix(stream, SolSocket, SoLinger, guard.data + guard.size, &readSize), ErrorFault));

    guard.SetFirstPageReadOnly(true);
    Require(FailsWith(getsockopt_nid_postfix(stream, SolSocket, SoLinger, guard.data, &readSize), ErrorFault));
    guard.SetFirstPageReadOnly(false);

    readSize = 4;
    Require(getsockopt_nid_postfix(stream, SolSocket, SoLinger, &readDisabled, &readSize) == 0);
    Require(readSize == 4);

    readSize = 0;
    Require(getsockopt_nid_postfix(stream, SolSocket, SoLinger, &readDisabled, &readSize) == 0);
    Require(readSize == 0);

    Require(close_nid_postfix(stream) == 0);
    Require(FailsWith(setsockopt_nid_postfix(stream, SolSocket, SoLinger, &validLinger, sizeof(validLinger)), ErrorBadDescriptor));
    Require(FailsWith(getsockopt_nid_postfix(stream, SolSocket, SoLinger, &readDisabled, &readSize), ErrorBadDescriptor));
}

void TestShutdownDescriptorAndFlagsBounds() {
    for (const int how : {ShutRd, ShutWr, ShutRdWr, -1, 3, 999}) {
        Require(FailsWith(shutdown_nid_postfix(-1, how), ErrorBadDescriptor));
        Require(FailsWith(shutdown_nid_postfix(-100, how), ErrorBadDescriptor));
        Require(FailsWith(shutdown_nid_postfix(1024, how), ErrorBadDescriptor));
        Require(FailsWith(shutdown_nid_postfix(65535, how), ErrorBadDescriptor));
        Require(FailsWith(_shutdown_nid_postfix(-1, how), ErrorBadDescriptor));
    }

    const int closedSock = socket_nid_postfix(AfInet, SockStream, 0);
    Require(closedSock >= 0);
    Require(close_nid_postfix(closedSock) == 0);
    for (const int how : {ShutRd, ShutWr, ShutRdWr, -1, 3, 999}) {
        Require(FailsWith(shutdown_nid_postfix(closedSock, how), ErrorBadDescriptor));
        Require(FailsWith(_shutdown_nid_postfix(closedSock, how), ErrorBadDescriptor));
    }

    int pipeEnds[2]{-1, -1};
    Require(pipe_nid_postfix(pipeEnds) == 0);
    for (const int how : {ShutRd, ShutWr, ShutRdWr, -1, 3, 999}) {
        Require(FailsWith(shutdown_nid_postfix(pipeEnds[0], how), ErrorNotSocket));
        Require(FailsWith(shutdown_nid_postfix(pipeEnds[1], how), ErrorNotSocket));
        Require(FailsWith(_shutdown_nid_postfix(pipeEnds[0], how), ErrorNotSocket));
        Require(FailsWith(_shutdown_nid_postfix(pipeEnds[1], how), ErrorNotSocket));
    }
    Require(close_nid_postfix(pipeEnds[0]) == 0);
    Require(close_nid_postfix(pipeEnds[1]) == 0);

    const int stream = socket_nid_postfix(AfInet, SockStream, 0);
    Require(stream >= 0);
    for (const int how : {-1000, -100, -2, -1, 3, 4, 10, 100, 256, 65535}) {
        Require(FailsWith(shutdown_nid_postfix(stream, how), ErrorInvalid));
        Require(FailsWith(_shutdown_nid_postfix(stream, how), ErrorInvalid));
    }
    for (const int how : {ShutRd, ShutWr, ShutRdWr}) {
        Require(FailsWith(shutdown_nid_postfix(stream, how), ErrorNotConnected));
        Require(FailsWith(_shutdown_nid_postfix(stream, how), ErrorNotConnected));
    }
    Require(close_nid_postfix(stream) == 0);

    const int datagram = socket_nid_postfix(AfInet, SockDgram, 0);
    Require(datagram >= 0);
    for (const int how : {-1, 3, 10}) {
        Require(FailsWith(shutdown_nid_postfix(datagram, how), ErrorInvalid));
    }
    for (const int how : {ShutRd, ShutWr, ShutRdWr}) {
        Require(FailsWith(shutdown_nid_postfix(datagram, how), ErrorNotConnected));
    }
    Require(close_nid_postfix(datagram) == 0);

    std::array<std::uint8_t, 16> address{};
    const int listener = CreateListeningTcpSocket(address);
    for (const int how : {ShutRd, ShutWr, ShutRdWr}) {
        Require(FailsWith(shutdown_nid_postfix(listener, how), ErrorNotConnected));
    }
    Require(close_nid_postfix(listener) == 0);
}

void TestShutdownReadWriteSemantics() {
    {
        TcpPair pair;
        Require(shutdown_nid_postfix(pair.client, ShutWr) == 0);
        Require(_shutdown_nid_postfix(pair.client, ShutWr) == 0);

        const char ping[] = "ping";
        Require(FailsWith(send_nid_postfix(pair.client, ping, sizeof(ping), 0), ErrorPipe));

        Require(send_nid_postfix(pair.server, ping, sizeof(ping), 0) == sizeof(ping));
        char buffer[16]{};
        Require(recv_nid_postfix(pair.client, buffer, sizeof(buffer), 0) == sizeof(ping));
        Require(std::strcmp(buffer, ping) == 0);

        Require(recv_nid_postfix(pair.server, buffer, sizeof(buffer), 0) == 0);

        Require(shutdown_nid_postfix(pair.client, ShutRd) == 0);
        Require(recv_nid_postfix(pair.client, buffer, sizeof(buffer), 0) == 0);
    }
    {
        TcpPair pair;
        Require(shutdown_nid_postfix(pair.client, ShutRd) == 0);
        char buffer[16]{};
        Require(recv_nid_postfix(pair.client, buffer, sizeof(buffer), 0) == 0);

        const char pong[] = "pong";
        Require(send_nid_postfix(pair.client, pong, sizeof(pong), 0) == sizeof(pong));
        Require(recv_nid_postfix(pair.server, buffer, sizeof(buffer), 0) == sizeof(pong));
        Require(std::strcmp(buffer, pong) == 0);

        Require(shutdown_nid_postfix(pair.client, ShutRdWr) == 0);
        Require(FailsWith(send_nid_postfix(pair.client, pong, sizeof(pong), 0), ErrorPipe));
        Require(recv_nid_postfix(pair.client, buffer, sizeof(buffer), 0) == 0);
    }
}

void TestLingerAndShutdownInteraction() {
    TcpPair pair;
    Linger lingerCfg{1, 0};
    Require(setsockopt_nid_postfix(pair.client, SolSocket, SoLinger, &lingerCfg, sizeof(lingerCfg)) == 0);
    Linger readBack{};
    std::uint32_t readSize = sizeof(readBack);
    Require(getsockopt_nid_postfix(pair.client, SolSocket, SoLinger, &readBack, &readSize) == 0);
    Require(readBack.enabled == SoLinger && readBack.seconds == 0);

    Require(shutdown_nid_postfix(pair.client, ShutWr) == 0);

    readSize = sizeof(readBack);
    Require(getsockopt_nid_postfix(pair.client, SolSocket, SoLinger, &readBack, &readSize) == 0);
    Require(readBack.enabled == SoLinger && readBack.seconds == 0);

    lingerCfg = {1, 3};
    Require(setsockopt_nid_postfix(pair.client, SolSocket, SoLinger, &lingerCfg, sizeof(lingerCfg)) == 0);
    readSize = sizeof(readBack);
    Require(getsockopt_nid_postfix(pair.client, SolSocket, SoLinger, &readBack, &readSize) == 0);
    Require(readBack.enabled == SoLinger && readBack.seconds == 3);

    lingerCfg = {0, 0};
    Require(setsockopt_nid_postfix(pair.client, SolSocket, SoLinger, &lingerCfg, sizeof(lingerCfg)) == 0);
    readSize = sizeof(readBack);
    Require(getsockopt_nid_postfix(pair.client, SolSocket, SoLinger, &readBack, &readSize) == 0);
    Require(readBack.enabled == 0);

    Require(shutdown_nid_postfix(pair.server, ShutRdWr) == 0);
    Require(shutdown_nid_postfix(pair.client, ShutRdWr) == 0);
}

void TestUnixDomainSocketPairShutdownAndLinger() {
    int pair[2]{-1, -1};
    Require(socketpair_nid_postfix(AfUnix, SockStream, 0, pair) == 0);

    Linger lingerUnix{1, 15};
    Require(setsockopt_nid_postfix(pair[0], SolSocket, SoLinger, &lingerUnix, sizeof(lingerUnix)) == 0);
    Linger readLinger{};
    std::uint32_t size = sizeof(readLinger);
    Require(getsockopt_nid_postfix(pair[0], SolSocket, SoLinger, &readLinger, &size) == 0);
    Require(readLinger.enabled == SoLinger && readLinger.seconds == 15);

    for (const int how : {-1, 3, 100}) {
        Require(FailsWith(shutdown_nid_postfix(pair[0], how), ErrorInvalid));
        Require(FailsWith(_shutdown_nid_postfix(pair[0], how), ErrorInvalid));
    }

    Require(shutdown_nid_postfix(pair[0], ShutWr) == 0);
    const char msg[] = "unix";
    Require(FailsWith(send_nid_postfix(pair[0], msg, sizeof(msg), 0), ErrorPipe));

    char buf[16]{};
    Require(recv_nid_postfix(pair[1], buf, sizeof(buf), 0) == 0);

    Require(shutdown_nid_postfix(pair[1], ShutRd) == 0);
    Require(recv_nid_postfix(pair[1], buf, sizeof(buf), 0) == 0);

    Require(shutdown_nid_postfix(pair[0], ShutRdWr) == 0);
    Require(shutdown_nid_postfix(pair[1], ShutRdWr) == 0);

    Require(close_nid_postfix(pair[0]) == 0);
    Require(close_nid_postfix(pair[1]) == 0);
    Require(FailsWith(shutdown_nid_postfix(pair[0], ShutRdWr), ErrorBadDescriptor));
    Require(FailsWith(shutdown_nid_postfix(pair[1], ShutRdWr), ErrorBadDescriptor));
}

} // namespace

int main() {
    TestLingerParameterBounds();
    TestShutdownDescriptorAndFlagsBounds();
    TestShutdownReadWriteSemantics();
    TestLingerAndShutdownInteraction();
    TestUnixDomainSocketPairShutdownAndLinger();
    return 0;
}
