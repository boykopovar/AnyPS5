#include "prx/libwebrtc/include/AudioEncoderRuntimeConfig.hpp"
#include "prx/libwebrtc/libwebrtc.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "libwebrtc AudioEncoderRuntimeConfig check failed at line %u\n", location.line());
    std::abort();
}

void TestDestructor() {
    unsigned char storage[64];
    std::memset(storage, 0xAB, sizeof(storage));
    auto* config = reinterpret_cast<WebRtcAudioEncoderRuntimeConfig*>(storage);
    _ZN6webrtc25AudioEncoderRuntimeConfigD1Ev_nid_postfix(config);
    for (unsigned char byte : storage)
        Require(byte == 0xAB);
    _ZN6webrtc25AudioEncoderRuntimeConfigD1Ev_nid_postfix(nullptr);
}

}

int main() {
    TestDestructor();
    return 0;
}
