#include "prx/libwebrtc/libwebrtc.hpp"
#include "Testing.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>

int main() {
    static unsigned char object[1040];
    std::memset(object, 0xA5, sizeof(object));
    _ZN6webrtc12VideoEncoder21RateControlParametersD1Ev_nid_postfix(object);
    for (unsigned char byte : object)
        Require(byte == 0xA5);
    return 0;
}
