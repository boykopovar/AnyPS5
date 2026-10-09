#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
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
static void Require(bool value) { if (!value) std::abort(); }
constexpr short In = 0x1;
constexpr short Out = 0x4;
constexpr short Hup = 0x10;
constexpr short Invalid = 0x20;
int main() {
    const int listener = socket_nid_postfix(2, 1, 0);
    Require(listener >= 0);
    std::array<unsigned char, 16> address{16, 2, 0, 0, 127, 0, 0, 1};
    Require(bind_nid_postfix(listener, address.data(), address.size()) == 0);
    std::uint32_t size = address.size();
    Require(getsockname_nid_postfix(listener, address.data(), &size) == 0);
    Require(listen_nid_postfix(listener, 4) == 0);
    PollDescriptor single{listener, In, -1};
    Require(poll_nid_postfix(&single, 1, 0) == 0 && single.revents == 0);
    const auto start = std::chrono::steady_clock::now();
    Require(poll_nid_postfix(&single, 1, 50) == 0);
    Require(std::chrono::steady_clock::now() - start >= std::chrono::milliseconds(40));
    const int client = socket_nid_postfix(2, 1, 0);
    Require(client >= 0 && connect_nid_postfix(client, address.data(), address.size()) == 0);
    Require(poll_nid_postfix(&single, 1, 1000) == 1 && single.revents == In);
    const int server = accept_nid_postfix(listener, nullptr, nullptr);
    Require(server >= 0);
    PollDescriptor pair[3]{{server, In, 0}, {-1, In, 7}, {client, Out, 0}};
    Require(poll_nid_postfix(pair, 3, 1000) == 1);
    Require(pair[0].revents == 0 && pair[1].revents == 0 && pair[2].revents == Out);
    const char message[] = "poll";
    Require(send_nid_postfix(client, message, sizeof(message), 0) == sizeof(message));
    pair[2].events = In;
    Require(poll_nid_postfix(pair, 3, 1000) == 1 && pair[0].revents == In && pair[2].revents == 0);
    char buffer[8]{};
    Require(recv_nid_postfix(server, buffer, sizeof(buffer), 0) == sizeof(message));
    Require(close_nid_postfix(client) == 0);
    Require(poll_nid_postfix(pair, 1, 1000) == 1 && (pair[0].revents & (In | Hup)) != 0);
    Require(recv_nid_postfix(server, buffer, sizeof(buffer), 0) == 0);
    PollDescriptor closed[2]{{client, In, 0}, {listener, In, 0}};
    Require(poll_nid_postfix(closed, 2, -1) == 1 && closed[0].revents == Invalid && closed[1].revents == 0);
    Require(poll_nid_postfix(nullptr, 0, 10) == 0);
    Require(poll_nid_postfix(nullptr, 1, 0) == -1 && *__error_nid_postfix() == 14);
    Require(poll_nid_postfix(&single, 1, -2) == -1 && *__error_nid_postfix() == 22);
    Require(close_nid_postfix(server) == 0);
    Require(close_nid_postfix(listener) == 0);
}
