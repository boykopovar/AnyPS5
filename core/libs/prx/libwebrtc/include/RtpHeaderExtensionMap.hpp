#pragma once

#include "prx/libwebrtc/include/RtpExtension.hpp"

#include <cstddef>
#include <cstdint>

constexpr std::size_t WebRtcRtpExtensionNumberOfExtensions = 22;

struct WebRtcRtpExtensionView {
    const WebRtcRtpExtension* data;
    std::size_t size;
};

struct WebRtcRtpHeaderExtensionMap {
    std::uint8_t ids[WebRtcRtpExtensionNumberOfExtensions];
    bool extmapAllowMixed;
};

static_assert(sizeof(WebRtcRtpExtensionView) == 16);
static_assert(offsetof(WebRtcRtpExtensionView, size) == 8);
static_assert(sizeof(WebRtcRtpHeaderExtensionMap) == 23);
static_assert(offsetof(WebRtcRtpHeaderExtensionMap, extmapAllowMixed) == 22);
