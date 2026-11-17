#include "prx/libwebrtc/libwebrtc.hpp"
#include "Testing.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <source_location>
#include <stdexcept>

namespace {

WebRtcKeyParams Filled() {
    WebRtcKeyParams params;
    std::memset(&params, 0xA5, sizeof(params));
    return params;
}

bool Unchanged(const WebRtcKeyParams& params) {
    WebRtcKeyParams filled = Filled();
    return std::memcmp(&params, &filled, sizeof(params)) == 0;
}

void TestEcdsa() {
    WebRtcKeyParams params = Filled();
    _ZN3rtc9KeyParamsC1ENS_7KeyTypeE_nid_postfix(&params, WebRtcKeyTypeEcdsa);
    Require(params.type == WebRtcKeyTypeEcdsa);
    Require(params.curve == WebRtcEcNistP256);
    std::uint32_t publicExponent = 0;
    std::memcpy(&publicExponent, reinterpret_cast<const unsigned char*>(&params) + 8, sizeof(publicExponent));
    Require(publicExponent == 0xA5A5A5A5u);
}

void TestUnknownTypesThrow() {
    for (std::int32_t type : {2, 3, -1, 0x7fffffff}) {
        WebRtcKeyParams params = Filled();
        ExpectErr<std::invalid_argument>([&] { _ZN3rtc9KeyParamsC1ENS_7KeyTypeE_nid_postfix(&params, type); });
        Require(Unchanged(params));
    }
    ExpectErr<std::invalid_argument>([] { _ZN3rtc9KeyParamsC1ENS_7KeyTypeE_nid_postfix(nullptr, WebRtcKeyTypeEcdsa); });
}

}

int main() {
    TestEcdsa();
    TestUnknownTypesThrow();
    return 0;
}
