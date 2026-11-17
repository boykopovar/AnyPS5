#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "send before connect check failed at line %d, errno %d\n", line, *__error_nid_postfix());
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

constexpr int Inet = 2;
constexpr int Stream = 1;
constexpr int NoSignalFlag = 0x20000;
constexpr int WaitAll = 0x40;
constexpr int NotConnected = 57;

static bool Fails(std::int64_t result, int error) {
    return result == -1 && *__error_nid_postfix() == error;
}

int main() {
    const char text[] = "early";
    const int fresh = socket_nid_postfix(Inet, Stream, 0);
    Require(fresh >= 0);
    Require(Fails(send_nid_postfix(fresh, text, sizeof(text), 0), NotConnected));
    Require(Fails(send_nid_postfix(fresh, text, 0, 0), NotConnected));
    Require(Fails(send_nid_postfix(fresh, text, sizeof(text), NoSignalFlag), NotConnected));
    Require(Fails(sendto_nid_postfix(fresh, text, sizeof(text), 0, nullptr, 0), NotConnected));
    Require(Fails(write_nid_postfix(fresh, text, sizeof(text)), NotConnected));

    const int listener = socket_nid_postfix(Inet, Stream, 0);
    Require(listener >= 0);
    std::array<std::uint8_t, 16> address{16, Inet, 0, 0, 127, 0, 0, 1};
    Require(bind_nid_postfix(listener, address.data(), address.size()) == 0 && listen_nid_postfix(listener, 4) == 0);
    Require(Fails(send_nid_postfix(listener, text, sizeof(text), 0), NotConnected));
    Require(Fails(write_nid_postfix(listener, text, sizeof(text)), NotConnected));

    std::uint32_t size = address.size();
    Require(getsockname_nid_postfix(listener, address.data(), &size) == 0);
    Require(connect_nid_postfix(fresh, address.data(), address.size()) == 0);
    const int accepted = accept_nid_postfix(listener, nullptr, nullptr);
    Require(accepted >= 0);
    Require(send_nid_postfix(fresh, text, sizeof(text), 0) == sizeof(text));
    char received[sizeof(text)]{};
    Require(recv_nid_postfix(accepted, received, sizeof(received), WaitAll) == sizeof(text) && std::strcmp(text, received) == 0);
    Require(write_nid_postfix(accepted, text, sizeof(text)) == sizeof(text));
    Require(recv_nid_postfix(fresh, received, sizeof(received), WaitAll) == sizeof(text) && std::strcmp(text, received) == 0);
    for (const int descriptor : {fresh, accepted, listener}) Require(close_nid_postfix(descriptor) == 0);
    return 0;
}
