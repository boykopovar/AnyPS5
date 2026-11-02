#include "prx/libwebrtc/include/PacketInfo.hpp"
#include "prx/libwebrtc/libwebrtc.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "libwebrtc PacketInfo check failed at line %u\n", location.line());
    std::abort();
}

void TestDestructor() {
    unsigned char storage[64];
    std::memset(storage, 0xCD, sizeof(storage));
    auto* info = reinterpret_cast<WebRtcPacketInfo*>(storage);
    _ZN3rtc10PacketInfoD1Ev_nid_postfix(info);
    for (unsigned char byte : storage)
        Require(byte == 0xCD);
    _ZN3rtc10PacketInfoD1Ev_nid_postfix(nullptr);
}

}

int main() {
    TestDestructor();
    return 0;
}
