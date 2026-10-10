#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>

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

static void Require(bool condition) {
    if (!condition) std::abort();
}

int main() {
    const int listener = socket_nid_postfix(2, 1, 6);
    Require(listener >= 0);
    std::array<std::uint8_t, 16> address{16, 2, 0, 0, 127, 0, 0, 1};
    Require(bind_nid_postfix(listener, address.data(), address.size()) == 0);
    Require(bind_nid_postfix(listener, address.data(), address.size()) == -1 && *__error_nid_postfix() == 22);
    Require(listen_nid_postfix(listener, 4) == 0);
    std::uint32_t address_size = address.size();
    Require(getsockname_nid_postfix(listener, address.data(), &address_size) == 0);
    Require(address_size == 16 && (address[2] != 0 || address[3] != 0));

    const int client = socket_nid_postfix(2, 1, 0);
    Require(client >= 0);
    Require(accept_nid_postfix(client, nullptr, nullptr) == -1 && *__error_nid_postfix() == 22);
    Require(connect_nid_postfix(client, address.data(), address.size()) == 0);
    std::array<std::uint8_t, 16> peer{};
    address_size = peer.size();
    const int accepted = accept_nid_postfix(listener, peer.data(), &address_size);
    Require(accepted >= 0 && address_size == 16 && peer[1] == 2);

    std::array<std::uint8_t, 16> connected_peer{};
    address_size = connected_peer.size();
    Require(getpeername_nid_postfix(accepted, connected_peer.data(), &address_size) == 0);
    Require(address_size == 16 && connected_peer[1] == 2);

    const char request[] = "guest TCP loopback";
    char received[sizeof(request)]{};
    Require(send_nid_postfix(client, request, sizeof(request), 0x1) == -1 && *__error_nid_postfix() == 45);
    Require(send_nid_postfix(client, request, sizeof(request), 0x20000) == sizeof(request));
    Require(recv_nid_postfix(accepted, received, sizeof(received), 0) == sizeof(received));
    Require(std::strcmp(request, received) == 0);
    Require(shutdown_nid_postfix(client, 1) == 0);
    Require(send_nid_postfix(client, request, sizeof(request), 0x20000) == -1 && *__error_nid_postfix() == 32);

    Require(close_nid_postfix(accepted) == 0);
#ifndef _WIN32
    std::int64_t sent = 0;
    for (int i = 0; i < 100 && sent >= 0; ++i) sent = send_nid_postfix(client, request, sizeof(request), 0x20000);
    Require(sent == -1 && *__error_nid_postfix() == 32);
#endif
    Require(close_nid_postfix(client) == 0);
    Require(close_nid_postfix(listener) == 0);
    Require(close_nid_postfix(listener) == -1 && *__error_nid_postfix() == 9);
}