#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"
#include "Testing.hpp"

#include <cstdio>
#include <cstdlib>

int main() {
    Require(!_ZN7WebCore17LibWebRTCProvider15webRTCAvailableEv());
    _ZN7WebCore17LibWebRTCProvider24registerWebKitVP9DecoderEv();
    _ZN7WebCore17LibWebRTCProvider29setH264HardwareEncoderAllowedEb(true);
    _ZN7WebCore17LibWebRTCProvider29setH264HardwareEncoderAllowedEb(false);
    Require(!_ZN7WebCore17LibWebRTCProvider15webRTCAvailableEv());
    return 0;
}
