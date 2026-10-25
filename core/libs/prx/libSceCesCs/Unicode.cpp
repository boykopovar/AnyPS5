#include "prx/libSceCesCs/include/Api.hpp"
#include <bit>
#include <cstdint>
#include <cstring>
#include "prx/libc/include/general/VabiMacros.hpp"

namespace {

constexpr int INVALID_SRC_BUFFER = static_cast<int>(0x805C0010);
constexpr int SRC_BUFFER_END = static_cast<int>(0x805C0011);
constexpr int INVALID_ENCODE = static_cast<int>(0x805C0014);
constexpr int UNASSIGNED_CODE = static_cast<int>(0x805C0020);
constexpr int INVALID_DST_BUFFER = static_cast<int>(0x805C0030);
constexpr int DST_BUFFER_END = static_cast<int>(0x805C0031);
constexpr bool NATIVE_BIG_ENDIAN = std::endian::native == std::endian::big;

bool isScalar(uint32_t code) {
    return code <= 0x10FFFF && (code < 0xD800 || code > 0xDFFF);
}

int decodeUtf8(const uint8_t* source, uint32_t available, uint32_t& code, uint32_t& length) {
    if (!source) return INVALID_SRC_BUFFER;
    if (available == 0) return SRC_BUFFER_END;
    const uint8_t lead = source[0];
    if (lead <= 0x7F) { code = lead; length = 1; return 0; }
    if (lead >= 0xC2 && lead <= 0xDF) { code = lead & 0x1F; length = 2; }
    else if (lead >= 0xE0 && lead <= 0xEF) { code = lead & 0x0F; length = 3; }
    else if (lead >= 0xF0 && lead <= 0xF4) { code = lead & 0x07; length = 4; }
    else return INVALID_ENCODE;
    for (uint32_t i = 1; i < length; ++i) {
        if (i >= available) return SRC_BUFFER_END;
        const uint8_t next = source[i];
        if (next < 0x80 || next > 0xBF) return INVALID_ENCODE;
        if (i == 1 && ((lead == 0xE0 && next < 0xA0) || (lead == 0xED && next > 0x9F)
            || (lead == 0xF0 && next < 0x90) || (lead == 0xF4 && next > 0x8F))) return INVALID_ENCODE;
        code = code << 6 | (next & 0x3F);
    }
    return 0;
}

int encodeUtf8(uint32_t code, uint8_t* target, uint32_t capacity, uint32_t* targetLength) {
    if (!isScalar(code)) return INVALID_ENCODE;
    if (!target) return INVALID_DST_BUFFER;
    uint8_t bytes[4];
    uint32_t length = 0;
    if (code < 0x80) { bytes[0] = static_cast<uint8_t>(code); length = 1; }
    else if (code < 0x800) {
        bytes[0] = static_cast<uint8_t>(0xC0 | code >> 6);
        bytes[1] = static_cast<uint8_t>(0x80 | (code & 0x3F));
        length = 2;
    } else if (code < 0x10000) {
        bytes[0] = static_cast<uint8_t>(0xE0 | code >> 12);
        bytes[1] = static_cast<uint8_t>(0x80 | (code >> 6 & 0x3F));
        bytes[2] = static_cast<uint8_t>(0x80 | (code & 0x3F));
        length = 3;
    } else {
        bytes[0] = static_cast<uint8_t>(0xF0 | code >> 18);
        bytes[1] = static_cast<uint8_t>(0x80 | (code >> 12 & 0x3F));
        bytes[2] = static_cast<uint8_t>(0x80 | (code >> 6 & 0x3F));
        bytes[3] = static_cast<uint8_t>(0x80 | (code & 0x3F));
        length = 4;
    }
    if (capacity < length) return DST_BUFFER_END;
    std::memcpy(target, bytes, length);
    if (targetLength) *targetLength = length;
    return 0;
}

template<typename Unit, bool BigEndian>
uint32_t loadUnit(const Unit* source) {
    const auto* bytes = reinterpret_cast<const uint8_t*>(source);
    uint32_t code = 0;
    for (uint32_t i = 0; i < sizeof(Unit); ++i) code |= uint32_t(bytes[i]) << (8 * (BigEndian ? sizeof(Unit) - 1 - i : i));
    return code;
}

template<typename Unit, bool BigEndian>
void storeUnit(Unit* target, uint32_t code) {
    auto* bytes = reinterpret_cast<uint8_t*>(target);
    for (uint32_t i = 0; i < sizeof(Unit); ++i) bytes[i] = static_cast<uint8_t>(code >> (8 * (BigEndian ? sizeof(Unit) - 1 - i : i)));
}

template<bool BigEndian>
int utf8ToUtf32(const uint8_t* source, uint32_t available, uint32_t* sourceLength, uint32_t* target) {
    uint32_t code = 0;
    uint32_t length = 0;
    if (const int result = decodeUtf8(source, available, code, length); result != 0) return result;
    if (!target) return INVALID_DST_BUFFER;
    storeUnit<uint32_t, BigEndian>(target, code);
    if (sourceLength) *sourceLength = length;
    return 0;
}

template<bool BigEndian>
int utf8ToUtf16(const uint8_t* source, uint32_t available, uint32_t* sourceLength, uint16_t* target, uint32_t capacity, uint32_t* targetLength) {
    uint32_t code = 0;
    uint32_t length = 0;
    if (const int result = decodeUtf8(source, available, code, length); result != 0) return result;
    if (!target) return INVALID_DST_BUFFER;
    const uint32_t units = code < 0x10000 ? 1 : 2;
    if (capacity < units) return DST_BUFFER_END;
    if (units == 1) storeUnit<uint16_t, BigEndian>(target, code);
    else {
        code -= 0x10000;
        storeUnit<uint16_t, BigEndian>(target, 0xD800 | code >> 10);
        storeUnit<uint16_t, BigEndian>(target + 1, 0xDC00 | (code & 0x3FF));
    }
    if (sourceLength) *sourceLength = length;
    if (targetLength) *targetLength = units;
    return 0;
}

template<bool BigEndian>
int utf16ToUtf8(const uint16_t* source, uint32_t available, uint32_t* sourceLength, uint8_t* target, uint32_t capacity, uint32_t* targetLength) {
    if (!source) return INVALID_SRC_BUFFER;
    if (available == 0) return SRC_BUFFER_END;
    uint32_t code = loadUnit<uint16_t, BigEndian>(source);
    uint32_t units = 1;
    if (code >= 0xDC00 && code <= 0xDFFF) return INVALID_ENCODE;
    if (code >= 0xD800 && code <= 0xDBFF) {
        if (available < 2) return SRC_BUFFER_END;
        const uint32_t low = loadUnit<uint16_t, BigEndian>(source + 1);
        if (low < 0xDC00 || low > 0xDFFF) return INVALID_ENCODE;
        code = 0x10000 + ((code - 0xD800) << 10) + low - 0xDC00;
        units = 2;
    }
    if (const int result = encodeUtf8(code, target, capacity, targetLength); result != 0) return result;
    if (sourceLength) *sourceLength = units;
    return 0;
}

template<bool BigEndian>
int utf32ToUtf8(const uint32_t* source, uint8_t* target, uint32_t capacity, uint32_t* targetLength) {
    if (!source) return INVALID_SRC_BUFFER;
    return encodeUtf8(loadUnit<uint32_t, BigEndian>(source), target, capacity, targetLength);
}

}

extern "C" {

int APS5_VABI sceCesUcs2ToUtf8(uint16_t source, uint8_t* target, uint32_t capacity, uint32_t* targetLength) {
    return encodeUtf8(source, target, capacity, targetLength);
}

int APS5_VABI sceCesUtf8ToUcs2(const uint8_t* source, uint32_t available, uint32_t* sourceLength, uint16_t* target) {
    uint32_t code = 0;
    uint32_t length = 0;
    if (const int result = decodeUtf8(source, available, code, length); result != 0) return result;
    if (!target) return INVALID_DST_BUFFER;
    if (code > 0xFFFF) return UNASSIGNED_CODE;
    *target = static_cast<uint16_t>(code);
    if (sourceLength) *sourceLength = length;
    return 0;
}

int APS5_VABI sceCesUtf8ToUtf32(const uint8_t* source, uint32_t available, uint32_t* sourceLength, uint32_t* target) {
    return utf8ToUtf32<NATIVE_BIG_ENDIAN>(source, available, sourceLength, target);
}

int APS5_VABI sceCesUtf8ToUtf32be(const uint8_t* source, uint32_t available, uint32_t* sourceLength, uint32_t* target) {
    return utf8ToUtf32<true>(source, available, sourceLength, target);
}

int APS5_VABI sceCesUtf8ToUtf32le(const uint8_t* source, uint32_t available, uint32_t* sourceLength, uint32_t* target) {
    return utf8ToUtf32<false>(source, available, sourceLength, target);
}

int APS5_VABI sceCesUtf8ToUtf16(const uint8_t* source, uint32_t available, uint32_t* sourceLength, uint16_t* target, uint32_t capacity, uint32_t* targetLength) {
    return utf8ToUtf16<NATIVE_BIG_ENDIAN>(source, available, sourceLength, target, capacity, targetLength);
}

int APS5_VABI sceCesUtf8ToUtf16be(const uint8_t* source, uint32_t available, uint32_t* sourceLength, uint16_t* target, uint32_t capacity, uint32_t* targetLength) {
    return utf8ToUtf16<true>(source, available, sourceLength, target, capacity, targetLength);
}

int APS5_VABI sceCesUtf8ToUtf16le(const uint8_t* source, uint32_t available, uint32_t* sourceLength, uint16_t* target, uint32_t capacity, uint32_t* targetLength) {
    return utf8ToUtf16<false>(source, available, sourceLength, target, capacity, targetLength);
}

int APS5_VABI sceCesUtf16ToUtf8(const uint16_t* source, uint32_t available, uint32_t* sourceLength, uint8_t* target, uint32_t capacity, uint32_t* targetLength) {
    return utf16ToUtf8<NATIVE_BIG_ENDIAN>(source, available, sourceLength, target, capacity, targetLength);
}

int APS5_VABI sceCesUtf16beToUtf8(const uint16_t* source, uint32_t available, uint32_t* sourceLength, uint8_t* target, uint32_t capacity, uint32_t* targetLength) {
    return utf16ToUtf8<true>(source, available, sourceLength, target, capacity, targetLength);
}

int APS5_VABI sceCesUtf16leToUtf8(const uint16_t* source, uint32_t available, uint32_t* sourceLength, uint8_t* target, uint32_t capacity, uint32_t* targetLength) {
    return utf16ToUtf8<false>(source, available, sourceLength, target, capacity, targetLength);
}

int APS5_VABI sceCesUtf32ToUtf8(uint32_t source, uint8_t* target, uint32_t capacity, uint32_t* targetLength) {
    return encodeUtf8(source, target, capacity, targetLength);
}

int APS5_VABI sceCesUtf32beToUtf8(const uint32_t* source, uint8_t* target, uint32_t capacity, uint32_t* targetLength) {
    return utf32ToUtf8<true>(source, target, capacity, targetLength);
}

int APS5_VABI sceCesUtf32leToUtf8(const uint32_t* source, uint8_t* target, uint32_t capacity, uint32_t* targetLength) {
    return utf32ToUtf8<false>(source, target, capacity, targetLength);
}

}
