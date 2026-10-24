#include "prx/libkernel/include/Api.hpp"
#include "prx/libSceNet/include/Net.hpp"
#include "SceTypes.hpp"
#include <algorithm>
#include <array>
#include <barrier>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <future>
#include <string_view>
#include <utility>
#include <vector>

namespace {

constexpr int DontWait = 0x80;

void Require(bool value, const char* message) {
    if (value) return;
    std::fprintf(stderr, "net dontwait: %s\n", message);
    std::exit(1);
}

void SetOption(int descriptor, int option, int value) {
    Require(sceNetSetsockopt(descriptor, 0xffff, option, &value, sizeof(value)) == 0, "set socket option");
}

struct Connection {
    int listener = sceNetSocket("dontwait-listener", 2, 1, 0);
    int sender = sceNetSocket("dontwait-sender", 2, 1, 0);
    int receiver = -1;

    Connection() {
        Require(listener >= 0 && sender >= 0, "create TCP sockets");
        SetOption(listener, 0x1002, 4096);
        SetOption(sender, 0x1001, 4096);
        std::array<std::uint8_t, 16> address{16, 2, 0, 0, 127, 0, 0, 1};
        Require(sceNetBind_nid_postfix(listener, address.data(), address.size()) == 0, "bind listener");
        std::uint32_t size = address.size();
        Require(sceNetGetsockname(listener, address.data(), &size) == 0, "get listener address");
        Require(sceNetListen(listener, 1) == 0, "listen");
        Require(sceNetConnect(sender, address.data(), size) == 0, "blocking connect");
        receiver = sceNetAccept(listener, nullptr, nullptr);
        Require(receiver >= 0, "blocking accept");
        SetOption(receiver, 0x1002, 4096);
        SetOption(receiver, 0x1106, 2000000);
    }

    ~Connection() {
        for (const int descriptor : {receiver, sender, listener})
            Require(sceNetSocketClose(descriptor) == 0, "close socket");
    }

    void Abort() {
        sceNetSocketAbort(sender, 0);
        sceNetSocketAbort(receiver, 0);
    }
};

std::int64_t Send(std::string_view api, int descriptor, const std::vector<char>& bytes) {
    if (api == "send" || api == "duplex") return sceNetSend(descriptor, bytes.data(), bytes.size(), DontWait);
    if (api == "sendto") return sceNetSendto(descriptor, bytes.data(), bytes.size(), DontWait, nullptr, 0);
    std::array<NetIovec, 2> vectors{{{const_cast<char*>(bytes.data()), bytes.size() / 2},
        {const_cast<char*>(bytes.data()) + bytes.size() / 2, bytes.size() - bytes.size() / 2}}};
    const NetMsghdr message{nullptr, 0, vectors.data(), static_cast<int>(vectors.size()), nullptr, 0, 0};
    return sceNetSendmsg(descriptor, &message, DontWait);
}

void CheckSend(std::string_view api) {
    Connection connection;
    std::vector<char> bytes(16 * 1024 * 1024, 'x');
    std::future<std::int64_t> reading;
    char reply = 0;
    if (api == "duplex") {
        reading = std::async(std::launch::async, [&] { return sceNetRecv(connection.sender, &reply, 1, 0); });
        Require(reading.wait_for(std::chrono::milliseconds(50)) == std::future_status::timeout,
            "ordinary receive must wait while a DONTWAIT send runs on the same socket");
    }
    auto sending = std::async(std::launch::async, [&] { return Send(api, connection.sender, bytes); });
    const bool completed = sending.wait_for(std::chrono::milliseconds(500)) == std::future_status::ready;
    if (!completed) connection.Abort();
    const auto sent = sending.get();
    Require(completed, "MSG_DONTWAIT send blocked behind a slow peer or an ordinary receive");
    Require(sent > 0 && sent <= static_cast<std::int64_t>(bytes.size()), "large send returned an invalid byte count");
    int nonblocking = -1;
    std::uint32_t size = sizeof(nonblocking);
    Require(sceNetGetsockopt(connection.sender, 0xffff, 0x1200, &nonblocking, &size) == 0 && nonblocking == 0,
        "DONTWAIT changed the socket option");
    Require(fcntl_nid_postfix(connection.sender, 3) == 2, "DONTWAIT changed descriptor flags");
    if (reading.valid()) {
        Require(reading.wait_for(std::chrono::milliseconds(0)) == std::future_status::timeout,
            "DONTWAIT interrupted an ordinary receive");
        const char value = 'r';
        Require(sceNetSend(connection.receiver, &value, 1, 0) == 1, "send reply");
        if (reading.wait_for(std::chrono::seconds(2)) != std::future_status::ready) connection.Abort();
        Require(reading.get() == 1 && reply == value, "ordinary receive did not complete with its reply");
    }
    std::int64_t received = 0;
    SetOption(connection.receiver, 0x1002, 1024 * 1024);
    std::array<char, 65536> buffer{};
    while (received < sent) {
        const auto count = sceNetRecv(connection.receiver, buffer.data(),
            std::min<std::size_t>(buffer.size(), static_cast<std::size_t>(sent - received)), 0);
        Require(count > 0, "reported send bytes were not delivered");
        for (std::int64_t index = 0; index < count; ++index) Require(buffer[index] == 'x', "payload changed");
        received += count;
    }
    Require(sceNetRecv(connection.receiver, buffer.data(), buffer.size(), DontWait) < 0 && *sceNetErrnoLoc() == 35,
        "send left unreported bytes or receive did not return EAGAIN");
    std::printf("%.*s: sent=%lld, delivered=%lld, blocking flags preserved\n",
        static_cast<int>(api.size()), api.data(), static_cast<long long>(sent), static_cast<long long>(received));
}

std::pair<std::int64_t, int> Receive(std::string_view api, int descriptor) {
    char byte = 0;
    std::int64_t result;
    if (api == "recv") result = sceNetRecv(descriptor, &byte, 1, DontWait);
    else if (api == "recvfrom") result = sceNetRecvfrom(descriptor, &byte, 1, DontWait, nullptr, nullptr);
    else {
        NetIovec vector{&byte, 1};
        NetMsghdr message{nullptr, 0, &vector, 1, nullptr, 0, 0};
        result = sceNetRecvmsg(descriptor, &message, DontWait);
    }
    if (result == 1) Require(byte == 'x', "received payload changed");
    return {result, *sceNetErrnoLoc()};
}

void CheckReceive(std::string_view api) {
    Connection connection;
    SetOption(connection.receiver, 0x1106, 0);
    constexpr int readers = 8;
    for (int iteration = 0; iteration < 32; ++iteration) {
        const char byte = 'x';
        Require(sceNetSend(connection.sender, &byte, 1, 0) == 1, "send race payload");
        std::barrier start(readers + 1);
        std::vector<std::future<std::pair<std::int64_t, int>>> receiving;
        for (int index = 0; index < readers; ++index)
            receiving.push_back(std::async(std::launch::async, [&] {
                start.arrive_and_wait();
                return Receive(api, connection.receiver);
            }));
        start.arrive_and_wait();
        int successes = 0;
        for (auto& future : receiving) {
            const bool completed = future.wait_for(std::chrono::milliseconds(500)) == std::future_status::ready;
            if (!completed) connection.Abort();
            const auto [result, error] = future.get();
            Require(completed, "MSG_DONTWAIT receive blocked after another reader consumed the byte");
            if (result == 1) ++successes;
            else Require(result < 0 && error == 35, "empty receive must return EAGAIN");
        }
        Require(successes == 1, "one byte must be delivered to exactly one reader");
    }
    std::printf("%.*s: 32 races, one successful reader and seven EAGAIN results each\n",
        static_cast<int>(api.size()), api.data());
}

void CheckBlocking() {
    const int listener = sceNetSocket("blocking-listener", 2, 1, 0);
    const int sender = sceNetSocket("blocking-sender", 2, 1, 0);
    Require(listener >= 0 && sender >= 0, "create blocking sockets");
    std::array<std::uint8_t, 16> address{16, 2, 0, 0, 127, 0, 0, 1};
    Require(sceNetBind_nid_postfix(listener, address.data(), address.size()) == 0, "bind blocking listener");
    std::uint32_t size = address.size();
    Require(sceNetGetsockname(listener, address.data(), &size) == 0, "get blocking listener address");
    Require(sceNetListen(listener, 1) == 0, "listen before accept");
    auto accepting = std::async(std::launch::async, [&] { return sceNetAccept(listener, nullptr, nullptr); });
    Require(accepting.wait_for(std::chrono::milliseconds(50)) == std::future_status::timeout, "accept did not wait for a connection");
    Require(sceNetConnect(sender, address.data(), size) == 0, "connect to waiting accept");
    if (accepting.wait_for(std::chrono::seconds(2)) != std::future_status::ready) sceNetSocketAbort(listener, 0);
    const int receiver = accepting.get();
    Require(receiver >= 0, "waiting accept failed");
    SetOption(receiver, 0x1200, 1);
    char byte = 0;
    Require(sceNetRecv(receiver, &byte, 1, 0) < 0 && *sceNetErrnoLoc() == 35, "socket nonblocking option did not affect receive");
    SetOption(receiver, 0x1200, 0);
    auto receiving = std::async(std::launch::async, [&] { return sceNetRecv(receiver, &byte, 1, 0); });
    Require(receiving.wait_for(std::chrono::milliseconds(50)) == std::future_status::timeout, "blocking receive did not wait");
    Require(sceNetSocketClose(receiver) == 0, "close a socket with a waiting receive");
    Require(receiving.wait_for(std::chrono::seconds(2)) == std::future_status::ready, "close did not wake the receive");
    const auto closedResult = receiving.get();
    std::fprintf(stderr, "closed receive result=%lld\n", static_cast<long long>(closedResult));
    Require(closedResult <= 0, "closed receive returned data that was never sent");
    Require(sceNetSocketClose(sender) == 0 && sceNetSocketClose(listener) == 0, "close blocking fixture");
    std::puts("blocking: accept waited, socket mode toggled, close woke the pending receive");
}

}

int main(int argc, char** argv) {
    Require(argc == 2, "one API argument required");
    const std::string_view api(argv[1]);
    if (api == "blocking") {
        CheckBlocking();
        return 0;
    }
    if (api == "send" || api == "sendto" || api == "sendmsg" || api == "duplex") CheckSend(api);
    else {
        Require(api == "recv" || api == "recvfrom" || api == "recvmsg", "unknown API");
        CheckReceive(api);
    }
}
