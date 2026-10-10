#include "prx/libc/include/general/VabiMacros.hpp"

#include <Testing/Test.hpp>

#include <array>
#include <cstdint>
#include <string>
#include <string_view>

extern "C" {
int APS5_VABI socket_nid_postfix(int, int, int);
int APS5_VABI bind_nid_postfix(int, const void*, std::uint32_t);
int APS5_VABI listen_nid_postfix(int, int);
int APS5_VABI getsockname_nid_postfix(int, void*, std::uint32_t*);
int APS5_VABI connect_nid_postfix(int, const void*, std::uint32_t);
int APS5_VABI accept_nid_postfix(int, void*, std::uint32_t*);
std::int64_t APS5_VABI send_nid_postfix(int, const void*, std::uint64_t, int);
std::int64_t APS5_VABI recv_nid_postfix(int, void*, std::uint64_t, int);
int APS5_VABI getpeername_nid_postfix(int, void*, std::uint32_t*);
int APS5_VABI shutdown_nid_postfix(int, int);
int APS5_VABI close_nid_postfix(int);
int* APS5_VABI __error_nid_postfix();
}

namespace {

using Testing::Case;
using Testing::Require;
using Testing::RequireEqual;

using Address = std::array<std::uint8_t, 16>;

constexpr int ebadf = 9;
constexpr int einval = 22;
constexpr int epipe = 32;
constexpr int eopnotsupp = 45;
constexpr int msgOob = 0x1;
constexpr int msgNoSignal = 0x20000;
constexpr int shutWrite = 1;
constexpr char request[] = "guest TCP loopback";
constexpr auto requestSize = static_cast<std::int64_t>(sizeof(request));

void RequireFailure(std::int64_t result, int expectedError, const std::string& message) {
    RequireEqual(result, std::int64_t{-1}, message + " result");
    RequireEqual(*__error_nid_postfix(), expectedError, message + " errno");
}

class Loopback {
public:
    Loopback() {
        listener = socket_nid_postfix(2, 1, 6);
        Require(listener >= 0, "create listener");
        RequireEqual(bind_nid_postfix(listener, address.data(), address.size()), 0, "bind");
        RequireEqual(listen_nid_postfix(listener, 4), 0, "listen");
        std::uint32_t size = address.size();
        RequireEqual(getsockname_nid_postfix(listener, address.data(), &size), 0, "getsockname");
        addressSize = size;
    }
    ~Loopback() {
        for (const int descriptor : {accepted, client, listener}) {
            if (descriptor >= 0) close_nid_postfix(descriptor);
        }
    }
    Loopback(const Loopback&) = delete;
    Loopback& operator=(const Loopback&) = delete;

    void CreateClient() {
        client = socket_nid_postfix(2, 1, 0);
        Require(client >= 0, "create client");
    }

    void Connect() {
        CreateClient();
        RequireEqual(connect_nid_postfix(client, address.data(), address.size()), 0, "connect");
    }

    void Accept() {
        Connect();
        std::uint32_t size = peer.size();
        accepted = accept_nid_postfix(listener, peer.data(), &size);
        Require(accepted >= 0, "accept");
        peerSize = size;
    }

    int Close(int& descriptor) {
        const int result = close_nid_postfix(descriptor);
        descriptor = -1;
        return result;
    }

    int listener = -1;
    int client = -1;
    int accepted = -1;
    Address address{16, 2, 0, 0, 127, 0, 0, 1};
    std::uint32_t addressSize = 0;
    Address peer{};
    std::uint32_t peerSize = 0;
};

const Case rebind{"Bind_AlreadyBoundSocket_FailsWithEinval", [] {
    Loopback sockets;
    RequireFailure(bind_nid_postfix(sockets.listener, sockets.address.data(), sockets.address.size()), einval, "second bind");
}};

const Case boundAddress{"Getsockname_BoundListener_ReportsAssignedPort", [] {
    Loopback sockets;
    RequireEqual(sockets.addressSize, std::uint32_t{16}, "address size");
    Require(sockets.address[2] != 0 || sockets.address[3] != 0, "port assigned");
}};

const Case acceptUnlistening{"Accept_SocketNotListening_FailsWithEinval", [] {
    Loopback sockets;
    sockets.CreateClient();
    RequireFailure(accept_nid_postfix(sockets.client, nullptr, nullptr), einval, "accept on client");
}};

const Case acceptPeer{"Accept_ConnectedClient_ReportsInetPeerAddress", [] {
    Loopback sockets;
    sockets.Accept();
    RequireEqual(sockets.peerSize, std::uint32_t{16}, "peer size");
    RequireEqual(sockets.peer[1], std::uint8_t{2}, "peer family");
}};

const Case peerName{"Getpeername_AcceptedSocket_ReportsInetPeerAddress", [] {
    Loopback sockets;
    sockets.Accept();
    Address connectedPeer{};
    std::uint32_t size = connectedPeer.size();
    RequireEqual(getpeername_nid_postfix(sockets.accepted, connectedPeer.data(), &size), 0, "getpeername");
    RequireEqual(size, std::uint32_t{16}, "peer size");
    RequireEqual(connectedPeer[1], std::uint8_t{2}, "peer family");
}};

const Case outOfBand{"Send_OutOfBandFlag_FailsWithEopnotsupp", [] {
    Loopback sockets;
    sockets.Accept();
    RequireFailure(send_nid_postfix(sockets.client, request, sizeof(request), msgOob), eopnotsupp, "MSG_OOB");
}};

const Case transfer{"SendRecv_NoSignalFlag_TransfersRequest", [] {
    Loopback sockets;
    sockets.Accept();
    char received[sizeof(request)]{};
    RequireEqual(send_nid_postfix(sockets.client, request, sizeof(request), msgNoSignal), requestSize, "send");
    RequireEqual(recv_nid_postfix(sockets.accepted, received, sizeof(received), 0), requestSize, "recv");
    RequireEqual(std::string_view(received), std::string_view(request), "received data");
}};

const Case afterShutdown{"Send_AfterShutdownWrite_FailsWithEpipe", [] {
    Loopback sockets;
    sockets.Accept();
    RequireEqual(shutdown_nid_postfix(sockets.client, shutWrite), 0, "shutdown");
    RequireFailure(send_nid_postfix(sockets.client, request, sizeof(request), msgNoSignal), epipe, "send after shutdown");
}};

#ifndef _WIN32
const Case afterPeerClose{"Send_AfterPeerClosed_EventuallyFailsWithEpipe", [] {
    Loopback sockets;
    sockets.Accept();
    RequireEqual(shutdown_nid_postfix(sockets.client, shutWrite), 0, "shutdown");
    RequireEqual(sockets.Close(sockets.accepted), 0, "close accepted");
    std::int64_t sent = 0;
    for (int i = 0; i < 100 && sent >= 0; ++i) sent = send_nid_postfix(sockets.client, request, sizeof(request), msgNoSignal);
    RequireFailure(sent, epipe, "send after peer closed");
}};
#endif

const Case closeTwice{"Close_AlreadyClosedDescriptor_FailsWithEbadf", [] {
    Loopback sockets;
    sockets.Accept();
    RequireEqual(sockets.Close(sockets.accepted), 0, "close accepted");
    RequireEqual(sockets.Close(sockets.client), 0, "close client");
    const int listener = sockets.listener;
    RequireEqual(sockets.Close(sockets.listener), 0, "close listener");
    RequireFailure(close_nid_postfix(listener), ebadf, "second close");
}};

} // namespace
