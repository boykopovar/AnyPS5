#pragma once

#include <cstddef>
#include <cstdint>

constexpr std::int32_t WebRtcKeyTypeRsa = 0;
constexpr std::int32_t WebRtcKeyTypeEcdsa = 1;
constexpr std::int32_t WebRtcEcNistP256 = 0;

struct WebRtcKeyParams {
    std::int32_t type;
    union {
        struct {
            std::uint32_t modSize;
            std::uint32_t publicExponent;
        } rsa;
        std::int32_t curve;
    };
};

static_assert(sizeof(WebRtcKeyParams) == 12);
static_assert(offsetof(WebRtcKeyParams, curve) == 4);
