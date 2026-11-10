#pragma once

constexpr unsigned WebRtcEchoCanceller3ConfigSize = 608;

struct alignas(8) WebRtcEchoCanceller3Config {
    unsigned char bytes[WebRtcEchoCanceller3ConfigSize];
};

static_assert(sizeof(WebRtcEchoCanceller3Config) == 608);
static_assert(alignof(WebRtcEchoCanceller3Config) == 8);
