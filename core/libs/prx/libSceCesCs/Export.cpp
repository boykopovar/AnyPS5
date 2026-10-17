#include <cstdint>
#include <cstddef>
#include <algorithm>
#include <cstring>
#include <utility>
#include <vector>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "JisX0208.hpp"

namespace {

constexpr int CES_ERROR_INVALID_PARAMETER = static_cast<int>(0x805C0001);
constexpr int CES_ERROR_INVALID_SRC_BUFFER = static_cast<int>(0x805C0010);
constexpr int CES_ERROR_SRC_BUFFER_END = static_cast<int>(0x805C0011);
constexpr int CES_ERROR_INVALID_ENCODE = static_cast<int>(0x805C0014);
constexpr int CES_ERROR_UNASSIGNED_CODE = static_cast<int>(0x805C0020);
constexpr int CES_ERROR_INVALID_DST_BUFFER = static_cast<int>(0x805C0030);
constexpr int CES_ERROR_DST_BUFFER_END = static_cast<int>(0x805C0031);

constexpr uint8_t PROFILES[3] = {0, 1, 2};
constexpr const uint8_t* CP1252_PROFILE = &PROFILES[0];
constexpr const uint8_t* CP1251_PROFILE = &PROFILES[1];
constexpr const uint8_t* CP1250_PROFILE = &PROFILES[2];

constexpr uint16_t CP1252_HIGH[32] = {
    0x20AC, 0x0081, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021, 0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0x008D, 0x017D, 0x008F,
    0x0090, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014, 0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0x009D, 0x017E, 0x0178,
};

constexpr uint16_t CP1251_HIGH[128] = {
    0x0402, 0x0403, 0x201A, 0x0453, 0x201E, 0x2026, 0x2020, 0x2021, 0x20AC, 0x2030, 0x0409, 0x2039, 0x040A, 0x040C, 0x040B, 0x040F,
    0x0452, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014, 0x0098, 0x2122, 0x0459, 0x203A, 0x045A, 0x045C, 0x045B, 0x045F,
    0x00A0, 0x040E, 0x045E, 0x0408, 0x00A4, 0x0490, 0x00A6, 0x00A7, 0x0401, 0x00A9, 0x0404, 0x00AB, 0x00AC, 0x00AD, 0x00AE, 0x0407,
    0x00B0, 0x00B1, 0x0406, 0x0456, 0x0491, 0x00B5, 0x00B6, 0x00B7, 0x0451, 0x2116, 0x0454, 0x00BB, 0x0458, 0x0405, 0x0455, 0x0457,
    0x0410, 0x0411, 0x0412, 0x0413, 0x0414, 0x0415, 0x0416, 0x0417, 0x0418, 0x0419, 0x041A, 0x041B, 0x041C, 0x041D, 0x041E, 0x041F,
    0x0420, 0x0421, 0x0422, 0x0423, 0x0424, 0x0425, 0x0426, 0x0427, 0x0428, 0x0429, 0x042A, 0x042B, 0x042C, 0x042D, 0x042E, 0x042F,
    0x0430, 0x0431, 0x0432, 0x0433, 0x0434, 0x0435, 0x0436, 0x0437, 0x0438, 0x0439, 0x043A, 0x043B, 0x043C, 0x043D, 0x043E, 0x043F,
    0x0440, 0x0441, 0x0442, 0x0443, 0x0444, 0x0445, 0x0446, 0x0447, 0x0448, 0x0449, 0x044A, 0x044B, 0x044C, 0x044D, 0x044E, 0x044F,
};

constexpr uint16_t CP1250_HIGH[128] = {
    0x20AC, 0x0081, 0x201A, 0x0083, 0x201E, 0x2026, 0x2020, 0x2021, 0x0088, 0x2030, 0x0160, 0x2039, 0x015A, 0x0164, 0x017D, 0x0179,
    0x0090, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014, 0x0098, 0x2122, 0x0161, 0x203A, 0x015B, 0x0165, 0x017E, 0x017A,
    0x00A0, 0x02C7, 0x02D8, 0x0141, 0x00A4, 0x0104, 0x00A6, 0x00A7, 0x00A8, 0x00A9, 0x015E, 0x00AB, 0x00AC, 0x00AD, 0x00AE, 0x017B,
    0x00B0, 0x00B1, 0x02DB, 0x0142, 0x00B4, 0x00B5, 0x00B6, 0x00B7, 0x00B8, 0x0105, 0x015F, 0x00BB, 0x013D, 0x02DD, 0x013E, 0x017C,
    0x0154, 0x00C1, 0x00C2, 0x0102, 0x00C4, 0x0139, 0x0106, 0x00C7, 0x010C, 0x00C9, 0x0118, 0x00CB, 0x011A, 0x00CD, 0x00CE, 0x010E,
    0x0110, 0x0143, 0x0147, 0x00D3, 0x00D4, 0x0150, 0x00D6, 0x00D7, 0x0158, 0x016E, 0x00DA, 0x0170, 0x00DC, 0x00DD, 0x0162, 0x00DF,
    0x0155, 0x00E1, 0x00E2, 0x0103, 0x00E4, 0x013A, 0x0107, 0x00E7, 0x010D, 0x00E9, 0x0119, 0x00EB, 0x011B, 0x00ED, 0x00EE, 0x010F,
    0x0111, 0x0144, 0x0148, 0x00F3, 0x00F4, 0x0151, 0x00F6, 0x00F7, 0x0159, 0x016F, 0x00FA, 0x0171, 0x00FC, 0x00FD, 0x0163, 0x02D9,
};

constexpr uint16_t CP932_IBM_HEAD[28] = {
    0x2170, 0x2171, 0x2172, 0x2173, 0x2174, 0x2175, 0x2176, 0x2177, 0x2178, 0x2179,
    0x2160, 0x2161, 0x2162, 0x2163, 0x2164, 0x2165, 0x2166, 0x2167, 0x2168, 0x2169,
    0xFFE2, 0xFFE4, 0xFF07, 0xFF02, 0x3231, 0x2116, 0x2121, 0x2235,
};

uint32_t sbcToUnicode(const uint8_t* profile, uint8_t sbc) {
    if (profile == CP1251_PROFILE) return sbc >= 0x80 ? CP1251_HIGH[sbc - 0x80] : sbc;
    if (profile == CP1250_PROFILE) return sbc >= 0x80 ? CP1250_HIGH[sbc - 0x80] : sbc;
    return sbc >= 0x80 && sbc < 0xA0 ? CP1252_HIGH[sbc - 0x80] : sbc;
}

bool unicodeToSbc(const uint8_t* profile, uint32_t code, uint8_t& sbc) {
    if (profile == CP1251_PROFILE) {
        if (code < 0x80) {
            sbc = static_cast<uint8_t>(code);
            return true;
        }
        for (uint32_t i = 0; i < 128; ++i) {
            if (CP1251_HIGH[i] == code) {
                sbc = static_cast<uint8_t>(0x80 + i);
                return true;
            }
        }
        return false;
    }
    if (profile == CP1250_PROFILE) {
        if (code < 0x80) {
            sbc = static_cast<uint8_t>(code);
            return true;
        }
        for (uint32_t i = 0; i < 128; ++i) {
            if (CP1250_HIGH[i] == code) {
                sbc = static_cast<uint8_t>(0x80 + i);
                return true;
            }
        }
        return false;
    }
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

constexpr uint8_t EUC_JP_TAG[8] = {'E', 'U', 'C', 'J', 'P', '9', '3', '2'};
constexpr uint8_t SJIS_TAG[8] = {'S', 'J', 'I', 'S', '1', '9', '9', '7'};

bool isEucJp(const void* profile) {
    return profile != nullptr && std::memcmp(profile, EUC_JP_TAG, sizeof(EUC_JP_TAG)) == 0;
}

bool isSjis(const void* profile) {
    return profile != nullptr && std::memcmp(profile, SJIS_TAG, sizeof(SJIS_TAG)) == 0;
}

bool isSbc(const void* profile) {
    return profile == CP1252_PROFILE || profile == CP1251_PROFILE || profile == CP1250_PROFILE;
}

const void* contextProfile(const void* context) {
    const void* profile = nullptr;
    std::memcpy(&profile, context, sizeof(profile));
    return profile;
}

int decodeMbcs(const void* profile, const uint8_t* mbcs, uint32_t mbcsMax, uint32_t& length, uint32_t& code) {
    const uint8_t lead = mbcs[0];
    if (isSbc(profile)) {
        length = 1;
        code = sbcToUnicode(static_cast<const uint8_t*>(profile), lead);
        return 0;
    }
    if (isSjis(profile)) {
        if (lead < 0x80) {
            length = 1;
            code = lead;
            return 0;
        }
        if (lead >= 0xA1 && lead <= 0xDF) {
            length = 1;
            code = 0xFF61 + (lead - 0xA1);
            return 0;
        }
        if ((lead >= 0x81 && lead <= 0x9F) || (lead >= 0xE0 && lead <= 0xFC)) {
            if (mbcsMax < 2) return CES_ERROR_SRC_BUFFER_END;
            const uint8_t trail = mbcs[1];
            if (trail < 0x40 || trail == 0x7F || trail > 0xFC) return CES_ERROR_INVALID_ENCODE;
            if (lead >= 0xFA) {
                const uint32_t trailIdx = trail < 0x7F ? trail - 0x40 : trail - 0x41;
                const uint32_t offset = (lead - 0xFA) * 188 + trailIdx;
                if (offset <= 387) {
                    if (offset < 28) {
                        code = CP932_IBM_HEAD[offset];
                        length = 2;
                        return 0;
                    }
                    const uint32_t kanjiIdx = offset - 28;
                    const int row = 89 + kanjiIdx / 94;
                    const int cell = 1 + kanjiIdx % 94;
                    code = JisX0208::TO_UCS2[(row - 1) * 94 + (cell - 1)];
                    if (code == 0) return CES_ERROR_UNASSIGNED_CODE;
                    length = 2;
                    return 0;
                }
                return CES_ERROR_UNASSIGNED_CODE;
            }
            int row = (lead <= 0x9F) ? (lead - 0x81) * 2 + 1 : (lead - 0xE0) * 2 + 63;
            int cell = 0;
            if (trail >= 0x9F) {
                row += 1;
                cell = trail - 0x9E;
            } else {
                cell = (trail > 0x7E) ? trail - 0x40 : trail - 0x3F;
            }
            if (row < 1 || row > 94 || cell < 1 || cell > 94) return CES_ERROR_UNASSIGNED_CODE;
            code = JisX0208::TO_UCS2[(row - 1) * 94 + (cell - 1)];
            if (code == 0) return CES_ERROR_UNASSIGNED_CODE;
            length = 2;
            return 0;
        }
        return CES_ERROR_INVALID_ENCODE;
    }
    if (lead < 0x80) {
        length = 1;
        code = lead;
        return 0;
    }
    if (lead == 0x8E) {
        if (mbcsMax < 2) return CES_ERROR_SRC_BUFFER_END;
        if (mbcs[1] < 0xA1 || mbcs[1] > 0xDF) return CES_ERROR_INVALID_ENCODE;
        length = 2;
        code = 0xFF61 + (mbcs[1] - 0xA1);
        return 0;
    }
    if (lead >= 0xA1 && lead <= 0xFE) {
        if (mbcsMax < 2) return CES_ERROR_SRC_BUFFER_END;
        if (mbcs[1] < 0xA1 || mbcs[1] > 0xFE) return CES_ERROR_INVALID_ENCODE;
        length = 2;
        code = JisX0208::TO_UCS2[(lead - 0xA1) * 94 + (mbcs[1] - 0xA1)];
        return code != 0 ? 0 : CES_ERROR_UNASSIGNED_CODE;
    }
    if (lead == 0x8F) NotImplemented_nid_no_patch("EUC-JP JIS X 0212 characters");
    return CES_ERROR_INVALID_ENCODE;
}

const std::vector<std::pair<uint16_t, uint16_t>>& jisX0208Reverse() {
    static const auto reverse = [] {
        std::vector<std::pair<uint16_t, uint16_t>> entries;
        entries.reserve(94 * 94);
        for (uint32_t cell = 0; cell < 94 * 94; ++cell) {
            if (JisX0208::TO_UCS2[cell] != 0) entries.emplace_back(JisX0208::TO_UCS2[cell], static_cast<uint16_t>(cell));
        }
        std::stable_sort(entries.begin(), entries.end(), [](const auto& left, const auto& right) { return left.first < right.first; });
        entries.erase(std::unique(entries.begin(), entries.end(), [](const auto& left, const auto& right) { return left.first == right.first; }), entries.end());
        return entries;
    }();
    return reverse;
}

int encodeMbcs(const void* profile, uint32_t code, uint8_t* mbcs, uint32_t& length) {
    if (isSbc(profile)) {
        length = 1;
        return unicodeToSbc(static_cast<const uint8_t*>(profile), code, mbcs[0]) ? 0 : CES_ERROR_UNASSIGNED_CODE;
    }
    if (isSjis(profile)) {
        if (code < 0x80) {
            length = 1;
            mbcs[0] = static_cast<uint8_t>(code);
            return 0;
        }
        if (code >= 0xFF61 && code <= 0xFF9F) {
            length = 1;
            mbcs[0] = static_cast<uint8_t>(0xA1 + (code - 0xFF61));
            return 0;
        }
        if (code > 0xFFFF) return CES_ERROR_UNASSIGNED_CODE;
        const auto& reverse = jisX0208Reverse();
        const auto found = std::lower_bound(reverse.begin(), reverse.end(), static_cast<uint16_t>(code), [](const std::pair<uint16_t, uint16_t>& entry, uint16_t value) { return entry.first < value; });
        if (found == reverse.end() || found->first != code) return CES_ERROR_UNASSIGNED_CODE;
        const int row = found->second / 94 + 1;
        const int cell = found->second % 94 + 1;
        if (row % 2 != 0) {
            mbcs[0] = static_cast<uint8_t>(row < 63 ? 0x81 + (row - 1) / 2 : 0xE0 + (row - 63) / 2);
            mbcs[1] = static_cast<uint8_t>(cell <= 63 ? cell + 0x3F : cell + 0x40);
        } else {
            mbcs[0] = static_cast<uint8_t>(row < 63 ? 0x81 + (row - 2) / 2 : 0xE0 + (row - 64) / 2);
            mbcs[1] = static_cast<uint8_t>(cell + 0x9E);
        }
        length = 2;
        return 0;
    }
    if (code < 0x80) {
        length = 1;
        mbcs[0] = static_cast<uint8_t>(code);
        return 0;
    }
    if (code >= 0xFF61 && code <= 0xFF9F) {
        length = 2;
        mbcs[0] = 0x8E;
        mbcs[1] = static_cast<uint8_t>(0xA1 + (code - 0xFF61));
        return 0;
    }
    if (code > 0xFFFF) return CES_ERROR_UNASSIGNED_CODE;
    const auto& reverse = jisX0208Reverse();
    const auto found = std::lower_bound(reverse.begin(), reverse.end(), static_cast<uint16_t>(code), [](const std::pair<uint16_t, uint16_t>& entry, uint16_t value) { return entry.first < value; });
    if (found == reverse.end() || found->first != code) return CES_ERROR_UNASSIGNED_CODE;
    length = 2;
    mbcs[0] = static_cast<uint8_t>(0xA1 + found->second / 94);
    mbcs[1] = static_cast<uint8_t>(0xA1 + found->second % 94);
    return 0;
}

int decodeUcs2(const uint16_t* ucs2, uint32_t, uint32_t& length, uint32_t& code) {
    length = 1;
    code = ucs2[0];
    return code >= 0xD800 && code <= 0xDFFF ? CES_ERROR_INVALID_ENCODE : 0;
}

uint32_t encodeUcs2(uint32_t code, uint16_t* ucs2) {
    ucs2[0] = static_cast<uint16_t>(code);
    return 1;
}

template<typename Source, typename Decode>
int toMbcs(const void* context, const Source* source, uint32_t sourceMax, uint32_t* sourceLen, uint8_t* mbcs, uint32_t mbcsMax, uint32_t* mbcsLen, Decode decode) {
    if (!context) return CES_ERROR_INVALID_PARAMETER;
    const void* profile = contextProfile(context);
    if (!isEucJp(profile) && !isSjis(profile) && !isSbc(profile)) return CES_ERROR_INVALID_PARAMETER;
    if (!source) return CES_ERROR_INVALID_SRC_BUFFER;
    uint32_t read = 0;
    uint32_t written = 0;
    while (read < sourceMax && source[read] != 0) {
        uint32_t length = 0;
        uint32_t code = 0;
        if (const int result = decode(source + read, sourceMax - read, length, code); result != 0) return result;
        uint8_t encoded[2];
        uint32_t size = 0;
        if (const int result = encodeMbcs(profile, code, encoded, size); result != 0) return result;
        if (mbcs) {
            if (written + size > mbcsMax) return CES_ERROR_DST_BUFFER_END;
            std::memcpy(mbcs + written, encoded, size);
        }
        read += length;
        written += size;
    }
    if (sourceLen) *sourceLen = read;
    if (mbcsLen) *mbcsLen = written;
    return 0;
}

template<typename Target, typename Encode>
int fromMbcs(const void* context, const uint8_t* mbcs, uint32_t mbcsMax, uint32_t* mbcsLen, Target* target, uint32_t targetMax, uint32_t* targetLen, Encode encode) {
    if (!context) return CES_ERROR_INVALID_PARAMETER;
    const void* profile = contextProfile(context);
    if (!isEucJp(profile) && !isSjis(profile) && !isSbc(profile)) return CES_ERROR_INVALID_PARAMETER;
    if (!mbcs) return CES_ERROR_INVALID_SRC_BUFFER;
    uint32_t read = 0;
    uint32_t written = 0;
    while (read < mbcsMax && mbcs[read] != 0) {
        uint32_t length = 0;
        uint32_t code = 0;
        if (const int result = decodeMbcs(profile, mbcs + read, mbcsMax - read, length, code); result != 0) return result;
        Target encoded[4];
        const uint32_t size = encode(code, encoded);
        if (target) {
            if (written + size > targetMax) return CES_ERROR_DST_BUFFER_END;
            std::memcpy(target + written, encoded, size * sizeof(Target));
        }
        read += length;
        written += size;
    }
    if (mbcsLen) *mbcsLen = read;
    if (targetLen) *targetLen = written;
    return 0;
}

}

extern "C" {

const uint8_t* APS5_VABI sceCesRefersUcsProfileCp1252(void) {
    return CP1252_PROFILE;
}

const uint8_t* APS5_VABI sceCesRefersUcsProfileCp1251(void) {
    return CP1251_PROFILE;
}

const uint8_t* APS5_VABI sceCesRefersUcsProfileCp1250(void) {
    return CP1250_PROFILE;
}

void* APS5_VABI sceCesUcsProfileInitSJis1997Cp932(void* sheet) {
    if (!sheet) return nullptr;
    std::memcpy(sheet, SJIS_TAG, sizeof(SJIS_TAG));
    return sheet;
}

int APS5_VABI sceCesMbcsUcsContextInit(void* context, const void* profile) {
    if (!context) return CES_ERROR_INVALID_PARAMETER;
    std::memcpy(context, &profile, sizeof(profile));
    return 0;
}

void* APS5_VABI sceCesUcsProfileInitEucJpCp51932(void* sheet) {
    if (!sheet) return nullptr;
    std::memcpy(sheet, EUC_JP_TAG, sizeof(EUC_JP_TAG));
    return sheet;
}

int APS5_VABI sceCesUtf8StrToMbcsStr(void* context, const uint8_t* utf8, uint32_t utf8max, uint32_t* utf8Len, uint8_t* mbcs, uint32_t mbcsMax, uint32_t* mbcsLen) {
    if (!mbcs) return CES_ERROR_INVALID_DST_BUFFER;
    return toMbcs(context, utf8, utf8max, utf8Len, mbcs, mbcsMax, mbcsLen, decodeUtf8);
}

int APS5_VABI sceCesUtf8StrGetMbcsLen(void* context, const uint8_t* utf8, uint32_t utf8max, uint32_t* utf8Len, uint32_t* mbcsLen) {
    return toMbcs(context, utf8, utf8max, utf8Len, static_cast<uint8_t*>(nullptr), 0, mbcsLen, decodeUtf8);
}

int APS5_VABI sceCesUcs2StrToMbcsStr(void* context, const uint16_t* ucs2, uint32_t ucs2max, uint32_t* ucs2Len, uint8_t* mbcs, uint32_t mbcsMax, uint32_t* mbcsLen) {
    if (!mbcs) return CES_ERROR_INVALID_DST_BUFFER;
    return toMbcs(context, ucs2, ucs2max, ucs2Len, mbcs, mbcsMax, mbcsLen, decodeUcs2);
}

int APS5_VABI sceCesUcs2StrGetMbcsLen(void* context, const uint16_t* ucs2, uint32_t ucs2max, uint32_t* ucs2Len, uint32_t* mbcsLen) {
    return toMbcs(context, ucs2, ucs2max, ucs2Len, static_cast<uint8_t*>(nullptr), 0, mbcsLen, decodeUcs2);
}

int APS5_VABI sceCesMbcsStrToUtf8Str(void* context, const uint8_t* mbcs, uint32_t mbcsMax, uint32_t* mbcsLen, uint8_t* utf8, uint32_t utf8max, uint32_t* utf8Len) {
    if (!utf8) return CES_ERROR_INVALID_DST_BUFFER;
    return fromMbcs(context, mbcs, mbcsMax, mbcsLen, utf8, utf8max, utf8Len, encodeUtf8);
}

int APS5_VABI sceCesMbcsStrGetUtf8Len(void* context, const uint8_t* mbcs, uint32_t mbcsMax, uint32_t* mbcsLen, uint32_t* utf8Len) {
    return fromMbcs(context, mbcs, mbcsMax, mbcsLen, static_cast<uint8_t*>(nullptr), 0, utf8Len, encodeUtf8);
}

int APS5_VABI sceCesMbcsStrToUcs2Str(void* context, const uint8_t* mbcs, uint32_t mbcsMax, uint32_t* mbcsLen, uint16_t* ucs2, uint32_t ucs2max, uint32_t* ucs2Len) {
    if (!ucs2) return CES_ERROR_INVALID_DST_BUFFER;
    return fromMbcs(context, mbcs, mbcsMax, mbcsLen, ucs2, ucs2max, ucs2Len, encodeUcs2);
}

int APS5_VABI sceCesMbcsStrGetUcs2Len(void* context, const uint8_t* mbcs, uint32_t mbcsMax, uint32_t* mbcsLen, uint32_t* ucs2Len) {
    return fromMbcs(context, mbcs, mbcsMax, mbcsLen, static_cast<uint16_t*>(nullptr), 0, ucs2Len, encodeUcs2);
}

int APS5_VABI sceCesEucJpToUcs2(const void* profile, const uint8_t* euc, uint32_t eucMax, uint32_t* eucSize, uint16_t* ucs2) {
    if (!isEucJp(profile)) return CES_ERROR_INVALID_PARAMETER;
    if (!euc) return CES_ERROR_INVALID_SRC_BUFFER;
    if (eucMax == 0) return CES_ERROR_SRC_BUFFER_END;
    if (!ucs2) return CES_ERROR_INVALID_DST_BUFFER;
    uint32_t length = 0;
    uint32_t code = 0;
    if (const int result = decodeMbcs(profile, euc, eucMax, length, code); result != 0) return result;
    *ucs2 = static_cast<uint16_t>(code);
    if (eucSize) *eucSize = length;
    return 0;
}

int APS5_VABI sceCesSbcToUtf8(const uint8_t* profile, uint8_t sbc, uint8_t* utf8, uint32_t utf8max, uint32_t* utf8_len) {
    if (profile != CP1252_PROFILE && profile != CP1251_PROFILE && profile != CP1250_PROFILE) return CES_ERROR_INVALID_PARAMETER;
    if (!utf8) return CES_ERROR_INVALID_DST_BUFFER;

    uint8_t encoded[3];
    const uint32_t length = encodeUtf8(sbcToUnicode(profile, sbc), encoded);
    if (utf8max < length) return CES_ERROR_DST_BUFFER_END;
    for (uint32_t i = 0; i < length; ++i) utf8[i] = encoded[i];
    if (utf8_len) *utf8_len = length;
    return 0;
}

int APS5_VABI sceCesUtf8ToSbc(const uint8_t* utf8, uint32_t utf8max, uint32_t* utf8_len, const uint8_t* profile, uint8_t* sbc) {
    if (profile != CP1252_PROFILE && profile != CP1251_PROFILE && profile != CP1250_PROFILE) return CES_ERROR_INVALID_PARAMETER;
    if (!utf8) return CES_ERROR_INVALID_SRC_BUFFER;
    if (utf8max == 0) return CES_ERROR_SRC_BUFFER_END;
    if (!sbc) return CES_ERROR_INVALID_DST_BUFFER;

    uint32_t length = 0;
    uint32_t code = 0;
    if (const int result = decodeUtf8(utf8, utf8max, length, code); result != 0) return result;
    if (!unicodeToSbc(profile, code, *sbc)) return CES_ERROR_UNASSIGNED_CODE;
    if (utf8_len) *utf8_len = length;
    return 0;
}

}
