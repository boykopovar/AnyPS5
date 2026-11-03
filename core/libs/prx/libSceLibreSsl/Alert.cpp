#include "prx/libSceLibreSsl/libSceLibreSsl.hpp"

extern "C" const char* APS5_VABI SSL_alert_desc_string_long_nid_postfix(int value) {
    struct Description {
        unsigned int code;
        const char* text;
    };
    static constexpr Description descriptions[] = {
        {0, "close notify"},
        {10, "unexpected_message"},
        {20, "bad record mac"},
        {21, "decryption failed"},
        {22, "record overflow"},
        {30, "decompression failure"},
        {40, "handshake failure"},
        {41, "no certificate"},
        {42, "bad certificate"},
        {43, "unsupported certificate"},
        {44, "certificate revoked"},
        {45, "certificate expired"},
        {46, "certificate unknown"},
        {47, "illegal parameter"},
        {48, "unknown CA"},
        {49, "access denied"},
        {50, "decode error"},
        {51, "decrypt error"},
        {60, "export restriction"},
        {70, "protocol version"},
        {71, "insufficient security"},
        {80, "internal error"},
        {90, "user canceled"},
        {100, "no renegotiation"},
        {110, "unsupported extension"},
        {111, "certificate unobtainable"},
        {112, "unrecognized name"},
        {113, "bad certificate status response"},
        {114, "bad certificate hash value"},
        {115, "unknown PSK identity"},
    };
    const auto code = static_cast<unsigned int>(value) & 0xffu;
    for (const auto& description : descriptions) {
        if (description.code == code) return description.text;
    }
    return "unknown";
}
