#pragma once

#include "prx/libwebrtc/include/GuestString.hpp"

#include <cstddef>
#include <cstdint>

struct WebRtcRtcError {
    std::int32_t type;
    WebRtcGuestString message;
    std::int32_t errorDetail;
    std::uint16_t sctpCauseCode;
    bool hasSctpCauseCode;
};

static_assert(sizeof(WebRtcRtcError) == 56);
static_assert(offsetof(WebRtcRtcError, message) == 8);
static_assert(offsetof(WebRtcRtcError, errorDetail) == 48);
static_assert(offsetof(WebRtcRtcError, sctpCauseCode) == 52);
static_assert(offsetof(WebRtcRtcError, hasSctpCauseCode) == 54);
