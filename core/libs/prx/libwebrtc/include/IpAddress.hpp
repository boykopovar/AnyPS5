#pragma once

#include <cstddef>
#include <cstdint>

constexpr std::int32_t WebRtcAfUnspec = 0;
constexpr std::int32_t WebRtcAfInet = 2;
constexpr std::int32_t WebRtcAfInet6 = 28;

struct WebRtcInAddr {
    std::uint32_t sAddr;
};

struct WebRtcIn6Addr {
    std::uint8_t bytes[16];
};

struct WebRtcIpAddress {
    const void* vtable;
    std::int32_t family;
    union {
        WebRtcInAddr ip4;
        WebRtcIn6Addr ip6;
    } address;
};

static_assert(sizeof(WebRtcInAddr) == 4);
static_assert(sizeof(WebRtcIn6Addr) == 16);
static_assert(sizeof(WebRtcIpAddress) == 32);
static_assert(offsetof(WebRtcIpAddress, family) == 8);
static_assert(offsetof(WebRtcIpAddress, address) == 12);
