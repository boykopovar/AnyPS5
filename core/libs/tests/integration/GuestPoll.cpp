#include "prx/libc/include/general/VabiMacros.hpp"

#include <Testing/Test.hpp>

#include <array>
#include <chrono>
#include <cstdint>
#include <string>

struct PollDescriptor {
    int descriptor;
    short events;
    short revents;
};

extern "C" {
int APS5_VABI socket_nid_postfix(int, int, int);
int APS5_VABI bind_nid_postfix(int, const void*, std::uint32_t);
int APS5_VABI listen_nid_postfix(int, int);
int APS5_VABI accept_nid_postfix(int, void*, std::uint32_t*);
int APS5_VABI connect_nid_postfix(int, const void*, std::uint32_t);
int APS5_VABI getsockname_nid_postfix(int, void*, std::uint32_t*);
std::int64_t APS5_VABI send_nid_postfix(int, const void*, std::uint64_t, int);
std::int64_t APS5_VABI recv_nid_postfix(int, void*, std::uint64_t, int);
int APS5_VABI close_nid_postfix(int);
int APS5_VABI poll_nid_postfix(PollDescriptor*, std::uint32_t, int);
int* APS5_VABI __error_nid_postfix();
}

namespace {

using Testing::Case;
using Testing::Require;
using Testing::RequireEqual;

constexpr short In = 0x1;
constexpr short Out = 0x4;
constexpr short Hup = 0x10;
constexpr short Invalid = 0x20;
constexpr int efault = 14;
constexpr int einval = 22;
constexpr char message[] = "poll";

class Loopback {
public:
    Loopback() {
        listener = socket_nid_postfix(2, 1, 0);
        Require(listener >= 0, "create listener");
        RequireEqual(bind_nid_postfix(listener, address.data(), address.size()), 0, "bind listener");
        std::uint32_t size = address.size();
        RequireEqual(getsockname_nid_postfix(listener, address.data(), &size), 0, "listener address");
        RequireEqual(listen_nid_postfix(listener, 4), 0, "listen");
    }
    ~Loopback() {
        for (const int descriptor : {client, server, listener}) {
            if (descriptor >= 0) close_nid_postfix(descriptor);
        }
    }
    Loopback(const Loopback&) = delete;
    Loopback& operator=(const Loopback&) = delete;

    void Connect() {
        client = socket_nid_postfix(2, 1, 0);
        Require(client >= 0, "create client");
        RequireEqual(connect_nid_postfix(client, address.data(), address.size()), 0, "connect");
    }

    void Accept() {
        Connect();
        server = accept_nid_postfix(listener, nullptr, nullptr);
        Require(server >= 0, "accept");
    }

    void SendFromClient() {
        RequireEqual(send_nid_postfix(client, message, sizeof(message), 0), static_cast<std::int64_t>(sizeof(message)), "send");
    }

    int CloseClient() {
        const int result = close_nid_postfix(client);
        closedClient = client;
        client = -1;
        return result;
    }

    int CloseServer() {
        const int result = close_nid_postfix(server);
        server = -1;
        return result;
    }

    int CloseListener() {
        const int result = close_nid_postfix(listener);
        listener = -1;
        return result;
    }

    int listener = -1;
    int client = -1;
    int server = -1;
    int closedClient = -1;

private:
    std::array<unsigned char, 16> address{16, 2, 0, 0, 127, 0, 0, 1};
};

const Case idleListener{"Poll_ListenerWithoutPendingConnection_ReportsNoEvents", [] {
    Loopback sockets;
    PollDescriptor single{sockets.listener, In, -1};
    RequireEqual(poll_nid_postfix(&single, 1, 0), 0, "ready descriptors");
    RequireEqual(single.revents, short{0}, "revents");
}};

const Case idleTimeout{"Poll_ListenerWithoutPendingConnection_WaitsForTimeout", [] {
    Loopback sockets;
    PollDescriptor single{sockets.listener, In, 0};
    const auto start = std::chrono::steady_clock::now();
    RequireEqual(poll_nid_postfix(&single, 1, 50), 0, "ready descriptors");
    Require(std::chrono::steady_clock::now() - start >= std::chrono::milliseconds(40), "poll waited at least 40 ms");
}};

const Case pendingConnection{"Poll_ListenerWithPendingConnection_ReportsIn", [] {
    Loopback sockets;
    sockets.Connect();
    PollDescriptor single{sockets.listener, In, 0};
    RequireEqual(poll_nid_postfix(&single, 1, 1000), 1, "ready descriptors");
    RequireEqual(single.revents, In, "revents");
}};

const Case connectedPair{"Poll_ConnectedPairWithoutData_ReportsWritableClientAndSkipsNegativeDescriptor", [] {
    Loopback sockets;
    sockets.Accept();
    PollDescriptor pair[3]{{sockets.server, In, 0}, {-1, In, 7}, {sockets.client, Out, 0}};
    RequireEqual(poll_nid_postfix(pair, 3, 1000), 1, "ready descriptors");
    RequireEqual(pair[0].revents, short{0}, "server revents");
    RequireEqual(pair[1].revents, short{0}, "negative descriptor revents");
    RequireEqual(pair[2].revents, Out, "client revents");
}};

const Case pendingData{"Poll_DataSent_ReportsInOnReceiverOnly", [] {
    Loopback sockets;
    sockets.Accept();
    sockets.SendFromClient();
    PollDescriptor pair[3]{{sockets.server, In, 0}, {-1, In, 7}, {sockets.client, In, 0}};
    RequireEqual(poll_nid_postfix(pair, 3, 1000), 1, "ready descriptors");
    RequireEqual(pair[0].revents, In, "server revents");
    RequireEqual(pair[2].revents, short{0}, "client revents");
}};

const Case peerClosed{"Poll_PeerClosed_ReportsInOrHupAndRecvReturnsEndOfStream", [] {
    Loopback sockets;
    sockets.Accept();
    sockets.SendFromClient();
    char buffer[8]{};
    RequireEqual(recv_nid_postfix(sockets.server, buffer, sizeof(buffer), 0), static_cast<std::int64_t>(sizeof(message)), "recv data");
    RequireEqual(sockets.CloseClient(), 0, "close client");
    PollDescriptor server{sockets.server, In, 0};
    RequireEqual(poll_nid_postfix(&server, 1, 1000), 1, "ready descriptors");
    Require((server.revents & (In | Hup)) != 0, "server revents contain In or Hup, got " + std::to_string(server.revents));
    RequireEqual(recv_nid_postfix(sockets.server, buffer, sizeof(buffer), 0), std::int64_t{0}, "recv after close");
}};

const Case closedDescriptor{"Poll_ClosedDescriptor_ReportsInvalid", [] {
    Loopback sockets;
    sockets.Accept();
    RequireEqual(sockets.CloseClient(), 0, "close client");
    PollDescriptor closed[2]{{sockets.closedClient, In, 0}, {sockets.listener, In, 0}};
    RequireEqual(poll_nid_postfix(closed, 2, -1), 1, "ready descriptors");
    RequireEqual(closed[0].revents, Invalid, "closed descriptor revents");
    RequireEqual(closed[1].revents, short{0}, "listener revents");
    RequireEqual(sockets.CloseServer(), 0, "close server");
    RequireEqual(sockets.CloseListener(), 0, "close listener");
}};

const Case emptySet{"Poll_NullArrayWithZeroCount_ReturnsZero", [] {
    RequireEqual(poll_nid_postfix(nullptr, 0, 10), 0, "ready descriptors");
}};

const Case nullArray{"Poll_NullArrayWithEntries_FailsWithEfault", [] {
    RequireEqual(poll_nid_postfix(nullptr, 1, 0), -1, "result");
    RequireEqual(*__error_nid_postfix(), efault, "errno");
}};

const Case invalidTimeout{"Poll_TimeoutBelowMinusOne_FailsWithEinval", [] {
    Loopback sockets;
    PollDescriptor single{sockets.listener, In, 0};
    RequireEqual(poll_nid_postfix(&single, 1, -2), -1, "result");
    RequireEqual(*__error_nid_postfix(), einval, "errno");
}};

} // namespace
