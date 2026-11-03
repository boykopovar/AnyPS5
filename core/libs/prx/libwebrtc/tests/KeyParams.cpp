#include "prx/libwebrtc/libwebrtc.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "libwebrtc KeyParams check failed at line %u\n", location.line());
    std::abort();
}

template<typename TException, typename TAction> void RequireThrows(TAction action, std::source_location location = std::source_location::current()) {
    try {
        action();
    } catch (const TException&) {
        return;
    }
    Require(false, location);
}

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
        RequireThrows<std::invalid_argument>([&] { _ZN3rtc9KeyParamsC1ENS_7KeyTypeE_nid_postfix(&params, type); });
        Require(Unchanged(params));
    }
    RequireThrows<std::invalid_argument>([] { _ZN3rtc9KeyParamsC1ENS_7KeyTypeE_nid_postfix(nullptr, WebRtcKeyTypeEcdsa); });
}

}

int main() {
    TestEcdsa();
    TestUnknownTypesThrow();
    return 0;
}
