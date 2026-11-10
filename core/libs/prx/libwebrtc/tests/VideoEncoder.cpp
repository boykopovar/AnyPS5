#include "prx/libwebrtc/libwebrtc.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "libwebrtc VideoEncoder check failed at line %u\n", location.line());
    std::abort();
}

}

int main() {
    unsigned char encoder[64];
    unsigned char fecControllerOverride[16];
    std::memset(encoder, 0xA5, sizeof(encoder));
    std::memset(fecControllerOverride, 0x5A, sizeof(fecControllerOverride));
    _ZN6webrtc12VideoEncoder24SetFecControllerOverrideEPNS_21FecControllerOverrideE_nid_postfix(encoder, fecControllerOverride);
    _ZN6webrtc12VideoEncoder24SetFecControllerOverrideEPNS_21FecControllerOverrideE_nid_postfix(encoder, nullptr);
    for (unsigned char byte : encoder)
        Require(byte == 0xA5);
    for (unsigned char byte : fecControllerOverride)
        Require(byte == 0x5A);
    return 0;
}
