#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int CES_ERROR_INVALID_PARAMETER = static_cast<int>(0x805C0001);
constexpr int CES_ERROR_INVALID_SRC_BUFFER = static_cast<int>(0x805C0010);
constexpr int CES_ERROR_SRC_BUFFER_END = static_cast<int>(0x805C0011);
constexpr int CES_ERROR_INVALID_ENCODE = static_cast<int>(0x805C0014);
constexpr int CES_ERROR_UNASSIGNED_CODE = static_cast<int>(0x805C0020);
constexpr int CES_ERROR_INVALID_DST_BUFFER = static_cast<int>(0x805C0030);
constexpr int CES_ERROR_DST_BUFFER_END = static_cast<int>(0x805C0031);

constexpr uint8_t CP1252_PROFILE = 0;

constexpr uint16_t CP1252_HIGH[32] = {
    0x20AC, 0x0081, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021, 0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0x008D, 0x017D, 0x008F,
    0x0090, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014, 0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0x009D, 0x017E, 0x0178,
};

uint32_t cp1252ToUnicode(uint8_t sbc) {
    return sbc >= 0x80 && sbc < 0xA0 ? CP1252_HIGH[sbc - 0x80] : sbc;
}

bool unicodeToCp1252(uint32_t code, uint8_t& sbc) {
    if (code < 0x80 || (code >= 0xA0 && code <= 0xFF)) {
        sbc = static_cast<uint8_t>(code);
        return true;
    }
    for (uint8_t i = 0; i < 32; ++i) {
        if (CP1252_HIGH[i] == code) {
            sbc = static_cast<uint8_t>(0x80 + i);
            return true;
        }
    }
    return false;
}

int decodeUtf8(const uint8_t* utf8, uint32_t utf8max, uint32_t& length, uint32_t& code) {
    const uint8_t lead = utf8[0];
    uint32_t minimum = 0;
    if (lead < 0x80) {
        length = 1;
        code = lead;
        return 0;
    }
    if ((lead & 0xE0) == 0xC0) { length = 2; code = lead & 0x1F; minimum = 0x80; }
    else if ((lead & 0xF0) == 0xE0) { length = 3; code = lead & 0x0F; minimum = 0x800; }
    else if ((lead & 0xF8) == 0xF0) { length = 4; code = lead & 0x07; minimum = 0x10000; }
    else return CES_ERROR_INVALID_ENCODE;

    for (uint32_t i = 1; i < length; ++i) {
        if (i >= utf8max) return CES_ERROR_SRC_BUFFER_END;
        if ((utf8[i] & 0xC0) != 0x80) return CES_ERROR_INVALID_ENCODE;
        code = code << 6 | (utf8[i] & 0x3F);
    }
    if (code < minimum || code > 0x10FFFF || (code >= 0xD800 && code <= 0xDFFF)) return CES_ERROR_INVALID_ENCODE;
    return 0;
}

uint32_t encodeUtf8(uint32_t code, uint8_t* utf8) {
    if (code < 0x80) {
        utf8[0] = static_cast<uint8_t>(code);
        return 1;
    }
    if (code < 0x800) {
        utf8[0] = static_cast<uint8_t>(0xC0 | code >> 6);
        utf8[1] = static_cast<uint8_t>(0x80 | (code & 0x3F));
        return 2;
    }
    utf8[0] = static_cast<uint8_t>(0xE0 | code >> 12);
    utf8[1] = static_cast<uint8_t>(0x80 | (code >> 6 & 0x3F));
    utf8[2] = static_cast<uint8_t>(0x80 | (code & 0x3F));
    return 3;
}

}

extern "C" {

const uint8_t* APS5_VABI sceCesRefersUcsProfileCp1252(void) {
    return &CP1252_PROFILE;
}

const void* APS5_VABI sceCesRefersUcsProfileCp1250(void) {
    NotImplemented_nid_no_patch(__func__);
    return nullptr;
}

const void* APS5_VABI sceCesRefersUcsProfileCp1251(void) {
    NotImplemented_nid_no_patch(__func__);
    return nullptr;
}

const void* APS5_VABI sceCesUcsProfileInitSJis1997Cp932(void* buffer) {
    (void)buffer;
    NotImplemented_nid_no_patch(__func__);
    return nullptr;
}

int APS5_VABI sceCesMbcsUcsContextInit(void* context, const void* profile) {
    (void)context;
    (void)profile;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCesSbcToUtf8(const uint8_t* profile, uint8_t sbc, uint8_t* utf8, uint32_t utf8max, uint32_t* utf8_len) {
    if (profile != &CP1252_PROFILE) return CES_ERROR_INVALID_PARAMETER;
    if (!utf8) return CES_ERROR_INVALID_DST_BUFFER;

    uint8_t encoded[3];
    const uint32_t length = encodeUtf8(cp1252ToUnicode(sbc), encoded);
    if (utf8max < length) return CES_ERROR_DST_BUFFER_END;
    for (uint32_t i = 0; i < length; ++i) utf8[i] = encoded[i];
    if (utf8_len) *utf8_len = length;
    return 0;
}

int APS5_VABI sceCesUtf8ToSbc(const uint8_t* utf8, uint32_t utf8max, uint32_t* utf8_len, const uint8_t* profile, uint8_t* sbc) {
    if (profile != &CP1252_PROFILE) return CES_ERROR_INVALID_PARAMETER;
    if (!utf8) return CES_ERROR_INVALID_SRC_BUFFER;
    if (utf8max == 0) return CES_ERROR_SRC_BUFFER_END;
    if (!sbc) return CES_ERROR_INVALID_DST_BUFFER;

    uint32_t length = 0;
    uint32_t code = 0;
    if (const int result = decodeUtf8(utf8, utf8max, length, code); result != 0) return result;
    if (!unicodeToCp1252(code, *sbc)) return CES_ERROR_UNASSIGNED_CODE;
    if (utf8_len) *utf8_len = length;
    return 0;
}

}
