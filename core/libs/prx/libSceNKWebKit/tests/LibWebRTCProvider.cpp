#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstdio>
#include <cstdlib>

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "LibWebRTCProvider check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

int main() {
    Require(!_ZN7WebCore17LibWebRTCProvider15webRTCAvailableEv());
    _ZN7WebCore17LibWebRTCProvider24registerWebKitVP9DecoderEv();
    _ZN7WebCore17LibWebRTCProvider29setH264HardwareEncoderAllowedEb(true);
    _ZN7WebCore17LibWebRTCProvider29setH264HardwareEncoderAllowedEb(false);
    Require(!_ZN7WebCore17LibWebRTCProvider15webRTCAvailableEv());
    return 0;
}
