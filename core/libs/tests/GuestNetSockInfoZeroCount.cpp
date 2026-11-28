#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include "prx/libSceNet/libSceNet.hpp"

static void Require(bool condition, std::source_location where = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "check failed at line %u, sceNet errno %d\n", where.line(), *sceNetErrnoLoc());
        std::abort();
    }
}

static bool Failed(int result, int error) {
    return result == static_cast<int>(0x80410100u | static_cast<unsigned>(error)) && *sceNetErrnoLoc() == error;
}

static bool Untouched(const std::array<std::uint8_t, 0x140>& buffer) {
    for (const auto byte : buffer) {
        if (byte != 0xcc) return false;
    }
    return true;
}

int main() {
    Require(sceNetInit_nid_postfix() == 0);
    const int datagram = sceNetSocket("sockinfo-zero-count", 2, 2, 0);
    const int stream = sceNetSocket("sockinfo-zero-count-stream", 2, 1, 0);
    Require(datagram >= 0 && stream >= 0);
    std::array<std::uint8_t, 0x140> buffer;
    buffer.fill(0xcc);
    for (const int socket : {datagram, stream}) {
        Require(sceNetGetSockInfo(socket, buffer.data(), 0, 0) == 0);
        Require(Untouched(buffer));
    }
    Require(sceNetGetSockInfo(datagram, buffer.data(), 1, 0) == 1);
    Require(sceNetSocketClose(stream) == 0);
    buffer.fill(0xcc);
    Require(Failed(sceNetGetSockInfo(stream, buffer.data(), 0, 0), 9));
    Require(Untouched(buffer));
    Require(Failed(sceNetGetSockInfo(datagram, buffer.data(), 0, 1), 22));
    Require(Failed(sceNetGetSockInfo(-1, buffer.data(), 0, 0), 22));
    Require(Untouched(buffer));
    Require(sceNetSocketClose(datagram) == 0);
}
