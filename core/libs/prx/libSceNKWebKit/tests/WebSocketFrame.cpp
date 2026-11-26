#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

using NKWebKit::WebCoreAbi::LegacyWebSocketFrame;

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "web socket frame check failed at line %d\n", line);
        std::fflush(stderr);
        std::_Exit(1);
    }
}
#define Require(value) Check((value), __LINE__)

static const auto constructors = {&_ZN7WebCore14WebSocketFrameC1ENS0_6OpCodeEbbbPKcm, &_ZN7WebCore14WebSocketFrameC2ENS0_6OpCodeEbbbPKcm};

static bool Flag(const unsigned char* bytes, std::size_t offset) {
    Require(bytes[offset] == 0 || bytes[offset] == 1);
    return bytes[offset] == 1;
}

int main() {
    static const char payload[] = "payload";
    const std::uint32_t opCodes[] = {0x0, 0x1, 0x2, 0x8, 0x9, 0xA, 0x10};
    for (const auto construct : constructors) {
        for (const std::uint32_t opCode : opCodes)
            for (unsigned bits = 0; bits < 8; ++bits) {
                const bool final = (bits & 1) != 0;
                const bool compress = (bits & 2) != 0;
                const bool masked = (bits & 4) != 0;
                const char* data = (bits & 1) ? payload : nullptr;
                const std::size_t length = (bits & 1) ? 7 + (static_cast<std::size_t>(opCode) << 33) : 0;
                alignas(8) unsigned char bytes[32];
                std::memset(bytes, 0xA5, sizeof(bytes));
                auto* frame = reinterpret_cast<LegacyWebSocketFrame*>(bytes);
                construct(frame, opCode, final, compress, masked, data, length);
                Require(frame->opCode == opCode && frame->payload == data && frame->payloadLength == length);
                Require(Flag(bytes, 4) == final && Flag(bytes, 5) == compress && Flag(bytes, 8) == masked);
                Require(!Flag(bytes, 6) && !Flag(bytes, 7));
            }
        bool thrown = false;
        try {
            construct(nullptr, 0x1, true, false, false, payload, 7);
        } catch (const std::invalid_argument&) {
            thrown = true;
        }
        Require(thrown);
    }
    std::puts("libSceNKWebKit web socket frame tests passed");
    return 0;
}
