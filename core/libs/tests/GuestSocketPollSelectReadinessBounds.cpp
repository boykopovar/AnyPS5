#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>

namespace {

constexpr int Inet = 2;
constexpr int Stream = 1;

constexpr int ErrorBadDescriptor = 9;
constexpr int ErrorInvalid = 22;

struct GuestTimeval {
    std::int64_t seconds;
    std::int64_t microseconds;
};

struct GuestFdSet {
    std::array<std::uint64_t, 16> words{};
    void Set(int descriptor) {
        if (descriptor >= 0 && descriptor < 1024) {
            words[descriptor / 64] |= (std::uint64_t{1} << (descriptor % 64));
        }
    }
    bool IsSet(int descriptor) const {
        if (descriptor >= 0 && descriptor < 1024) {
            return ((words[descriptor / 64] >> (descriptor % 64)) & 1u) != 0;
        }
        return false;
    }
    void Zero() {
        words.fill(0);
    }
};

void Require(bool condition, const std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "Requirement failed at %s:%u in %s (errno %d)\n", location.file_name(), location.line(), location.function_name(), *__error_nid_postfix());
        std::abort();
    }
}

void TestPrecedence() {
    const int tempSock = socket_nid_postfix(Inet, Stream, 0);
    Require(tempSock >= 0);
    Require(close_nid_postfix(tempSock) == 0);

    GuestFdSet closedFdSet{};
    closedFdSet.Set(tempSock);
    GuestTimeval zeroTimeout{0, 0};

    *__error_nid_postfix() = 0;
    Require(select_nid_postfix(tempSock + 1, &closedFdSet, nullptr, nullptr, &zeroTimeout) == -1);
    Require(*__error_nid_postfix() == ErrorBadDescriptor);

    *__error_nid_postfix() = 0;
    Require(select_nid_postfix(-1, &closedFdSet, nullptr, nullptr, &zeroTimeout) == -1);
    Require(*__error_nid_postfix() == ErrorInvalid);

    GuestTimeval negativeSec{-1, 0};
    *__error_nid_postfix() = 0;
    Require(select_nid_postfix(tempSock + 1, &closedFdSet, nullptr, nullptr, &negativeSec) == -1);
    Require(*__error_nid_postfix() == ErrorInvalid);

    GuestTimeval negativeUsec{0, -1};
    *__error_nid_postfix() = 0;
    Require(select_nid_postfix(tempSock + 1, &closedFdSet, nullptr, nullptr, &negativeUsec) == -1);
    Require(*__error_nid_postfix() == ErrorInvalid);

    GuestTimeval overLimitUsec{0, 1000000};
    *__error_nid_postfix() = 0;
    Require(select_nid_postfix(tempSock + 1, &closedFdSet, nullptr, nullptr, &overLimitUsec) == -1);
    Require(*__error_nid_postfix() == ErrorInvalid);

    *__error_nid_postfix() = 0;
    Require(poll_nid_postfix(nullptr, 1, -5) == -1);
    Require(*__error_nid_postfix() == ErrorInvalid);
}

void TestSelectTcpReadiness() {
    const int listener = socket_nid_postfix(Inet, Stream, 0);
    Require(listener >= 0);
    std::array<std::uint8_t, 16> listenAddr{16, Inet, 0, 0, 127, 0, 0, 1};
    Require(bind_nid_postfix(listener, listenAddr.data(), listenAddr.size()) == 0);
    std::uint32_t listenLen = listenAddr.size();
    Require(getsockname_nid_postfix(listener, listenAddr.data(), &listenLen) == 0 && listenLen == 16);
    Require(listen_nid_postfix(listener, 5) == 0);

    GuestTimeval zeroTimeout{0, 0};
    GuestFdSet listenSelectSet{};
    listenSelectSet.Set(listener);
    Require(select_nid_postfix(listener + 1, &listenSelectSet, nullptr, nullptr, &zeroTimeout) == 0);

    const int client = socket_nid_postfix(Inet, Stream, 0);
    Require(client >= 0);
    Require(connect_nid_postfix(client, listenAddr.data(), listenAddr.size()) == 0);

    GuestTimeval boundedTimeout{1, 0};
    listenSelectSet.Zero();
    listenSelectSet.Set(listener);
    Require(select_nid_postfix(listener + 1, &listenSelectSet, nullptr, nullptr, &boundedTimeout) == 1);
    Require(listenSelectSet.IsSet(listener));

    const int server = accept_nid_postfix(listener, nullptr, nullptr);
    Require(server >= 0);

    listenSelectSet.Zero();
    listenSelectSet.Set(listener);
    Require(select_nid_postfix(listener + 1, &listenSelectSet, nullptr, nullptr, &zeroTimeout) == 0);

    GuestFdSet serverReadSet{};
    GuestFdSet serverWriteSet{};
    serverReadSet.Set(server);
    serverWriteSet.Set(server);
    Require(select_nid_postfix(server + 1, &serverReadSet, &serverWriteSet, nullptr, &boundedTimeout) == 1);
    Require(!serverReadSet.IsSet(server) && serverWriteSet.IsSet(server));

    const char sendData[] = "DATA";
    Require(send_nid_postfix(client, sendData, 4, 0) == 4);

    serverReadSet.Zero();
    serverWriteSet.Zero();
    serverReadSet.Set(server);
    serverWriteSet.Set(server);
    Require(select_nid_postfix(server + 1, &serverReadSet, &serverWriteSet, nullptr, &boundedTimeout) == 2);
    Require(serverReadSet.IsSet(server) && serverWriteSet.IsSet(server));

    char recvBuf[8]{};
    Require(recv_nid_postfix(server, recvBuf, sizeof(recvBuf), 0) == 4);

    Require(shutdown_nid_postfix(client, 1) == 0);

    serverReadSet.Zero();
    serverReadSet.Set(server);
    Require(select_nid_postfix(server + 1, &serverReadSet, nullptr, nullptr, &boundedTimeout) == 1);
    Require(serverReadSet.IsSet(server));

    Require(recv_nid_postfix(server, recvBuf, sizeof(recvBuf), 0) == 0);

    Require(close_nid_postfix(server) == 0);
    Require(close_nid_postfix(client) == 0);
    Require(close_nid_postfix(listener) == 0);

    GuestFdSet postCloseSelectSet{};
    postCloseSelectSet.Set(server);
    *__error_nid_postfix() = 0;
    Require(select_nid_postfix(server + 1, &postCloseSelectSet, nullptr, nullptr, &zeroTimeout) == -1);
    Require(*__error_nid_postfix() == ErrorBadDescriptor);
}

} // namespace

int main() {
    TestPrecedence();
    TestSelectTcpReadiness();
    return 0;
}
