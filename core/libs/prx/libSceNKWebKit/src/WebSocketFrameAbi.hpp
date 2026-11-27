#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace NKWebKit::WebCoreAbi {

struct LegacyWebSocketFrame {
    std::uint32_t opCode;
    bool final;
    bool compress;
    bool reserved2;
    bool reserved3;
    bool masked;
    const char* payload;
    std::size_t payloadLength;
};

static_assert(sizeof(LegacyWebSocketFrame) == 32);
static_assert(offsetof(LegacyWebSocketFrame, final) == 4);
static_assert(offsetof(LegacyWebSocketFrame, masked) == 8);
static_assert(offsetof(LegacyWebSocketFrame, payload) == 16);
static_assert(offsetof(LegacyWebSocketFrame, payloadLength) == 24);
static_assert(std::is_trivially_copyable_v<LegacyWebSocketFrame>);

}  // namespace NKWebKit::WebCoreAbi
