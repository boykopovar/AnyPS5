#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <thread>

extern "C" {
int APS5_VABI socket_nid_postfix(int, int, int);
int APS5_VABI bind_nid_postfix(int, const void*, std::uint32_t);
int APS5_VABI connect_nid_postfix(int, const void*, std::uint32_t);
int APS5_VABI listen_nid_postfix(int, int);
int APS5_VABI accept_nid_postfix(int, void*, std::uint32_t*);
int APS5_VABI getsockname_nid_postfix(int, void*, std::uint32_t*);
int APS5_VABI close_nid_postfix(int);
int* APS5_VABI __error_nid_postfix();
int APS5_VABI sceNetInit_nid_postfix();
int APS5_VABI sceNetSocket(const char*, int, int, int);
int APS5_VABI sceNetConnect(int, const void*, std::uint32_t);
int APS5_VABI sceNetSocketClose(int);
int* APS5_VABI sceNetErrnoLoc();
}

namespace {

constexpr int GuestInet = 2;
constexpr int GuestStream = 1;
constexpr int GuestConnectionRefused = 61;
constexpr auto RefusalLimit = std::chrono::milliseconds(1000);

void Require(bool value) { if (!value) std::abort(); }

std::array<std::uint8_t, 16> ClosedLoopbackPort() {
    std::array<std::uint8_t, 16> address{16, 2, 0, 0, 127, 0, 0, 1};
    const int descriptor = socket_nid_postfix(GuestInet, GuestStream, 0);
    Require(descriptor >= 0);
    Require(bind_nid_postfix(descriptor, address.data(), address.size()) == 0);
    std::uint32_t length = address.size();
    Require(getsockname_nid_postfix(descriptor, address.data(), &length) == 0);
    Require(close_nid_postfix(descriptor) == 0);
    Require(address[2] != 0 || address[3] != 0);
    return address;
}

template<typename TConnect>
bool ConnectsOnceBacklogDrains(TConnect connect) {
    std::array<std::uint8_t, 16> address{16, 2, 0, 0, 127, 0, 0, 1};
    const int listener = socket_nid_postfix(GuestInet, GuestStream, 0);
    Require(listener >= 0);
    Require(bind_nid_postfix(listener, address.data(), address.size()) == 0);
    std::uint32_t length = address.size();
    Require(getsockname_nid_postfix(listener, address.data(), &length) == 0);
    Require(listen_nid_postfix(listener, 1) == 0);
    const int queued = socket_nid_postfix(GuestInet, GuestStream, 0);
    Require(queued >= 0 && connect_nid_postfix(queued, address.data(), address.size()) == 0);
    std::thread acceptor([listener] {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        const int accepted = accept_nid_postfix(listener, nullptr, nullptr);
        Require(accepted >= 0 && close_nid_postfix(accepted) == 0);
    });
    const bool connected = connect(address);
    acceptor.join();
    Require(close_nid_postfix(queued) == 0 && close_nid_postfix(listener) == 0);
    return connected;
}

template<typename TConnect>
bool RefusedAtOnce(TConnect connect) {
    const auto start = std::chrono::steady_clock::now();
    const bool refused = connect();
    return refused && std::chrono::steady_clock::now() - start < RefusalLimit;
}

}

int main() {
    Require(sceNetInit_nid_postfix() == 0);
    const auto address = ClosedLoopbackPort();
    Require(RefusedAtOnce([&] {
        const int descriptor = socket_nid_postfix(GuestInet, GuestStream, 0);
        Require(descriptor >= 0);
        const bool refused = connect_nid_postfix(descriptor, address.data(), address.size()) == -1 && *__error_nid_postfix() == GuestConnectionRefused;
        Require(close_nid_postfix(descriptor) == 0);
        return refused;
    }));
    Require(RefusedAtOnce([&] {
        const int descriptor = sceNetSocket("refused", GuestInet, GuestStream, 0);
        Require(descriptor >= 0);
        const bool refused = sceNetConnect(descriptor, address.data(), address.size()) == static_cast<int>(0x80410100u | GuestConnectionRefused)
            && *sceNetErrnoLoc() == GuestConnectionRefused;
        Require(sceNetSocketClose(descriptor) == 0);
        return refused;
    }));
    Require(ConnectsOnceBacklogDrains([](const std::array<std::uint8_t, 16>& listening) {
        const int descriptor = socket_nid_postfix(GuestInet, GuestStream, 0);
        Require(descriptor >= 0);
        const bool connected = connect_nid_postfix(descriptor, listening.data(), listening.size()) == 0;
        Require(close_nid_postfix(descriptor) == 0);
        return connected;
    }));
    Require(ConnectsOnceBacklogDrains([](const std::array<std::uint8_t, 16>& listening) {
        const int descriptor = sceNetSocket("backlog", GuestInet, GuestStream, 0);
        Require(descriptor >= 0);
        const bool connected = sceNetConnect(descriptor, listening.data(), listening.size()) == 0;
        Require(sceNetSocketClose(descriptor) == 0);
        return connected;
    }));
}
