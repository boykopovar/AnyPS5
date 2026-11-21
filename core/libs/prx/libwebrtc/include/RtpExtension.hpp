#pragma once

#include "prx/libwebrtc/include/GuestString.hpp"

#include <cstddef>
#include <cstdint>

struct WebRtcRtpExtension {
    WebRtcGuestString uri;
    std::int32_t id;
    bool encrypt;
};

static_assert(sizeof(WebRtcRtpExtension) == 48);
static_assert(offsetof(WebRtcRtpExtension, id) == 40);
static_assert(offsetof(WebRtcRtpExtension, encrypt) == 44);
