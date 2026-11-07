#include "prx/libwebrtc/libwebrtc.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "libwebrtc webrtc::VideoEncoder::RateControlParameters destructor check failed at line %u\n", location.line());
    std::abort();
}

}

int main() {
    static unsigned char object[1040];
    std::memset(object, 0xA5, sizeof(object));
    _ZN6webrtc12VideoEncoder21RateControlParametersD1Ev_nid_postfix(object);
    for (unsigned char byte : object)
        Require(byte == 0xA5);
    return 0;
}
