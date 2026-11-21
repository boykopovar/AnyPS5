#pragma once

#include <cstddef>
#include <cstdint>

struct WebRtcRtcCertificate {
    std::int32_t refCount;
    void* identity;
};

static_assert(sizeof(WebRtcRtcCertificate) == 16);
static_assert(offsetof(WebRtcRtcCertificate, identity) == 8);
