#pragma once

#include "prx/libwebrtc/include/GuestString.hpp"
#include "prx/libwebrtc/include/IpAddress.hpp"

#include <cstddef>
#include <cstdint>

struct WebRtcSocketAddress {
    WebRtcGuestString hostname;
    WebRtcIpAddress ip;
    std::uint16_t port;
    std::int32_t scopeId;
    bool literal;
};

static_assert(sizeof(WebRtcSocketAddress) == 88);
static_assert(offsetof(WebRtcSocketAddress, ip) == 40);
static_assert(offsetof(WebRtcSocketAddress, port) == 72);
static_assert(offsetof(WebRtcSocketAddress, scopeId) == 76);
static_assert(offsetof(WebRtcSocketAddress, literal) == 80);
