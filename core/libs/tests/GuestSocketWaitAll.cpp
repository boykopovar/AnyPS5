#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <thread>

struct Timeval {
    std::int64_t seconds;
    std::int64_t microseconds;
};

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "MSG_WAITALL check failed at line %d, errno %d\n", line, *__error_nid_postfix());
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

constexpr int SocketLevel = 0xffff;
constexpr int ReceiveTimeout = 0x1006;
constexpr int Peek = 0x2;
constexpr int WaitAll = 0x40;
constexpr int DontWait = 0x80;
constexpr int SetStatusFlags = 4;
constexpr int NonBlocking = 4;
constexpr int WouldBlock = 35;
constexpr auto MostOfTimeout = std::chrono::milliseconds(180);

using Clock = std::chrono::steady_clock;

static bool Fails(std::int64_t result, int error) {
    return result == -1 && *__error_nid_postfix() == error;
}

static void Queue(int client, const char* text) {
    Require(send_nid_postfix(client, text, 3, 0) == 3);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
}

static bool PartialPeekThrows(int descriptor, char* buffer, std::size_t length) {
    try {
        recv_nid_postfix(descriptor, buffer, length, Peek | WaitAll);
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

static bool Holds(const char* buffer, const char* text) {
    return std::memcmp(buffer, text, 3) == 0;
}

int main() {
    const int listener = socket_nid_postfix(2, 1, 0);
    std::array<std::uint8_t, 16> address{16, 2, 0, 0, 127, 0, 0, 1};
    Require(listener >= 0 && bind_nid_postfix(listener, address.data(), address.size()) == 0 && listen_nid_postfix(listener, 4) == 0);
    std::uint32_t size = address.size();
    Require(getsockname_nid_postfix(listener, address.data(), &size) == 0);
    const int client = socket_nid_postfix(2, 1, 0);
    Require(client >= 0 && connect_nid_postfix(client, address.data(), address.size()) == 0);
    const int accepted = accept_nid_postfix(listener, nullptr, nullptr);
    Require(accepted >= 0);
    const Timeval time{0, 200000};
    Require(setsockopt_nid_postfix(accepted, SocketLevel, ReceiveTimeout, &time, sizeof(time)) == 0);

    char buffer[8]{};
    Queue(client, "123");
    auto started = Clock::now();
    Require(recv_nid_postfix(accepted, buffer, sizeof(buffer), WaitAll) == 3 && Holds(buffer, "123"));
    Require(Clock::now() - started >= MostOfTimeout);
    started = Clock::now();
    Require(Fails(recv_nid_postfix(accepted, buffer, sizeof(buffer), WaitAll), WouldBlock));
    Require(Clock::now() - started >= MostOfTimeout);

    Queue(client, "456");
    started = Clock::now();
    Require(recv_nid_postfix(accepted, buffer, sizeof(buffer), DontWait | WaitAll) == 3 && Holds(buffer, "456"));
    Require(Clock::now() - started >= MostOfTimeout);

    Queue(client, "pk!");
    Require(PartialPeekThrows(accepted, buffer, sizeof(buffer)));
    Require(recv_nid_postfix(accepted, buffer, sizeof(buffer), 0) == 3 && Holds(buffer, "pk!"));

    Require(fcntl_nid_postfix(accepted, SetStatusFlags, NonBlocking) == 0);
    Queue(client, "789");
    started = Clock::now();
    Require(recv_nid_postfix(accepted, buffer, sizeof(buffer), WaitAll) == 3 && Holds(buffer, "789"));
    Require(Clock::now() - started >= MostOfTimeout);
    started = Clock::now();
    Require(Fails(recv_nid_postfix(accepted, buffer, sizeof(buffer), WaitAll), WouldBlock));
    Require(Clock::now() - started < MostOfTimeout);
    Require(Fails(recv_nid_postfix(accepted, buffer, sizeof(buffer), Peek | WaitAll), WouldBlock));

    Queue(client, "abc");
    started = Clock::now();
    Require(recvfrom_nid_postfix(accepted, buffer, sizeof(buffer), WaitAll, nullptr, nullptr) == 3 && Holds(buffer, "abc"));
    Require(Clock::now() - started >= MostOfTimeout);

    Queue(client, "xyz");
    KernelIovec vector{buffer, sizeof(buffer)};
    GuestMsghdr message{nullptr, 0, &vector, 1, nullptr, 0, 0};
    started = Clock::now();
    Require(recvmsg_nid_postfix(accepted, &message, WaitAll) == 3 && Holds(buffer, "xyz") && message.flags == WaitAll);
    Require(Clock::now() - started >= MostOfTimeout);

    for (const int descriptor : {client, accepted, listener}) Require(close_nid_postfix(descriptor) == 0);
    return 0;
}
