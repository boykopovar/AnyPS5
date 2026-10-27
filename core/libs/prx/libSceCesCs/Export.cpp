#include "prx/libSceCesCs/include/Api.hpp"
#include <cstdint>
#include <cstddef>
#include <algorithm>
#include <bit>
#include <cstring>
#include <iterator>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "JisX0208.hpp"
#include "JisX0212.hpp"
#include "JisX0213.hpp"
#include "Cp936.hpp"
#include "Gb18030.hpp"
#include "Cp949.hpp"
#include "Cp950.hpp"
#include "SingleByte.hpp"

namespace {

constexpr int CES_ERROR_INVALID_PARAMETER = static_cast<int>(0x805C0001);
constexpr int CES_ERROR_INVALID_PROFILE = static_cast<int>(0x805C0004);
constexpr int CES_ERROR_INVALID_SRC_BUFFER = static_cast<int>(0x805C0010);
constexpr int CES_ERROR_SRC_BUFFER_END = static_cast<int>(0x805C0011);
constexpr int CES_ERROR_INVALID_ENCODE = static_cast<int>(0x805C0014);
constexpr int CES_ERROR_ILLEGAL_CODE = static_cast<int>(0x805C0015);
constexpr int CES_ERROR_UNASSIGNED_CODE = static_cast<int>(0x805C0020);
constexpr int CES_ERROR_UNSUPPORTED_CODE = static_cast<int>(0x805C0024);
constexpr int CES_ERROR_INVALID_DST_BUFFER = static_cast<int>(0x805C0030);
constexpr int CES_ERROR_DST_BUFFER_END = static_cast<int>(0x805C0031);

constexpr uint8_t PROFILES[3 + SingleByte::Count][8] = {{0}, {1}, {2}};
constexpr const uint8_t* CP1252_PROFILE = PROFILES[0];
constexpr const uint8_t* CP1251_PROFILE = PROFILES[1];
constexpr const uint8_t* CP1250_PROFILE = PROFILES[2];

constexpr uint16_t CP1252_HIGH[32] = {
    0x20AC, 0x0000, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021, 0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0x0000, 0x017D, 0x0000,
    0x0000, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014, 0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0x0000, 0x017E, 0x0178,
};

constexpr uint16_t CP1251_HIGH[128] = {
    0x0402, 0x0403, 0x201A, 0x0453, 0x201E, 0x2026, 0x2020, 0x2021, 0x20AC, 0x2030, 0x0409, 0x2039, 0x040A, 0x040C, 0x040B, 0x040F,
    0x0452, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014, 0x0000, 0x2122, 0x0459, 0x203A, 0x045A, 0x045C, 0x045B, 0x045F,
    0x00A0, 0x040E, 0x045E, 0x0408, 0x00A4, 0x0490, 0x00A6, 0x00A7, 0x0401, 0x00A9, 0x0404, 0x00AB, 0x00AC, 0x00AD, 0x00AE, 0x0407,
    0x00B0, 0x00B1, 0x0406, 0x0456, 0x0491, 0x00B5, 0x00B6, 0x00B7, 0x0451, 0x2116, 0x0454, 0x00BB, 0x0458, 0x0405, 0x0455, 0x0457,
    0x0410, 0x0411, 0x0412, 0x0413, 0x0414, 0x0415, 0x0416, 0x0417, 0x0418, 0x0419, 0x041A, 0x041B, 0x041C, 0x041D, 0x041E, 0x041F,
    0x0420, 0x0421, 0x0422, 0x0423, 0x0424, 0x0425, 0x0426, 0x0427, 0x0428, 0x0429, 0x042A, 0x042B, 0x042C, 0x042D, 0x042E, 0x042F,
    0x0430, 0x0431, 0x0432, 0x0433, 0x0434, 0x0435, 0x0436, 0x0437, 0x0438, 0x0439, 0x043A, 0x043B, 0x043C, 0x043D, 0x043E, 0x043F,
    0x0440, 0x0441, 0x0442, 0x0443, 0x0444, 0x0445, 0x0446, 0x0447, 0x0448, 0x0449, 0x044A, 0x044B, 0x044C, 0x044D, 0x044E, 0x044F,
};

constexpr uint16_t CP1250_HIGH[128] = {
    0x20AC, 0x0000, 0x201A, 0x0000, 0x201E, 0x2026, 0x2020, 0x2021, 0x0000, 0x2030, 0x0160, 0x2039, 0x015A, 0x0164, 0x017D, 0x0179,
    0x0000, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014, 0x0000, 0x2122, 0x0161, 0x203A, 0x015B, 0x0165, 0x017E, 0x017A,
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

const uint8_t* singleByteProfile(SingleByte::Table table) {
    return PROFILES[3 + table];
}

const uint16_t* sbcHighTable(const void* profile) {
    if (profile == CP1251_PROFILE) return CP1251_HIGH;
    if (profile == CP1250_PROFILE) return CP1250_HIGH;
    for (std::size_t table = 0; table < SingleByte::Count; ++table) {
        if (profile == singleByteProfile(static_cast<SingleByte::Table>(table))) return SingleByte::HIGH[table];
    }
    return nullptr;
}

bool sbcToUnicode(const uint8_t* profile, uint8_t sbc, uint32_t& code) {
    code = sbc;
    if (sbc < 0x80) return true;
    if (const uint16_t* high = sbcHighTable(profile)) code = high[sbc - 0x80];
    else if (sbc < 0xA0) code = CP1252_HIGH[sbc - 0x80];
    return code != 0;
}

bool unicodeToSbc(const uint8_t* profile, uint32_t code, uint8_t& sbc) {
    if (const uint16_t* high = sbcHighTable(profile)) {
        if (code < 0x80) {
            sbc = static_cast<uint8_t>(code);
            return true;
        }
        for (uint32_t i = 0; i < 128; ++i) {
            if (high[i] == code) {
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

int decodeScalarUtf8(const uint8_t* utf8, uint32_t utf8max, uint32_t& length, uint32_t& code) {
    const uint8_t lead = utf8[0];
    length = 0;
    uint32_t minimum = 0;
    if (lead < 0x80) {
        length = 1;
        code = lead;
        return 0;
    }
    if (lead < 0xC0 || lead >= 0xFE) return CES_ERROR_INVALID_ENCODE;
    if (lead < 0xE0) { length = 2; code = lead & 0x1F; minimum = 0x80; }
    else if (lead < 0xF0) { length = 3; code = lead & 0x0F; minimum = 0x800; }
    else if (lead < 0xF8) { length = 4; code = lead & 0x07; minimum = 0x10000; }
    else if (lead < 0xFC) { length = 5; code = lead & 0x03; minimum = 0x200000; }
    else { length = 6; code = lead & 0x01; minimum = 0x4000000; }
    const bool truncated = utf8max < length;
    const uint32_t present = truncated ? utf8max : length;
    for (uint32_t i = 1; i < present; ++i) {
        if ((utf8[i] & 0xC0) != 0x80) {
            length = truncated ? i + 1 : i;
            return CES_ERROR_INVALID_ENCODE;
        }
        code = code << 6 | (utf8[i] & 0x3F);
    }
    if (truncated) return CES_ERROR_SRC_BUFFER_END;
    if (code < minimum || (code >= 0xD800 && code <= 0xDFFF)) return CES_ERROR_ILLEGAL_CODE;
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
    if (code < 0x10000) {
        utf8[0] = static_cast<uint8_t>(0xE0 | code >> 12);
        utf8[1] = static_cast<uint8_t>(0x80 | (code >> 6 & 0x3F));
        utf8[2] = static_cast<uint8_t>(0x80 | (code & 0x3F));
        return 3;
    }
    utf8[0] = static_cast<uint8_t>(0xF0 | code >> 18);
    utf8[1] = static_cast<uint8_t>(0x80 | (code >> 12 & 0x3F));
    utf8[2] = static_cast<uint8_t>(0x80 | (code >> 6 & 0x3F));
    utf8[3] = static_cast<uint8_t>(0x80 | (code & 0x3F));
    return 4;
}

constexpr uint8_t EUC_JP_TAG[8] = {'E', 'U', 'C', 'J', 'P', '9', '3', '2'};
constexpr uint8_t EUC_JP_X0208_TAG[8] = {'E', 'U', 'C', 'J', 'X', '2', '0', '8'};
constexpr uint8_t EUC_JP_X0208_SS2_TAG[8] = {'E', 'U', 'C', 'J', 'X', 'S', 'S', '2'};
constexpr uint8_t EUC_JP_X0208_SS3_TAG[8] = {'E', 'U', 'C', 'J', 'X', 'S', 'S', '3'};
constexpr uint8_t SJIS_TAG[8] = {'S', 'J', 'I', 'S', '1', '9', '9', '7'};
constexpr uint8_t SJIS_X0208_TAG[8] = {'S', 'J', 'I', 'S', 'X', '2', '0', '8'};
constexpr uint8_t EUC_JIS2004_TAG[8] = {'E', 'U', 'C', 'J', 'I', 'S', '0', '4'};
constexpr uint8_t SJIS2004_TAG[8] = {'S', 'J', 'I', 'S', '2', '0', '0', '4'};

bool hasTag(const void* profile, const uint8_t (&tag)[8]) {
    return profile != nullptr && std::memcmp(profile, tag, sizeof(tag)) == 0;
}

bool isEucJpX0208(const void* profile) {
    return hasTag(profile, EUC_JP_X0208_TAG) || hasTag(profile, EUC_JP_X0208_SS2_TAG) || hasTag(profile, EUC_JP_X0208_SS3_TAG);
}

bool isEucJp(const void* profile) {
    return hasTag(profile, EUC_JP_TAG) || isEucJpX0208(profile);
}

bool hasSs2(const void* profile) {
    return !hasTag(profile, EUC_JP_X0208_TAG);
}

bool hasSs3(const void* profile) {
    return hasTag(profile, EUC_JP_X0208_SS3_TAG);
}

bool isSjisX0208(const void* profile) {
    return hasTag(profile, SJIS_X0208_TAG);
}

bool isSjis(const void* profile) {
    return hasTag(profile, SJIS_TAG) || isSjisX0208(profile);
}

bool isEucJis2004(const void* profile) {
    return hasTag(profile, EUC_JIS2004_TAG);
}

bool isSjis2004(const void* profile) {
    return hasTag(profile, SJIS2004_TAG);
}

bool isJis2004(const void* profile) {
    return isEucJis2004(profile) || isSjis2004(profile);
}

struct JisChange {
    uint16_t jis;
    uint16_t code;
};

constexpr JisChange JIS_X0208_CHANGES[7] = {{0x213D, 0x2014}, {0x2141, 0x301C}, {0x2142, 0x2016}, {0x215D, 0x2212}, {0x2171, 0x00A2}, {0x2172, 0x00A3}, {0x224C, 0x00AC}};
constexpr uint16_t JIS_X0208_BACKSLASH_CELL = 0x2140;
constexpr JisChange JIS_X0208_EUC_FALLBACKS[2] = {{0x216F, 0x00A5}, {0x2131, 0x203E}};

enum class JisStrict { None, Sjis, Euc };

constexpr uint32_t jisIndex(uint16_t jis) {
    return ((jis >> 8) - 0x21) * 94 + (jis & 0xFF) - 0x21;
}

bool isCp932ExtensionRow(uint32_t row) {
    return row == 13 || (row >= 89 && row <= 92);
}

uint16_t strictJisX0208(JisStrict strict, uint32_t index) {
    if (isCp932ExtensionRow(index / 94 + 1)) return 0;
    if (index == jisIndex(JIS_X0208_BACKSLASH_CELL)) return strict == JisStrict::Sjis ? 0x005C : 0xFF3C;
    for (const JisChange& change : JIS_X0208_CHANGES) {
        if (jisIndex(change.jis) == index) return change.code;
    }
    return JisX0208::TO_UCS2[index];
}

uint32_t jisX0208ToUnicode(JisStrict strict, uint32_t index) {
    return strict == JisStrict::None ? JisX0208::TO_UCS2[index] : strictJisX0208(strict, index);
}

uint32_t sjisX0208Single(uint8_t byte) {
    return byte == 0x5C ? 0x00A5 : byte == 0x7E ? 0x203E : byte;
}

constexpr uint8_t EUC_CN_TAG[8] = {'E', 'U', 'C', 'C', 'N', '9', '3', '6'};
constexpr uint8_t GBK_TAG[8] = {'G', 'B', 'K', 'C', 'P', '9', '3', '6'};

bool isEucCn(const void* profile) {
    return profile != nullptr && std::memcmp(profile, EUC_CN_TAG, sizeof(EUC_CN_TAG)) == 0;
}

bool isGbk(const void* profile) {
    return profile != nullptr && std::memcmp(profile, GBK_TAG, sizeof(GBK_TAG)) == 0;
}

bool isSbc(const void* profile) {
    return profile == CP1252_PROFILE || sbcHighTable(profile) != nullptr;
}

int rejectLead(uint32_t& length) {
    length = 1;
    return CES_ERROR_INVALID_ENCODE;
}

int needBytes(uint32_t size, uint32_t& length) {
    length = size;
    return CES_ERROR_SRC_BUFFER_END;
}

int rejectTrail(uint8_t trail, uint32_t size, uint32_t& length) {
    length = trail == 0 ? 1 : size;
    return CES_ERROR_INVALID_ENCODE;
}

bool stopsAtNul(const uint8_t* mbcs, uint32_t mbcsMax, uint32_t from, uint32_t size, uint32_t& length) {
    for (uint32_t index = from; index < size && index < mbcsMax; ++index) {
        if (mbcs[index] == 0) {
            length = index;
            return true;
        }
    }
    return false;
}

int unassigned(uint32_t size, uint32_t& length) {
    length = size;
    return CES_ERROR_UNASSIGNED_CODE;
}

int decodeGb(const void* profile, const uint8_t* mbcs, uint32_t mbcsMax, uint32_t& length, uint32_t& code) {
    const uint8_t lead = mbcs[0];
    if (lead < 0x80) {
        length = 1;
        code = lead;
        return 0;
    }
    const bool gbk = isGbk(profile);
    if (gbk && lead == 0x80) {
        length = 1;
        code = 0x20AC;
        return 0;
    }
    if (lead == 0xFF || (!gbk && lead < 0xA1)) return rejectLead(length);
    if (mbcsMax < 2) return needBytes(2, length);
    const uint8_t trail = mbcs[1];
    if (trail == 0xFF || (gbk ? trail < 0x40 || trail == 0x7F : trail < 0xA1)) return rejectTrail(trail, 2, length);
    code = Cp936::TO_UCS2[(lead - 0x81) * 190 + (trail < 0x7F ? trail - 0x40 : trail - 0x41)];
    if (code == 0) return unassigned(2, length);
    length = 2;
    return 0;
}

const std::vector<std::pair<uint16_t, uint16_t>>& cp936Reverse() {
    static const auto reverse = [] {
        std::vector<std::pair<uint16_t, uint16_t>> entries;
        for (uint32_t cell = 0; cell < 126 * 190; ++cell) {
            if (Cp936::TO_UCS2[cell] != 0) entries.emplace_back(Cp936::TO_UCS2[cell], static_cast<uint16_t>(cell));
        }
        std::sort(entries.begin(), entries.end());
        return entries;
    }();
    return reverse;
}

int encodeGb(const void* profile, uint32_t code, uint8_t* mbcs, uint32_t& length) {
    if (code < 0x80) {
        length = 1;
        mbcs[0] = static_cast<uint8_t>(code);
        return 0;
    }
    const bool gbk = isGbk(profile);
    if (gbk && code == 0x20AC) {
        length = 1;
        mbcs[0] = 0x80;
        return 0;
    }
    if (code > 0xFFFF) return CES_ERROR_UNASSIGNED_CODE;
    const auto& reverse = cp936Reverse();
    for (auto found = std::lower_bound(reverse.begin(), reverse.end(), std::pair<uint16_t, uint16_t>(static_cast<uint16_t>(code), 0));
         found != reverse.end() && found->first == code; ++found) {
        const uint8_t lead = static_cast<uint8_t>(0x81 + found->second / 190);
        const uint32_t cell = found->second % 190;
        const uint8_t trail = static_cast<uint8_t>(cell < 0x3F ? 0x40 + cell : 0x41 + cell);
        if (!gbk && (lead < 0xA1 || trail < 0xA1)) continue;
        mbcs[0] = lead;
        mbcs[1] = trail;
        length = 2;
        return 0;
    }
    return CES_ERROR_UNASSIGNED_CODE;
}

constexpr uint8_t GB18030_2000_TAG[8] = {'G', 'B', '1', '8', '2', '0', '0', '0'};
constexpr uint8_t GB18030_2005_TAG[8] = {'G', 'B', '1', '8', '2', '0', '0', '5'};
constexpr uint32_t GB_CELLS = 126 * 190;

struct Gb18030Tables {
    std::span<const uint32_t[3]> twoByte;
    std::span<const uint32_t[3]> fourByte;
    bool edition2005;
};

constexpr Gb18030Tables GB18030_2000{Gb18030::TWO_BYTE_2000, Gb18030::FOUR_BYTE_2000, false};
constexpr Gb18030Tables GB18030_2005{Gb18030::TWO_BYTE_2000, Gb18030::FOUR_BYTE_2000, true};
constexpr uint32_t GB18030_SWAPPED_CELL = (0xA8 - 0x81) * 190 + (0xBC - 0x41);
constexpr uint32_t GB18030_SWAPPED_LINEAR = (5 * 126 + (0xF4 - 0x81)) * 10 + 7;

bool mapsGb18030Supplementary(const Gb18030Tables& tables, uint32_t code) {
    return tables.edition2005 && code >= 0x20000 && code <= 0x2FFFF;
}

const Gb18030Tables* gb18030Tables(const void* profile) {
    if (profile == nullptr) return nullptr;
    if (std::memcmp(profile, GB18030_2000_TAG, sizeof(GB18030_2000_TAG)) == 0) return &GB18030_2000;
    if (std::memcmp(profile, GB18030_2005_TAG, sizeof(GB18030_2005_TAG)) == 0) return &GB18030_2005;
    return nullptr;
}

bool isGb18030(const void* profile) {
    return gb18030Tables(profile) != nullptr;
}

bool isGb(const void* profile) {
    return isGbk(profile) || isGb18030(profile);
}

const uint32_t* findRun(std::span<const uint32_t[3]> runs, uint32_t key) {
    const auto next = std::upper_bound(runs.begin(), runs.end(), key, [](uint32_t value, const uint32_t (&run)[3]) { return value < run[0]; });
    if (next == runs.begin()) return nullptr;
    const uint32_t* run = *std::prev(next);
    return key - run[0] < run[2] ? run : nullptr;
}

uint32_t gb18030Cell(const Gb18030Tables& tables, uint32_t cell) {
    if (tables.edition2005 && cell == GB18030_SWAPPED_CELL) return 0x1E3F;
    const uint32_t* run = findRun(tables.twoByte, cell);
    return run ? run[1] + cell - run[0] : Cp936::TO_UCS2[cell];
}

void putGbLinear(uint32_t linear, uint8_t* mbcs) {
    mbcs[3] = static_cast<uint8_t>(0x30 + linear % 10);
    linear /= 10;
    mbcs[2] = static_cast<uint8_t>(0x81 + linear % 126);
    linear /= 126;
    mbcs[1] = static_cast<uint8_t>(0x30 + linear % 10);
    mbcs[0] = static_cast<uint8_t>(0x81 + linear / 10);
}

void putGbCell(uint32_t cell, uint8_t* mbcs) {
    const uint32_t column = cell % 190;
    mbcs[0] = static_cast<uint8_t>(0x81 + cell / 190);
    mbcs[1] = static_cast<uint8_t>(column < 0x3F ? 0x40 + column : 0x41 + column);
}

int decodeGb18030(const Gb18030Tables& tables, const uint8_t* mbcs, uint32_t mbcsMax, uint32_t& length, uint32_t& code) {
    const uint8_t lead = mbcs[0];
    if (lead < 0x80) {
        length = 1;
        code = lead;
        return 0;
    }
    if (lead == 0x80 || lead == 0xFF) return rejectLead(length);
    if (mbcsMax < 2) return needBytes(2, length);
    const uint8_t second = mbcs[1];
    if (second >= 0x30 && second <= 0x39) {
        if (stopsAtNul(mbcs, mbcsMax, 2, 4, length)) return CES_ERROR_INVALID_ENCODE;
        if (mbcsMax < 4) return needBytes(4, length);
        if (mbcs[2] < 0x81 || mbcs[2] == 0xFF || mbcs[3] < 0x30 || mbcs[3] > 0x39) return rejectTrail(mbcs[2], 4, length);
        const uint32_t linear = (((lead - 0x81) * 10 + (second - 0x30)) * 126 + (mbcs[2] - 0x81)) * 10 + (mbcs[3] - 0x30);
        const uint32_t* run = findRun(tables.fourByte, linear);
        if (!run) return unassigned(4, length);
        code = tables.edition2005 && linear == GB18030_SWAPPED_LINEAR ? 0xE7C7 : run[1] + linear - run[0];
        if (code > 0xFFFF && !mapsGb18030Supplementary(tables, code)) return unassigned(4, length);
        length = 4;
        return 0;
    }
    if (second < 0x40 || second == 0x7F || second == 0xFF) return rejectTrail(second, 2, length);
    length = 2;
    code = gb18030Cell(tables, (lead - 0x81) * 190 + (second < 0x7F ? second - 0x40 : second - 0x41));
    return 0;
}

const std::vector<std::pair<uint32_t, uint16_t>>& gb18030Reverse(const Gb18030Tables& tables) {
    const auto build = [](const Gb18030Tables& source) {
        std::vector<std::pair<uint32_t, uint16_t>> entries;
        entries.reserve(GB_CELLS);
        for (uint32_t cell = 0; cell < GB_CELLS; ++cell) entries.emplace_back(gb18030Cell(source, cell), static_cast<uint16_t>(cell));
        std::sort(entries.begin(), entries.end());
        return entries;
    };
    static const auto reverse2000 = build(GB18030_2000);
    static const auto reverse2005 = build(GB18030_2005);
    return &tables == &GB18030_2005 ? reverse2005 : reverse2000;
}

int encodeGb18030(const Gb18030Tables& tables, uint32_t code, uint8_t* mbcs, uint32_t& length) {
    if (code < 0x80) {
        length = 1;
        mbcs[0] = static_cast<uint8_t>(code);
        return 0;
    }
    const auto& reverse = gb18030Reverse(tables);
    const auto found = std::lower_bound(reverse.begin(), reverse.end(), std::pair<uint32_t, uint16_t>(code, 0));
    if (found != reverse.end() && found->first == code) {
        putGbCell(found->second, mbcs);
        length = 2;
        return 0;
    }
    if (code > 0xFFFF && !mapsGb18030Supplementary(tables, code)) return CES_ERROR_UNASSIGNED_CODE;
    if (tables.edition2005 && code == 0xE7C7) {
        putGbLinear(GB18030_SWAPPED_LINEAR, mbcs);
        length = 4;
        return 0;
    }
    for (const auto& run : tables.fourByte) {
        if (code - run[1] >= run[2]) continue;
        putGbLinear(run[0] + code - run[1], mbcs);
        length = 4;
        return 0;
    }
    return CES_ERROR_UNASSIGNED_CODE;
}

bool sjisToJis(uint8_t lead, uint8_t trail, int& row, int& cell) {
    if (!((lead >= 0x81 && lead <= 0x9F) || (lead >= 0xE0 && lead <= 0xFC))) return false;
    if (trail < 0x40 || trail == 0x7F || trail > 0xFC) return false;
    row = lead <= 0x9F ? (lead - 0x81) * 2 + 1 : (lead - 0xE0) * 2 + 63;
    if (trail >= 0x9F) {
        row += 1;
        cell = trail - 0x9E;
    } else {
        cell = trail > 0x7E ? trail - 0x40 : trail - 0x3F;
    }
    return true;
}

void jisToSjis(int row, int cell, uint8_t& lead, uint8_t& trail) {
    if (row % 2 != 0) {
        lead = static_cast<uint8_t>(row < 63 ? 0x81 + (row - 1) / 2 : 0xE0 + (row - 63) / 2);
        trail = static_cast<uint8_t>(cell <= 63 ? cell + 0x3F : cell + 0x40);
    } else {
        lead = static_cast<uint8_t>(row < 63 ? 0x81 + (row - 2) / 2 : 0xE0 + (row - 64) / 2);
        trail = static_cast<uint8_t>(cell + 0x9E);
    }
}

constexpr uint8_t JIS_X0213_PLANE2_ROWS[26] = {1, 8, 3, 4, 5, 12, 13, 14, 15, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94};

bool isSjisSingle(uint8_t byte) {
    return byte < 0x80 || (byte >= 0xA1 && byte <= 0xDF);
}

const void* contextProfile(const void* context) {
    const void* profile = nullptr;
    std::memcpy(&profile, context, sizeof(profile));
    return profile;
}

constexpr uint8_t EUC_KR_TAG[8] = {'E', 'U', 'C', 'K', 'R', '9', '4', '9'};
constexpr uint8_t UHC_TAG[8] = {'U', 'H', 'C', 'C', 'P', '9', '4', '9'};
constexpr uint8_t BIG5_TAG[8] = {'B', 'I', 'G', '5', 'C', '9', '5', '0'};
constexpr uint16_t BIG5_LAST_CELL[2] = {0x5341, 0x5345};

bool isEucKr(const void* profile) {
    return profile != nullptr && std::memcmp(profile, EUC_KR_TAG, sizeof(EUC_KR_TAG)) == 0;
}

bool isUhc(const void* profile) {
    return profile != nullptr && std::memcmp(profile, UHC_TAG, sizeof(UHC_TAG)) == 0;
}

bool isBig5(const void* profile) {
    return profile != nullptr && std::memcmp(profile, BIG5_TAG, sizeof(BIG5_TAG)) == 0;
}

bool isMbcs(const void* profile) {
    return isEucJp(profile) || isSjis(profile) || isJis2004(profile) || isSbc(profile) || isEucCn(profile) || isGb(profile) || isEucKr(profile) || isUhc(profile) || isBig5(profile);
}

int decodeCp949(bool unified, const uint8_t* mbcs, uint32_t mbcsMax, uint32_t& length, uint32_t& code) {
    const uint8_t lead = mbcs[0];
    if (lead < 0x80) {
        length = 1;
        code = lead;
        return 0;
    }
    if (lead < (unified ? 0x81 : 0xA1) || lead == 0xFF) return rejectLead(length);
    if (mbcsMax < 2) return needBytes(2, length);
    const uint8_t trail = mbcs[1];
    const bool extended = unified && ((trail >= 0x41 && trail <= 0x5A) || (trail >= 0x61 && trail <= 0x7A) || (trail >= 0x81 && trail <= 0xA0));
    if (!extended && (trail < 0xA1 || trail == 0xFF)) return rejectTrail(trail, 2, length);
    length = 2;
    code = Cp949::TO_UCS2[(lead - 0x81) * 190 + (trail - 0x41)];
    return code != 0 ? 0 : CES_ERROR_UNASSIGNED_CODE;
}

int decodeCp950(const uint8_t* mbcs, uint32_t mbcsMax, uint32_t& length, uint32_t& code) {
    const uint8_t lead = mbcs[0];
    if (lead < 0x80) {
        length = 1;
        code = lead;
        return 0;
    }
    if (lead == 0x80 || lead == 0xFF) return rejectLead(length);
    if (mbcsMax < 2) return needBytes(2, length);
    const uint8_t trail = mbcs[1];
    if (trail < 0x40 || (trail > 0x7E && trail < 0xA1) || trail == 0xFF) return rejectTrail(trail, 2, length);
    if (lead < 0xA1 || lead > 0xF9) return unassigned(2, length);
    length = 2;
    code = Cp950::TO_UCS2[(lead - 0xA1) * 157 + (trail < 0xA1 ? trail - 0x40 : trail - 0xA1 + 63)];
    return code != 0 ? 0 : CES_ERROR_UNASSIGNED_CODE;
}

struct DbcsCell {
    uint16_t code;
    uint16_t bytes;
};

bool byCode(const DbcsCell& left, const DbcsCell& right) {
    return left.code < right.code;
}

const std::vector<DbcsCell>& cp949Reverse() {
    static const auto reverse = [] {
        std::vector<DbcsCell> entries;
        for (uint32_t cell = 0; cell < 126 * 190; ++cell) {
            if (Cp949::TO_UCS2[cell] != 0) entries.push_back({Cp949::TO_UCS2[cell], static_cast<uint16_t>((0x81 + cell / 190) << 8 | (0x41 + cell % 190))});
        }
        std::sort(entries.begin(), entries.end(), byCode);
        return entries;
    }();
    return reverse;
}

const std::vector<DbcsCell>& cp950Reverse() {
    static const auto reverse = [] {
        std::vector<DbcsCell> entries;
        for (uint32_t cell = 0; cell < 89 * 157; ++cell) {
            const uint32_t column = cell % 157;
            const uint32_t trail = column < 63 ? 0x40 + column : 0xA1 + column - 63;
            if (Cp950::TO_UCS2[cell] != 0) entries.push_back({Cp950::TO_UCS2[cell], static_cast<uint16_t>((0xA1 + cell / 157) << 8 | trail)});
        }
        std::stable_sort(entries.begin(), entries.end(), byCode);
        std::vector<DbcsCell> unique;
        for (const DbcsCell& entry : entries) {
            if (unique.empty() || unique.back().code != entry.code) unique.push_back(entry);
            else if (std::find(std::begin(BIG5_LAST_CELL), std::end(BIG5_LAST_CELL), entry.code) != std::end(BIG5_LAST_CELL)) unique.back() = entry;
        }
        return unique;
    }();
    return reverse;
}

int encodeDbcs(const std::vector<DbcsCell>& reverse, bool euc, uint32_t code, uint8_t* mbcs, uint32_t& length) {
    if (code < 0x80) {
        length = 1;
        mbcs[0] = static_cast<uint8_t>(code);
        return 0;
    }
    const auto found = std::lower_bound(reverse.begin(), reverse.end(), code, [](const DbcsCell& entry, uint32_t value) { return entry.code < value; });
    if (found == reverse.end() || found->code != code) return CES_ERROR_UNASSIGNED_CODE;
    if (euc && ((found->bytes >> 8) < 0xA1 || (found->bytes & 0xFF) < 0xA1)) return CES_ERROR_UNASSIGNED_CODE;
    length = 2;
    mbcs[0] = static_cast<uint8_t>(found->bytes >> 8);
    mbcs[1] = static_cast<uint8_t>(found->bytes & 0xFF);
    return 0;
}

bool jisX0212RowEmpty(uint8_t row) {
    for (uint32_t cell = 0; cell < 94; ++cell) {
        if (JisX0212::TO_UCS2[(row - 0xA1) * 94 + cell] != 0) return false;
    }
    return true;
}

int decodeEucJpScalar(const uint8_t* euc, uint32_t eucMax, uint32_t& length, uint32_t& code) {
    const uint8_t lead = euc[0];
    length = 1;
    if (lead == 0x0E || lead == 0xA0 || lead == 0xFF) return CES_ERROR_INVALID_ENCODE;
    if (lead < 0xA0 && lead != 0x8E && lead != 0x8F) {
        code = lead;
        return 0;
    }
    const uint32_t needed = lead == 0x8F ? 3 : 2;
    for (uint32_t i = 1; i < needed && i < eucMax; ++i) {
        if (euc[i] == 0) {
            length = i;
            return CES_ERROR_INVALID_ENCODE;
        }
    }
    length = needed;
    if (eucMax < needed) return CES_ERROR_SRC_BUFFER_END;
    const uint8_t trail = euc[1];
    if (lead == 0x8E) {
        if (trail >= 0xA1 && trail <= 0xDF) {
            code = 0xFF61 + (trail - 0xA1);
            return 0;
        }
        return trail >= 0xE0 && trail <= 0xFE ? CES_ERROR_UNASSIGNED_CODE : CES_ERROR_INVALID_ENCODE;
    }
    if (lead == 0x8F) {
        if (trail < 0xA1 || trail > 0xFE || euc[2] < 0xA1 || euc[2] > 0xFE) return CES_ERROR_INVALID_ENCODE;
        return jisX0212RowEmpty(trail) ? CES_ERROR_UNASSIGNED_CODE : CES_ERROR_INVALID_ENCODE;
    }
    if (trail < 0xA1 || trail > 0xFE) return CES_ERROR_INVALID_ENCODE;
    code = JisX0208::TO_UCS2[(lead - 0xA1) * 94 + (trail - 0xA1)];
    return code != 0 ? 0 : CES_ERROR_UNASSIGNED_CODE;
}

int decodeEucJp(const void* profile, const uint8_t* mbcs, uint32_t mbcsMax, uint32_t& length, uint32_t& code) {
    if (hasTag(profile, EUC_JP_TAG)) return decodeEucJpScalar(mbcs, mbcsMax, length, code);
    const uint8_t lead = mbcs[0];
    if (lead < 0x80) {
        length = 1;
        code = lead;
        return 0;
    }
    if (lead == 0x8E && hasSs2(profile)) {
        if (mbcsMax < 2) return needBytes(2, length);
        if (mbcs[1] < 0xA1 || mbcs[1] > 0xDF) return rejectTrail(mbcs[1], 2, length);
        length = 2;
        code = 0xFF61 + (mbcs[1] - 0xA1);
        return 0;
    }
    if (lead >= 0xA1 && lead <= 0xFE) {
        if (mbcsMax < 2) return needBytes(2, length);
        if (mbcs[1] < 0xA1 || mbcs[1] > 0xFE) return rejectTrail(mbcs[1], 2, length);
        length = 2;
        code = jisX0208ToUnicode(isEucJpX0208(profile) ? JisStrict::Euc : JisStrict::None, (lead - 0xA1) * 94 + (mbcs[1] - 0xA1));
        return code != 0 ? 0 : CES_ERROR_UNASSIGNED_CODE;
    }
    if (lead == 0x8F && hasSs3(profile)) {
        if (stopsAtNul(mbcs, mbcsMax, 1, 3, length)) return CES_ERROR_INVALID_ENCODE;
        if (mbcsMax < 3) return needBytes(3, length);
        if (mbcs[1] < 0xA1 || mbcs[1] > 0xFE || mbcs[2] < 0xA1 || mbcs[2] > 0xFE) return rejectTrail(mbcs[1], 3, length);
        length = 3;
        code = JisX0212::TO_UCS2[(mbcs[1] - 0xA1) * 94 + (mbcs[2] - 0xA1)];
        return code != 0 ? 0 : CES_ERROR_UNASSIGNED_CODE;
    }
    return rejectLead(length);
}

constexpr JisChange JIS_X0213_FALLBACKS[2] = {{0x216F, 0x00A5}, {0x2131, 0x203E}};
constexpr uint8_t JIS_X0212_PLANE = 3;

bool isJisX0213Plane2Row(uint32_t row) {
    return std::find(std::begin(JIS_X0213_PLANE2_ROWS), std::end(JIS_X0213_PLANE2_ROWS), static_cast<uint8_t>(row)) != std::end(JIS_X0213_PLANE2_ROWS);
}

struct Jis2004Cell {
    uint32_t code;
    uint16_t cell;
    uint8_t plane;
};

const std::vector<Jis2004Cell>& jis2004Reverse() {
    static const std::vector<Jis2004Cell> reverse = [] {
        std::vector<Jis2004Cell> entries;
        for (uint8_t plane = 1; plane <= 2; ++plane) {
            const uint32_t* table = plane == 1 ? JisX0213::PLANE1 : JisX0213::PLANE2;
            for (uint32_t cell = 0; cell < 94 * 94; ++cell) {
                if (table[cell] != 0) entries.push_back({table[cell], static_cast<uint16_t>(cell), plane});
            }
        }
        for (const JisChange& fallback : JIS_X0213_FALLBACKS) entries.push_back({fallback.code, static_cast<uint16_t>(jisIndex(fallback.jis)), 1});
        for (uint32_t cell = 0; cell < 94 * 94; ++cell) {
            if (JisX0212::TO_UCS2[cell] != 0 && !isJisX0213Plane2Row(cell / 94 + 1)) entries.push_back({JisX0212::TO_UCS2[cell], static_cast<uint16_t>(cell), JIS_X0212_PLANE});
        }
        std::stable_sort(entries.begin(), entries.end(), [](const Jis2004Cell& a, const Jis2004Cell& b) { return a.code < b.code; });
        return entries;
    }();
    return reverse;
}

int decodeJis2004(bool sjis, const uint8_t* mbcs, uint32_t mbcsMax, uint32_t& length, uint32_t& code) {
    const uint8_t lead = mbcs[0];
    if (lead < 0x80) {
        length = 1;
        code = lead;
        return 0;
    }
    if (sjis) {
        if (lead >= 0xA1 && lead <= 0xDF) {
            length = 1;
            code = 0xFF61 + (lead - 0xA1);
            return 0;
        }
        if (!((lead >= 0x81 && lead <= 0x9F) || (lead >= 0xE0 && lead <= 0xFC))) return CES_ERROR_INVALID_ENCODE;
        if (mbcsMax < 2) return CES_ERROR_SRC_BUFFER_END;
        int row = 0;
        int cell = 0;
        if (!sjisToJis(lead, mbcs[1], row, cell)) return CES_ERROR_INVALID_ENCODE;
        length = 2;
        if (row <= 94) code = JisX0213::PLANE1[(row - 1) * 94 + (cell - 1)];
        else code = JisX0213::PLANE2[(JIS_X0213_PLANE2_ROWS[row - 95] - 1) * 94 + (cell - 1)];
        return code != 0 ? 0 : CES_ERROR_UNASSIGNED_CODE;
    }
    if (lead == 0x8E) {
        if (mbcsMax < 2) return CES_ERROR_SRC_BUFFER_END;
        if (mbcs[1] < 0xA1 || mbcs[1] > 0xDF) return CES_ERROR_INVALID_ENCODE;
        length = 2;
        code = 0xFF61 + (mbcs[1] - 0xA1);
        return 0;
    }
    if (lead == 0x8F) {
        if (mbcsMax < 3) return CES_ERROR_SRC_BUFFER_END;
        if (mbcs[1] < 0xA1 || mbcs[1] > 0xFE || mbcs[2] < 0xA1 || mbcs[2] > 0xFE) return CES_ERROR_INVALID_ENCODE;
        length = 3;
        const uint32_t cell = (mbcs[1] - 0xA1) * 94 + (mbcs[2] - 0xA1);
        code = isJisX0213Plane2Row(mbcs[1] - 0xA0) ? JisX0213::PLANE2[cell] : JisX0212::TO_UCS2[cell];
        return code != 0 ? 0 : CES_ERROR_UNASSIGNED_CODE;
    }
    if (lead < 0xA1 || lead > 0xFE) return CES_ERROR_INVALID_ENCODE;
    if (mbcsMax < 2) return CES_ERROR_SRC_BUFFER_END;
    if (mbcs[1] < 0xA1 || mbcs[1] > 0xFE) return CES_ERROR_INVALID_ENCODE;
    length = 2;
    code = JisX0213::PLANE1[(lead - 0xA1) * 94 + (mbcs[1] - 0xA1)];
    return code != 0 ? 0 : CES_ERROR_UNASSIGNED_CODE;
}

int encodeJis2004(bool sjis, uint32_t code, uint8_t* mbcs, uint32_t& length) {
    if (code < 0x80) {
        length = 1;
        mbcs[0] = static_cast<uint8_t>(code);
        return 0;
    }
    if (code >= 0xFF61 && code <= 0xFF9F) {
        const auto byte = static_cast<uint8_t>(0xA1 + (code - 0xFF61));
        if (sjis) {
            length = 1;
            mbcs[0] = byte;
        } else {
            length = 2;
            mbcs[0] = 0x8E;
            mbcs[1] = byte;
        }
        return 0;
    }
    const auto& reverse = jis2004Reverse();
    const auto found = std::lower_bound(reverse.begin(), reverse.end(), code, [](const Jis2004Cell& entry, uint32_t value) { return entry.code < value; });
    if (found == reverse.end() || found->code != code) return CES_ERROR_UNASSIGNED_CODE;
    const int row = found->cell / 94 + 1;
    const int cell = found->cell % 94 + 1;
    if (sjis) {
        if (found->plane == JIS_X0212_PLANE) return CES_ERROR_UNASSIGNED_CODE;
        int sjisRow = row;
        if (found->plane == 2) {
            const auto* position = std::find(std::begin(JIS_X0213_PLANE2_ROWS), std::end(JIS_X0213_PLANE2_ROWS), static_cast<uint8_t>(row));
            if (position == std::end(JIS_X0213_PLANE2_ROWS)) return CES_ERROR_UNASSIGNED_CODE;
            sjisRow = 95 + static_cast<int>(position - std::begin(JIS_X0213_PLANE2_ROWS));
        }
        jisToSjis(sjisRow, cell, mbcs[0], mbcs[1]);
        length = 2;
        return 0;
    }
    if (found->plane == 2 || found->plane == JIS_X0212_PLANE) {
        mbcs[0] = 0x8F;
        mbcs[1] = static_cast<uint8_t>(0xA0 + row);
        mbcs[2] = static_cast<uint8_t>(0xA0 + cell);
        length = 3;
        return 0;
    }
    mbcs[0] = static_cast<uint8_t>(0xA0 + row);
    mbcs[1] = static_cast<uint8_t>(0xA0 + cell);
    length = 2;
    return 0;
}

int decodeMbcs(const void* profile, const uint8_t* mbcs, uint32_t mbcsMax, uint32_t& length, uint32_t& code) {
    if (isJis2004(profile)) return decodeJis2004(isSjis2004(profile), mbcs, mbcsMax, length, code);
    if (isEucKr(profile) || isUhc(profile)) return decodeCp949(isUhc(profile), mbcs, mbcsMax, length, code);
    if (isBig5(profile)) return decodeCp950(mbcs, mbcsMax, length, code);
    if (const Gb18030Tables* tables = gb18030Tables(profile)) return decodeGb18030(*tables, mbcs, mbcsMax, length, code);
    if (isEucCn(profile) || isGbk(profile)) return decodeGb(profile, mbcs, mbcsMax, length, code);
    const uint8_t lead = mbcs[0];
    if (isSbc(profile)) {
        length = 1;
        return sbcToUnicode(static_cast<const uint8_t*>(profile), lead, code) ? 0 : CES_ERROR_UNASSIGNED_CODE;
    }
    if (isSjis(profile)) {
        const bool strict = isSjisX0208(profile);
        if (lead < 0x80) {
            length = 1;
            code = strict ? sjisX0208Single(lead) : lead;
            return 0;
        }
        if (lead >= 0xA1 && lead <= 0xDF) {
            length = 1;
            code = 0xFF61 + (lead - 0xA1);
            return 0;
        }
        if ((lead >= 0x81 && lead <= 0x9F) || (lead >= 0xE0 && lead <= 0xFC)) {
            if (mbcsMax < 2) return needBytes(2, length);
            const uint8_t trail = mbcs[1];
            if (trail < 0x40 || trail == 0x7F || trail > 0xFC) return rejectTrail(trail, 2, length);
            if (lead >= 0xFA && !strict) {
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
                    if (code == 0) return unassigned(2, length);
                    length = 2;
                    return 0;
                }
                return unassigned(2, length);
            }
            int row = 0;
            int cell = 0;
            sjisToJis(lead, trail, row, cell);
            if (row < 1 || row > 94 || cell < 1 || cell > 94) return unassigned(2, length);
            code = jisX0208ToUnicode(strict ? JisStrict::Sjis : JisStrict::None, (row - 1) * 94 + (cell - 1));
            if (code == 0) return unassigned(2, length);
            length = 2;
            return 0;
        }
        return rejectLead(length);
    }
    return decodeEucJp(profile, mbcs, mbcsMax, length, code);
}

struct JisCell {
    uint16_t code;
    uint16_t cell;
    bool supplementary;
};

std::vector<JisCell> buildJisReverse(JisStrict strict) {
    std::vector<JisCell> entries;
    entries.reserve(2 * 94 * 94);
    for (uint32_t cell = 0; cell < 94 * 94; ++cell) {
        const uint16_t code = static_cast<uint16_t>(jisX0208ToUnicode(strict, cell));
        if (code != 0) entries.push_back({code, static_cast<uint16_t>(cell), false});
    }
    if (strict == JisStrict::Euc) {
        for (const JisChange& fallback : JIS_X0208_EUC_FALLBACKS) entries.push_back({fallback.code, static_cast<uint16_t>(jisIndex(fallback.jis)), false});
    }
    for (uint32_t cell = 0; cell < 94 * 94; ++cell) {
        if (JisX0212::TO_UCS2[cell] != 0) entries.push_back({JisX0212::TO_UCS2[cell], static_cast<uint16_t>(cell), true});
    }
    std::stable_sort(entries.begin(), entries.end(), [](const JisCell& left, const JisCell& right) { return left.code < right.code; });
    entries.erase(std::unique(entries.begin(), entries.end(), [](const JisCell& left, const JisCell& right) { return left.code == right.code; }), entries.end());
    return entries;
}

const std::vector<JisCell>& jisReverse(JisStrict strict) {
    static const auto reverse = buildJisReverse(JisStrict::None);
    static const auto sjisReverse = buildJisReverse(JisStrict::Sjis);
    static const auto eucReverse = buildJisReverse(JisStrict::Euc);
    return strict == JisStrict::Sjis ? sjisReverse : strict == JisStrict::Euc ? eucReverse : reverse;
}

uint32_t jisAlias(uint32_t code) {
    switch (code) {
    case 0x00A2: return 0xFFE0;
    case 0x00A3: return 0xFFE1;
    case 0x00A5: return 0xFFE5;
    case 0x00AC: return 0xFFE2;
    case 0x2014: return 0x2015;
    case 0x2016: return 0x2225;
    case 0x203E: return 0xFFE3;
    case 0x2212: return 0xFF0D;
    case 0x301C: return 0xFF5E;
    default: return code;
    }
}

int encodeMbcs(const void* profile, uint32_t code, uint8_t* mbcs, uint32_t& length) {
    if (isJis2004(profile)) return encodeJis2004(isSjis2004(profile), code, mbcs, length);
    if (isEucKr(profile) || isUhc(profile)) return encodeDbcs(cp949Reverse(), isEucKr(profile), code, mbcs, length);
    if (isBig5(profile)) return encodeDbcs(cp950Reverse(), false, code, mbcs, length);
    if (const Gb18030Tables* tables = gb18030Tables(profile)) return encodeGb18030(*tables, code, mbcs, length);
    if (isEucCn(profile) || isGbk(profile)) return encodeGb(profile, code, mbcs, length);
    if (isSbc(profile)) {
        length = 1;
        return unicodeToSbc(static_cast<const uint8_t*>(profile), code, mbcs[0]) ? 0 : CES_ERROR_UNASSIGNED_CODE;
    }
    if (isSjis(profile)) {
        const bool strict = isSjisX0208(profile);
        if (strict && (code == 0x00A5 || code == 0x203E)) {
            length = 1;
            mbcs[0] = code == 0x00A5 ? 0x5C : 0x7E;
            return 0;
        }
        if (code < 0x80 && !(strict && (code == 0x5C || code == 0x7E))) {
            length = 1;
            mbcs[0] = static_cast<uint8_t>(code);
            return 0;
        }
        if (!strict && code <= 0x9F) return CES_ERROR_UNSUPPORTED_CODE;
        if (!strict && code == 0xFEFF) {
            length = 0;
            return 0;
        }
        if (!strict) code = jisAlias(code);
        if (code >= 0xFF61 && code <= 0xFF9F) {
            length = 1;
            mbcs[0] = static_cast<uint8_t>(0xA1 + (code - 0xFF61));
            return 0;
        }
        if (code > 0xFFFF) return CES_ERROR_UNASSIGNED_CODE;
        const auto& reverse = jisReverse(strict ? JisStrict::Sjis : JisStrict::None);
        const auto found = std::lower_bound(reverse.begin(), reverse.end(), static_cast<uint16_t>(code), [](const JisCell& entry, uint16_t value) { return entry.code < value; });
        if (found == reverse.end() || found->code != code || found->supplementary) return CES_ERROR_UNASSIGNED_CODE;
        jisToSjis(found->cell / 94 + 1, found->cell % 94 + 1, mbcs[0], mbcs[1]);
        length = 2;
        return 0;
    }
    const bool strict = isEucJpX0208(profile);
    if (!strict && code == 0x0E) return CES_ERROR_UNASSIGNED_CODE;
    if (code < 0x80 || (!strict && code < 0xA0 && code != 0x8E && code != 0x8F)) {
        length = 1;
        mbcs[0] = static_cast<uint8_t>(code);
        return 0;
    }
    if (!strict && code == 0xFEFF) {
        length = 0;
        return 0;
    }
    if (!strict) code = jisAlias(code);
    if (code >= 0xFF61 && code <= 0xFF9F) {
        if (!hasSs2(profile)) return CES_ERROR_UNASSIGNED_CODE;
        length = 2;
        mbcs[0] = 0x8E;
        mbcs[1] = static_cast<uint8_t>(0xA1 + (code - 0xFF61));
        return 0;
    }
    if (code > 0xFFFF) return CES_ERROR_UNASSIGNED_CODE;
    const auto& reverse = jisReverse(strict ? JisStrict::Euc : JisStrict::None);
    const auto found = std::lower_bound(reverse.begin(), reverse.end(), static_cast<uint16_t>(code), [](const JisCell& entry, uint16_t value) { return entry.code < value; });
    if (found == reverse.end() || found->code != code || (found->supplementary && !hasSs3(profile))) return CES_ERROR_UNASSIGNED_CODE;
    if (found->supplementary) {
        length = 3;
        mbcs[0] = 0x8F;
        mbcs[1] = static_cast<uint8_t>(0xA1 + found->cell / 94);
        mbcs[2] = static_cast<uint8_t>(0xA1 + found->cell % 94);
        return 0;
    }
    length = 2;
    mbcs[0] = static_cast<uint8_t>(0xA1 + found->cell / 94);
    mbcs[1] = static_cast<uint8_t>(0xA1 + found->cell % 94);
    return 0;
}

int decodeUcs2(const uint16_t* ucs2, uint32_t, uint32_t& length, uint32_t& code) {
    length = 1;
    code = ucs2[0];
    return code >= 0xD800 && code <= 0xDFFF ? CES_ERROR_ILLEGAL_CODE : 0;
}

uint32_t encodeUcs2(uint32_t code, uint16_t* ucs2) {
    if (code > 0xFFFF) return 0;
    ucs2[0] = static_cast<uint16_t>(code);
    return 1;
}

uint32_t encodeUtf16(uint32_t code, uint16_t* utf16) {
    if (code < 0x10000) {
        utf16[0] = static_cast<uint16_t>(code);
        return 1;
    }
    utf16[0] = static_cast<uint16_t>(0xD800 | (code - 0x10000) >> 10);
    utf16[1] = static_cast<uint16_t>(0xDC00 | (code & 0x3FF));
    return 2;
}

template<typename Source, typename Target, typename Decode, typename Encode>
int convertString(const void* context, const Source* source, uint32_t sourceMax, uint32_t* sourceLen, Target* target, uint32_t targetMax, uint32_t* targetLen, bool counting, Decode decode, Encode encode) {
    if (sourceLen) *sourceLen = 0;
    if (targetLen) *targetLen = 0;
    if (target && targetMax > 0) target[0] = 0;
    if (!context) return CES_ERROR_INVALID_PARAMETER;
    const void* profile = contextProfile(context);
    if (!isMbcs(profile)) return CES_ERROR_INVALID_PARAMETER;
    if (!source) return CES_ERROR_INVALID_SRC_BUFFER;
    if (!counting && !target) return CES_ERROR_INVALID_DST_BUFFER;
    uint32_t read = 0;
    uint32_t written = 0;
    int result = 0;
    while ((sourceMax == 0 || read < sourceMax) && source[read] != 0) {
        uint32_t length = 0;
        uint32_t code = 0;
        const uint32_t available = sourceMax == 0 ? UINT32_MAX - read : sourceMax - read;
        if ((result = decode(profile, source + read, available, length, code)) != 0) break;
        Target encoded[4];
        uint32_t size = 0;
        if ((result = encode(profile, code, encoded, size)) != 0) break;
        if (!counting) {
            if (written + size >= targetMax) {
                result = CES_ERROR_DST_BUFFER_END;
                break;
            }
            std::memcpy(target + written, encoded, size * sizeof(Target));
        }
        read += length;
        written += size;
    }
    if (!counting && written < targetMax) target[written] = 0;
    if (sourceLen) *sourceLen = read;
    if (targetLen) *targetLen = written;
    return result;
}

template<typename Unit>
int rejected(int result, uint32_t length, uint32_t* sourceSize, Unit* character, uint32_t* produced) {
    if (length == 0) return result;
    if (sourceSize) *sourceSize = length;
    if (character) *character = 0;
    if (produced) *produced = 0;
    return result;
}

int mbcsToUtf8(bool accepted, const void* profile, const uint8_t* mbcs, uint32_t mbcsMax, uint32_t* mbcsLen, uint8_t* utf8, uint32_t utf8Max, uint32_t* utf8Len) {
    if (!accepted) return CES_ERROR_INVALID_PARAMETER;
    if (!mbcs) return CES_ERROR_INVALID_SRC_BUFFER;
    if (mbcsMax == 0) return CES_ERROR_SRC_BUFFER_END;
    if (!utf8) return CES_ERROR_INVALID_DST_BUFFER;
    uint32_t consumed = 0;
    uint32_t code = 0;
    if (const int result = decodeMbcs(profile, mbcs, mbcsMax, consumed, code); result != 0) return rejected<uint8_t>(result, consumed, mbcsLen, nullptr, utf8Len);
    uint8_t encoded[4];
    const uint32_t produced = encodeUtf8(code, encoded);
    if (utf8Max < produced) return CES_ERROR_DST_BUFFER_END;
    std::memcpy(utf8, encoded, produced);
    if (mbcsLen) *mbcsLen = consumed;
    if (utf8Len) *utf8Len = produced;
    return 0;
}

constexpr bool NATIVE_BIG_ENDIAN = std::endian::native == std::endian::big;

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

bool isScalar(uint32_t code) {
    return code <= 0x10FFFF && (code < 0xD800 || code > 0xDFFF);
}

int scalarError(uint32_t code) {
    if (code > 0x10FFFF) return CES_ERROR_INVALID_ENCODE;
    return code >= 0xD800 && code <= 0xDFFF ? CES_ERROR_ILLEGAL_CODE : 0;
}

template<bool BigEndian>
int decodeUtf16(const uint16_t* utf16, uint32_t utf16max, uint32_t& length, uint32_t& code) {
    code = loadUnit<uint16_t, BigEndian>(utf16);
    length = 1;
    if (code >= 0xDC00 && code <= 0xDFFF) return CES_ERROR_INVALID_ENCODE;
    if (code >= 0xD800 && code <= 0xDBFF) {
        if (utf16max < 2) return CES_ERROR_SRC_BUFFER_END;
        const uint32_t low = loadUnit<uint16_t, BigEndian>(utf16 + 1);
        if (low < 0xDC00 || low > 0xDFFF) return CES_ERROR_INVALID_ENCODE;
        code = 0x10000 + ((code - 0xD800) << 10) + low - 0xDC00;
        length = 2;
    }
    return 0;
}

int decodeProfiled(bool (*matches)(const void*), const void* profile, const uint8_t* source, uint32_t sourceMax, const void* target, uint32_t& length, uint32_t& code) {
    if (!matches(profile)) return CES_ERROR_INVALID_PARAMETER;
    if (!source) return CES_ERROR_INVALID_SRC_BUFFER;
    if (sourceMax == 0) return CES_ERROR_SRC_BUFFER_END;
    if (!target) return CES_ERROR_INVALID_DST_BUFFER;
    return decodeMbcs(profile, source, sourceMax, length, code);
}


int profiledToUcs2(bool (*matches)(const void*), const void* profile, const uint8_t* source, uint32_t sourceMax, uint32_t* sourceSize, uint16_t* ucs2) {
    uint32_t length = 0;
    uint32_t code = 0;
    if (const int result = decodeProfiled(matches, profile, source, sourceMax, ucs2, length, code); result != 0) return rejected(result, length, sourceSize, ucs2, nullptr);
    if (code > 0xFFFF) return CES_ERROR_UNASSIGNED_CODE;
    *ucs2 = static_cast<uint16_t>(code);
    if (sourceSize) *sourceSize = length;
    return 0;
}

int profiledToUtf8(bool (*matches)(const void*), const void* profile, const uint8_t* source, uint32_t sourceMax, uint32_t* sourceSize, uint8_t* utf8, uint32_t utf8max, uint32_t* utf8Len) {
    uint32_t length = 0;
    uint32_t code = 0;
    if (const int result = decodeProfiled(matches, profile, source, sourceMax, utf8, length, code); result != 0) return rejected<uint8_t>(result, length, sourceSize, nullptr, utf8Len);
    uint8_t encoded[4];
    const uint32_t produced = encodeUtf8(code, encoded);
    if (utf8max < produced) return CES_ERROR_DST_BUFFER_END;
    std::memcpy(utf8, encoded, produced);
    if (sourceSize) *sourceSize = length;
    if (utf8Len) *utf8Len = produced;
    return 0;
}

template<bool BigEndian>
int profiledToUtf16(bool (*matches)(const void*), const void* profile, const uint8_t* source, uint32_t sourceMax, uint32_t* sourceSize, uint16_t* utf16, uint32_t utf16max, uint32_t* utf16Len) {
    uint32_t length = 0;
    uint32_t code = 0;
    if (const int result = decodeProfiled(matches, profile, source, sourceMax, utf16, length, code); result != 0) return rejected<uint16_t>(result, length, sourceSize, nullptr, utf16Len);
    const uint32_t units = code < 0x10000 ? 1 : 2;
    if (utf16max < units) return CES_ERROR_DST_BUFFER_END;
    if (units == 1) {
        storeUnit<uint16_t, BigEndian>(utf16, code);
    } else {
        storeUnit<uint16_t, BigEndian>(utf16, 0xD800 | (code - 0x10000) >> 10);
        storeUnit<uint16_t, BigEndian>(utf16 + 1, 0xDC00 | (code & 0x3FF));
    }
    if (sourceSize) *sourceSize = length;
    if (utf16Len) *utf16Len = units;
    return 0;
}

template<bool BigEndian>
int profiledToUtf32(bool (*matches)(const void*), const void* profile, const uint8_t* source, uint32_t sourceMax, uint32_t* sourceSize, uint32_t* utf32) {
    uint32_t length = 0;
    uint32_t code = 0;
    if (const int result = decodeProfiled(matches, profile, source, sourceMax, utf32, length, code); result != 0) return rejected(result, length, sourceSize, utf32, nullptr);
    storeUnit<uint32_t, BigEndian>(utf32, code);
    if (sourceSize) *sourceSize = length;
    return 0;
}

int encodeProfiled(const void* profile, uint32_t code, uint8_t* target, uint32_t targetMax, uint32_t* targetSize) {
    uint8_t encoded[4];
    uint32_t size = 0;
    if (const int result = encodeMbcs(profile, code, encoded, size); result != 0) return result;
    if (targetMax < size) return CES_ERROR_DST_BUFFER_END;
    std::memcpy(target, encoded, size);
    if (targetSize) *targetSize = size;
    return 0;
}

int scalarToProfiled(bool (*matches)(const void*), uint32_t code, const void* profile, uint8_t* target, uint32_t targetMax, uint32_t* targetSize) {
    if (!matches(profile)) return CES_ERROR_INVALID_PARAMETER;
    if (!target) return CES_ERROR_INVALID_DST_BUFFER;
    int result = scalarError(code);
    uint8_t encoded[4];
    uint32_t size = 0;
    if (result == 0) result = encodeMbcs(profile, code, encoded, size);
    if (result == 0 && targetMax < size) result = CES_ERROR_DST_BUFFER_END;
    if (result == 0) std::memcpy(target, encoded, size);
    if (targetSize) *targetSize = result == 0 || result == CES_ERROR_DST_BUFFER_END ? size : 0;
    return result;
}

template<bool BigEndian>
int utf32ToProfiled(bool (*matches)(const void*), const uint32_t* utf32, const void* profile, uint8_t* target, uint32_t targetMax, uint32_t* targetSize) {
    if (!matches(profile)) return CES_ERROR_INVALID_PARAMETER;
    if (!utf32) return CES_ERROR_INVALID_SRC_BUFFER;
    return scalarToProfiled(matches, loadUnit<uint32_t, BigEndian>(utf32), profile, target, targetMax, targetSize);
}

template<typename Source, typename Decode>
int bufferToProfiled(bool (*matches)(const void*), const Source* source, uint32_t sourceMax, uint32_t* sourceLen, const void* profile, uint8_t* target, uint32_t targetMax, uint32_t* targetSize, Decode decode) {
    if (!matches(profile)) return CES_ERROR_INVALID_PARAMETER;
    if (!source) return CES_ERROR_INVALID_SRC_BUFFER;
    if (sourceMax == 0) return CES_ERROR_SRC_BUFFER_END;
    if (!target) return CES_ERROR_INVALID_DST_BUFFER;
    uint32_t length = 0;
    uint32_t code = 0;
    if (const int result = decode(source, sourceMax, length, code); result != 0) return result;
    if (const int result = encodeProfiled(profile, code, target, targetMax, targetSize); result != 0) return result;
    if (sourceLen) *sourceLen = length;
    return 0;
}

int decodeUtf32(const uint32_t* utf32, uint32_t, uint32_t& length, uint32_t& code) {
    length = 1;
    code = utf32[0];
    return isScalar(code) ? 0 : CES_ERROR_INVALID_ENCODE;
}

uint32_t encodeUtf32(uint32_t code, uint32_t* utf32) {
    utf32[0] = code;
    return 1;
}

template<auto Decode, typename Unit>
int unitSource(const void*, const Unit* source, uint32_t sourceMax, uint32_t& length, uint32_t& code) {
    return Decode(source, sourceMax, length, code);
}

template<auto Encode, typename Unit>
int unitTarget(const void*, uint32_t code, Unit* target, uint32_t& length) {
    length = Encode(code, target);
    return length != 0 ? 0 : CES_ERROR_UNASSIGNED_CODE;
}

int sbcToUnit(const uint8_t* profile, uint8_t sbc, const void* target, uint32_t& code) {
    if (!isSbc(profile)) return CES_ERROR_INVALID_PARAMETER;
    if (!target) return CES_ERROR_INVALID_DST_BUFFER;
    return sbcToUnicode(profile, sbc, code) ? 0 : CES_ERROR_UNASSIGNED_CODE;
}

template<bool BigEndian>
int sbcToUtf16(const uint8_t* profile, uint8_t sbc, uint16_t* utf16, uint32_t utf16max, uint32_t* utf16Len) {
    uint32_t code = 0;
    if (const int result = sbcToUnit(profile, sbc, utf16, code); result != 0) return result;
    if (utf16max < 1) return CES_ERROR_DST_BUFFER_END;
    storeUnit<uint16_t, BigEndian>(utf16, code);
    if (utf16Len) *utf16Len = 1;
    return 0;
}

template<bool BigEndian>
int sbcToUtf32(const uint8_t* profile, uint8_t sbc, uint32_t* utf32) {
    uint32_t code = 0;
    if (const int result = sbcToUnit(profile, sbc, utf32, code); result != 0) return result;
    storeUnit<uint32_t, BigEndian>(utf32, code);
    return 0;
}

int scalarToSbc(uint32_t code, const uint8_t* profile, uint8_t* sbc) {
    if (!isSbc(profile)) return CES_ERROR_INVALID_PARAMETER;
    if (!sbc) return CES_ERROR_INVALID_DST_BUFFER;
    if (!isScalar(code)) return CES_ERROR_INVALID_ENCODE;
    return unicodeToSbc(profile, code, *sbc) ? 0 : CES_ERROR_UNASSIGNED_CODE;
}

template<bool BigEndian>
int utf16ToSbc(const uint16_t* utf16, uint32_t utf16max, uint32_t* utf16Len, const uint8_t* profile, uint8_t* sbc) {
    if (!isSbc(profile)) return CES_ERROR_INVALID_PARAMETER;
    if (!utf16) return CES_ERROR_INVALID_SRC_BUFFER;
    if (utf16max == 0) return CES_ERROR_SRC_BUFFER_END;
    if (!sbc) return CES_ERROR_INVALID_DST_BUFFER;
    uint32_t length = 0;
    uint32_t code = 0;
    if (const int result = decodeUtf16<BigEndian>(utf16, utf16max, length, code); result != 0) return result;
    if (!unicodeToSbc(profile, code, *sbc)) return CES_ERROR_UNASSIGNED_CODE;
    if (utf16Len) *utf16Len = length;
    return 0;
}

template<bool BigEndian>
int utf32ToSbc(const uint32_t* utf32, const uint8_t* profile, uint8_t* sbc) {
    if (!isSbc(profile)) return CES_ERROR_INVALID_PARAMETER;
    if (!utf32) return CES_ERROR_INVALID_SRC_BUFFER;
    return scalarToSbc(loadUnit<uint32_t, BigEndian>(utf32), profile, sbc);
}

}

extern "C" {

const uint8_t* APS5_VABI sceCesRefersUcsProfileCp1252(void) {
    return CP1252_PROFILE;
}

const uint8_t* APS5_VABI sceCesRefersUcsProfileCp1251(void) {
    return CP1251_PROFILE;
}

const void* APS5_VABI sceCesRefersUcsProfileCp1250(void) {
    return CP1250_PROFILE;
}

const uint8_t* APS5_VABI sceCesRefersUcsProfileIso8859_1(void) {
    return singleByteProfile(SingleByte::Iso8859_1);
}

const uint8_t* APS5_VABI sceCesRefersUcsProfileIso8859_2(void) {
    return singleByteProfile(SingleByte::Iso8859_2);
}

const uint8_t* APS5_VABI sceCesRefersUcsProfileIso8859_4(void) {
    return singleByteProfile(SingleByte::Iso8859_4);
}

const uint8_t* APS5_VABI sceCesRefersUcsProfileIso8859_5(void) {
    return singleByteProfile(SingleByte::Iso8859_5);
}

const uint8_t* APS5_VABI sceCesRefersUcsProfileIso8859_9(void) {
    return singleByteProfile(SingleByte::Iso8859_9);
}

const uint8_t* APS5_VABI sceCesRefersUcsProfileIso8859_10(void) {
    return singleByteProfile(SingleByte::Iso8859_10);
}

const uint8_t* APS5_VABI sceCesRefersUcsProfileIso8859_13(void) {
    return singleByteProfile(SingleByte::Iso8859_13);
}

const uint8_t* APS5_VABI sceCesRefersUcsProfileIso8859_14(void) {
    return singleByteProfile(SingleByte::Iso8859_14);
}

const uint8_t* APS5_VABI sceCesRefersUcsProfileIso8859_15(void) {
    return singleByteProfile(SingleByte::Iso8859_15);
}

const uint8_t* APS5_VABI sceCesRefersUcsProfileIso8859_16(void) {
    return singleByteProfile(SingleByte::Iso8859_16);
}

const uint8_t* APS5_VABI sceCesRefersUcsProfileCp437(void) {
    return singleByteProfile(SingleByte::Cp437);
}

const uint8_t* APS5_VABI sceCesRefersUcsProfileCp737(void) {
    return singleByteProfile(SingleByte::Cp737);
}

const uint8_t* APS5_VABI sceCesRefersUcsProfileCp775(void) {
    return singleByteProfile(SingleByte::Cp775);
}

const uint8_t* APS5_VABI sceCesRefersUcsProfileCp850(void) {
    return singleByteProfile(SingleByte::Cp850);
}

const uint8_t* APS5_VABI sceCesRefersUcsProfileCp858(void) {
    return singleByteProfile(SingleByte::Cp858);
}

const uint8_t* APS5_VABI sceCesRefersUcsProfileCp852(void) {
    return singleByteProfile(SingleByte::Cp852);
}

const uint8_t* APS5_VABI sceCesRefersUcsProfileCp855(void) {
    return singleByteProfile(SingleByte::Cp855);
}

const uint8_t* APS5_VABI sceCesRefersUcsProfileCp860(void) {
    return singleByteProfile(SingleByte::Cp860);
}

const uint8_t* APS5_VABI sceCesRefersUcsProfileCp861(void) {
    return singleByteProfile(SingleByte::Cp861);
}

const uint8_t* APS5_VABI sceCesRefersUcsProfileCp862(void) {
    return singleByteProfile(SingleByte::Cp862);
}

const uint8_t* APS5_VABI sceCesRefersUcsProfileCp863(void) {
    return singleByteProfile(SingleByte::Cp863);
}

const uint8_t* APS5_VABI sceCesRefersUcsProfileCp865(void) {
    return singleByteProfile(SingleByte::Cp865);
}

const uint8_t* APS5_VABI sceCesRefersUcsProfileCp866(void) {
    return singleByteProfile(SingleByte::Cp866);
}

const uint8_t* APS5_VABI sceCesRefersUcsProfileKoi8R(void) {
    return singleByteProfile(SingleByte::Koi8R);
}

const uint8_t* APS5_VABI sceCesRefersUcsProfileKoi8U(void) {
    return singleByteProfile(SingleByte::Koi8U);
}

const uint8_t* APS5_VABI sceCesRefersUcsProfileCp1254(void) {
    return singleByteProfile(SingleByte::Cp1254);
}

const uint8_t* APS5_VABI sceCesRefersUcsProfileCp1256(void) {
    return singleByteProfile(SingleByte::Cp1256);
}

const uint8_t* APS5_VABI sceCesRefersUcsProfileCp1258(void) {
    return singleByteProfile(SingleByte::Cp1258);
}

void* APS5_VABI sceCesUcsProfileInitSJis1997Cp932(void* sheet) {
    if (!sheet) return nullptr;
    std::memcpy(sheet, SJIS_TAG, sizeof(SJIS_TAG));
    return sheet;
}

void* APS5_VABI sceCesUcsProfileInitSJis1997X0208(void* sheet) {
    if (!sheet) return nullptr;
    std::memcpy(sheet, SJIS_X0208_TAG, sizeof(SJIS_X0208_TAG));
    return sheet;
}

int APS5_VABI sceCesMbcsUcsContextInit(void* context, const void* profile) {
    if (!context || !profile) return CES_ERROR_INVALID_PARAMETER;
    std::memcpy(context, &profile, sizeof(profile));
    return 0;
}

int APS5_VABI sceCesMbcsUcsContextInitCopy(void* context, const void* source) {
    if (!context || !source) return CES_ERROR_INVALID_PARAMETER;
    const void* profile = contextProfile(source);
    std::memcpy(context, &profile, sizeof(profile));
    return 0;
}

void* APS5_VABI sceCesUcsProfileInitEucJpCp51932(void* sheet) {
    if (!sheet) return nullptr;
    std::memcpy(sheet, EUC_JP_TAG, sizeof(EUC_JP_TAG));
    return sheet;
}

void* APS5_VABI sceCesUcsProfileInitEucJpX0208(void* sheet) {
    if (!sheet) return nullptr;
    std::memcpy(sheet, EUC_JP_X0208_TAG, sizeof(EUC_JP_X0208_TAG));
    return sheet;
}

void* APS5_VABI sceCesUcsProfileInitEucJpX0208Ss2(void* sheet) {
    if (!sheet) return nullptr;
    std::memcpy(sheet, EUC_JP_X0208_SS2_TAG, sizeof(EUC_JP_X0208_SS2_TAG));
    return sheet;
}

void* APS5_VABI sceCesUcsProfileInitEucJpX0208Ss2Ss3(void* sheet) {
    if (!sheet) return nullptr;
    std::memcpy(sheet, EUC_JP_X0208_SS3_TAG, sizeof(EUC_JP_X0208_SS3_TAG));
    return sheet;
}

void* APS5_VABI sceCesUcsProfileInitEucKr(void* sheet) {
    if (!sheet) return nullptr;
    std::memcpy(sheet, EUC_KR_TAG, sizeof(EUC_KR_TAG));
    return sheet;
}

void* APS5_VABI sceCesUcsProfileInitUhc(void* sheet) {
    if (!sheet) return nullptr;
    std::memcpy(sheet, UHC_TAG, sizeof(UHC_TAG));
    return sheet;
}

void* APS5_VABI sceCesUcsProfileInitBig5Cp950(void* sheet) {
    if (!sheet) return nullptr;
    std::memcpy(sheet, BIG5_TAG, sizeof(BIG5_TAG));
    return sheet;
}

int APS5_VABI sceCesUtf8StrToMbcsStr(void* context, const uint8_t* utf8, uint32_t utf8max, uint32_t* utf8Len, uint8_t* mbcs, uint32_t mbcsMax, uint32_t* mbcsLen) {
    return convertString(context, utf8, utf8max, utf8Len, mbcs, mbcsMax, mbcsLen, false, unitSource<decodeScalarUtf8, uint8_t>, encodeMbcs);
}

int APS5_VABI sceCesUtf8StrGetMbcsLen(void* context, const uint8_t* utf8, uint32_t utf8max, uint32_t* utf8Len, uint32_t* mbcsLen) {
    return convertString(context, utf8, utf8max, utf8Len, static_cast<uint8_t*>(nullptr), 0, mbcsLen, true, unitSource<decodeScalarUtf8, uint8_t>, encodeMbcs);
}

int APS5_VABI sceCesUcs2StrToMbcsStr(void* context, const uint16_t* ucs2, uint32_t ucs2max, uint32_t* ucs2Len, uint8_t* mbcs, uint32_t mbcsMax, uint32_t* mbcsLen) {
    return convertString(context, ucs2, ucs2max, ucs2Len, mbcs, mbcsMax, mbcsLen, false, unitSource<decodeUcs2, uint16_t>, encodeMbcs);
}

int APS5_VABI sceCesUcs2StrGetMbcsLen(void* context, const uint16_t* ucs2, uint32_t ucs2max, uint32_t* ucs2Len, uint32_t* mbcsLen) {
    return convertString(context, ucs2, ucs2max, ucs2Len, static_cast<uint8_t*>(nullptr), 0, mbcsLen, true, unitSource<decodeUcs2, uint16_t>, encodeMbcs);
}

int APS5_VABI sceCesMbcsStrToUtf8Str(void* context, const uint8_t* mbcs, uint32_t mbcsMax, uint32_t* mbcsLen, uint8_t* utf8, uint32_t utf8max, uint32_t* utf8Len) {
    return convertString(context, mbcs, mbcsMax, mbcsLen, utf8, utf8max, utf8Len, false, decodeMbcs, unitTarget<encodeUtf8, uint8_t>);
}

int APS5_VABI sceCesMbcsStrGetUtf8Len(void* context, const uint8_t* mbcs, uint32_t mbcsMax, uint32_t* mbcsLen, uint32_t* utf8Len) {
    return convertString(context, mbcs, mbcsMax, mbcsLen, static_cast<uint8_t*>(nullptr), 0, utf8Len, true, decodeMbcs, unitTarget<encodeUtf8, uint8_t>);
}

int APS5_VABI sceCesMbcsStrToUcs2Str(void* context, const uint8_t* mbcs, uint32_t mbcsMax, uint32_t* mbcsLen, uint16_t* ucs2, uint32_t ucs2max, uint32_t* ucs2Len) {
    return convertString(context, mbcs, mbcsMax, mbcsLen, ucs2, ucs2max, ucs2Len, false, decodeMbcs, unitTarget<encodeUcs2, uint16_t>);
}

int APS5_VABI sceCesMbcsStrGetUcs2Len(void* context, const uint8_t* mbcs, uint32_t mbcsMax, uint32_t* mbcsLen, uint32_t* ucs2Len) {
    return convertString(context, mbcs, mbcsMax, mbcsLen, static_cast<uint16_t*>(nullptr), 0, ucs2Len, true, decodeMbcs, unitTarget<encodeUcs2, uint16_t>);
}

int APS5_VABI sceCesEucJpToUcs2(const void* profile, const uint8_t* euc, uint32_t eucMax, uint32_t* eucSize, uint16_t* ucs2) {
    if (eucSize) *eucSize = 0;
    if (ucs2) *ucs2 = 0;
    if (!profile) return CES_ERROR_INVALID_PROFILE;
    if (!isEucJp(profile)) return CES_ERROR_INVALID_PARAMETER;
    if (!euc || eucMax == 0) return CES_ERROR_INVALID_SRC_BUFFER;
    uint32_t length = 0;
    uint32_t code = 0;
    const int decoded = decodeEucJp(profile, euc, eucMax, length, code);
    if (eucSize) *eucSize = length;
    if (decoded != 0) return decoded;
    if (!ucs2) return CES_ERROR_INVALID_DST_BUFFER;
    *ucs2 = static_cast<uint16_t>(code);
    return 0;
}

int APS5_VABI sceCesSbcToUtf8(const uint8_t* profile, uint8_t sbc, uint8_t* utf8, uint32_t utf8max, uint32_t* utf8_len) {
    if (utf8_len) *utf8_len = 0;
    if (!profile) return CES_ERROR_INVALID_PROFILE;
    if (!isSbc(profile)) return CES_ERROR_INVALID_PARAMETER;
    uint32_t code = 0;
    if (!sbcToUnicode(profile, sbc, code)) return CES_ERROR_UNASSIGNED_CODE;
    uint8_t encoded[4];
    const uint32_t length = encodeUtf8(code, encoded);
    if (utf8_len) *utf8_len = length;
    if (!utf8) return CES_ERROR_INVALID_DST_BUFFER;
    if (utf8max < length) return CES_ERROR_DST_BUFFER_END;
    for (uint32_t i = 0; i < length; ++i) utf8[i] = encoded[i];
    return 0;
}

int APS5_VABI sceCesUtf8ToSbc(const uint8_t* utf8, uint32_t utf8max, uint32_t* utf8_len, const uint8_t* profile, uint8_t* sbc) {
    if (utf8_len) *utf8_len = 0;
    if (sbc) *sbc = 0;
    if (!utf8 || utf8max == 0) return CES_ERROR_INVALID_SRC_BUFFER;
    uint32_t length = 0;
    uint32_t code = 0;
    const int decoded = decodeScalarUtf8(utf8, utf8max, length, code);
    if (utf8_len) *utf8_len = length;
    if (decoded != 0) return decoded;
    if (!profile) return CES_ERROR_INVALID_PROFILE;
    if (!isSbc(profile)) return CES_ERROR_INVALID_PARAMETER;
    uint8_t value = 0;
    if (!unicodeToSbc(profile, code, value)) return CES_ERROR_UNASSIGNED_CODE;
    if (!sbc) return CES_ERROR_INVALID_DST_BUFFER;
    *sbc = value;
    return 0;
}

int APS5_VABI sceCesEucJpToUtf8(const void* profile, const uint8_t* eucBuffer, uint32_t eucMax, uint32_t* eucLength, uint8_t* utf8Buffer, uint32_t utf8Max, uint32_t* utf8Length) {
    if (eucLength) *eucLength = 0;
    if (utf8Length) *utf8Length = 0;
    if (!profile) return CES_ERROR_INVALID_PROFILE;
    if (!isEucJp(profile)) return CES_ERROR_INVALID_PARAMETER;
    if (!eucBuffer || eucMax == 0) return CES_ERROR_INVALID_SRC_BUFFER;
    uint32_t consumed = 0;
    uint32_t code = 0;
    const int decoded = decodeEucJp(profile, eucBuffer, eucMax, consumed, code);
    if (eucLength) *eucLength = consumed;
    if (decoded != 0) return decoded;
    uint8_t encoded[4];
    const uint32_t produced = encodeUtf8(code, encoded);
    if (utf8Length) *utf8Length = produced;
    if (!utf8Buffer) return CES_ERROR_INVALID_DST_BUFFER;
    if (utf8Max < produced) return CES_ERROR_DST_BUFFER_END;
    std::memcpy(utf8Buffer, encoded, produced);
    return 0;
}

int APS5_VABI sceCesEucKrToUtf8(const void* profile, const uint8_t* eucBuffer, uint32_t eucMax, uint32_t* eucLength, uint8_t* utf8Buffer, uint32_t utf8Max, uint32_t* utf8Length) {
    return mbcsToUtf8(isEucKr(profile), profile, eucBuffer, eucMax, eucLength, utf8Buffer, utf8Max, utf8Length);
}

int APS5_VABI sceCesBig5ToUtf8(const void* profile, const uint8_t* big5Buffer, uint32_t big5Max, uint32_t* big5Length, uint8_t* utf8Buffer, uint32_t utf8Max, uint32_t* utf8Length) {
    return mbcsToUtf8(isBig5(profile), profile, big5Buffer, big5Max, big5Length, utf8Buffer, utf8Max, utf8Length);
}

int APS5_VABI sceCesUhcToUtf8(const void* profile, const uint8_t* uhcBuffer, uint32_t uhcMax, uint32_t* uhcLength, uint8_t* utf8Buffer, uint32_t utf8Max, uint32_t* utf8Length) {
    return mbcsToUtf8(isUhc(profile), profile, uhcBuffer, uhcMax, uhcLength, utf8Buffer, utf8Max, utf8Length);
}

int APS5_VABI sceCesSJisToUcs2(const void* profile, const uint8_t* sjisBuffer, uint32_t sjisMax, uint32_t* sjisSize, uint16_t* ucs2) {
    return profiledToUcs2(isSjis, profile, sjisBuffer, sjisMax, sjisSize, ucs2);
}

int APS5_VABI sceCesSJisToUtf8(const void* profile, const uint8_t* sjisBuffer, uint32_t sjisMax, uint32_t* sjisSize, uint8_t* utf8Buffer, uint32_t utf8Max, uint32_t* utf8Length) {
    return profiledToUtf8(isSjis, profile, sjisBuffer, sjisMax, sjisSize, utf8Buffer, utf8Max, utf8Length);
}

int APS5_VABI sceCesSJisToUtf16(const void* profile, const uint8_t* sjisBuffer, uint32_t sjisMax, uint32_t* sjisSize, uint16_t* utf16Buffer, uint32_t utf16Max, uint32_t* utf16Length) {
    return profiledToUtf16<NATIVE_BIG_ENDIAN>(isSjis, profile, sjisBuffer, sjisMax, sjisSize, utf16Buffer, utf16Max, utf16Length);
}

int APS5_VABI sceCesSJisToUtf16be(const void* profile, const uint8_t* sjisBuffer, uint32_t sjisMax, uint32_t* sjisSize, uint16_t* utf16Buffer, uint32_t utf16Max, uint32_t* utf16Length) {
    return profiledToUtf16<true>(isSjis, profile, sjisBuffer, sjisMax, sjisSize, utf16Buffer, utf16Max, utf16Length);
}

int APS5_VABI sceCesSJisToUtf16le(const void* profile, const uint8_t* sjisBuffer, uint32_t sjisMax, uint32_t* sjisSize, uint16_t* utf16Buffer, uint32_t utf16Max, uint32_t* utf16Length) {
    return profiledToUtf16<false>(isSjis, profile, sjisBuffer, sjisMax, sjisSize, utf16Buffer, utf16Max, utf16Length);
}

int APS5_VABI sceCesSJisToUtf32(const void* profile, const uint8_t* sjisBuffer, uint32_t sjisMax, uint32_t* sjisSize, uint32_t* utf32) {
    return profiledToUtf32<NATIVE_BIG_ENDIAN>(isSjis, profile, sjisBuffer, sjisMax, sjisSize, utf32);
}

int APS5_VABI sceCesSJisToUtf32be(const void* profile, const uint8_t* sjisBuffer, uint32_t sjisMax, uint32_t* sjisSize, uint32_t* utf32) {
    return profiledToUtf32<true>(isSjis, profile, sjisBuffer, sjisMax, sjisSize, utf32);
}

int APS5_VABI sceCesSJisToUtf32le(const void* profile, const uint8_t* sjisBuffer, uint32_t sjisMax, uint32_t* sjisSize, uint32_t* utf32) {
    return profiledToUtf32<false>(isSjis, profile, sjisBuffer, sjisMax, sjisSize, utf32);
}

int APS5_VABI sceCesEucJpToUtf16(const void* profile, const uint8_t* eucBuffer, uint32_t eucMax, uint32_t* eucSize, uint16_t* utf16Buffer, uint32_t utf16Max, uint32_t* utf16Length) {
    return profiledToUtf16<NATIVE_BIG_ENDIAN>(isEucJp, profile, eucBuffer, eucMax, eucSize, utf16Buffer, utf16Max, utf16Length);
}

int APS5_VABI sceCesEucJpToUtf16be(const void* profile, const uint8_t* eucBuffer, uint32_t eucMax, uint32_t* eucSize, uint16_t* utf16Buffer, uint32_t utf16Max, uint32_t* utf16Length) {
    return profiledToUtf16<true>(isEucJp, profile, eucBuffer, eucMax, eucSize, utf16Buffer, utf16Max, utf16Length);
}

int APS5_VABI sceCesEucJpToUtf16le(const void* profile, const uint8_t* eucBuffer, uint32_t eucMax, uint32_t* eucSize, uint16_t* utf16Buffer, uint32_t utf16Max, uint32_t* utf16Length) {
    return profiledToUtf16<false>(isEucJp, profile, eucBuffer, eucMax, eucSize, utf16Buffer, utf16Max, utf16Length);
}

int APS5_VABI sceCesEucJpToUtf32(const void* profile, const uint8_t* eucBuffer, uint32_t eucMax, uint32_t* eucSize, uint32_t* utf32) {
    return profiledToUtf32<NATIVE_BIG_ENDIAN>(isEucJp, profile, eucBuffer, eucMax, eucSize, utf32);
}

int APS5_VABI sceCesEucJpToUtf32be(const void* profile, const uint8_t* eucBuffer, uint32_t eucMax, uint32_t* eucSize, uint32_t* utf32) {
    return profiledToUtf32<true>(isEucJp, profile, eucBuffer, eucMax, eucSize, utf32);
}

int APS5_VABI sceCesEucJpToUtf32le(const void* profile, const uint8_t* eucBuffer, uint32_t eucMax, uint32_t* eucSize, uint32_t* utf32) {
    return profiledToUtf32<false>(isEucJp, profile, eucBuffer, eucMax, eucSize, utf32);
}

int APS5_VABI sceCesUcs2ToSJis(uint16_t ucs2, const void* profile, uint8_t* sjisBuffer, uint32_t sjisMax, uint32_t* sjisSize) {
    return scalarToProfiled(isSjis, ucs2, profile, sjisBuffer, sjisMax, sjisSize);
}

int APS5_VABI sceCesUtf8ToSJis(const uint8_t* utf8Buffer, uint32_t utf8Max, uint32_t* utf8Length, const void* profile, uint8_t* sjisBuffer, uint32_t sjisMax, uint32_t* sjisSize) {
    return bufferToProfiled(isSjis, utf8Buffer, utf8Max, utf8Length, profile, sjisBuffer, sjisMax, sjisSize, decodeScalarUtf8);
}

int APS5_VABI sceCesUtf16ToSJis(const uint16_t* utf16Buffer, uint32_t utf16Max, uint32_t* utf16Length, const void* profile, uint8_t* sjisBuffer, uint32_t sjisMax, uint32_t* sjisSize) {
    return bufferToProfiled(isSjis, utf16Buffer, utf16Max, utf16Length, profile, sjisBuffer, sjisMax, sjisSize, decodeUtf16<NATIVE_BIG_ENDIAN>);
}

int APS5_VABI sceCesUtf16beToSJis(const uint16_t* utf16Buffer, uint32_t utf16Max, uint32_t* utf16Length, const void* profile, uint8_t* sjisBuffer, uint32_t sjisMax, uint32_t* sjisSize) {
    return bufferToProfiled(isSjis, utf16Buffer, utf16Max, utf16Length, profile, sjisBuffer, sjisMax, sjisSize, decodeUtf16<true>);
}

int APS5_VABI sceCesUtf16leToSJis(const uint16_t* utf16Buffer, uint32_t utf16Max, uint32_t* utf16Length, const void* profile, uint8_t* sjisBuffer, uint32_t sjisMax, uint32_t* sjisSize) {
    return bufferToProfiled(isSjis, utf16Buffer, utf16Max, utf16Length, profile, sjisBuffer, sjisMax, sjisSize, decodeUtf16<false>);
}

int APS5_VABI sceCesUtf32ToSJis(uint32_t utf32, const void* profile, uint8_t* sjisBuffer, uint32_t sjisMax, uint32_t* sjisSize) {
    return scalarToProfiled(isSjis, utf32, profile, sjisBuffer, sjisMax, sjisSize);
}

int APS5_VABI sceCesUtf32beToSJis(const uint32_t* utf32, const void* profile, uint8_t* sjisBuffer, uint32_t sjisMax, uint32_t* sjisSize) {
    return utf32ToProfiled<true>(isSjis, utf32, profile, sjisBuffer, sjisMax, sjisSize);
}

int APS5_VABI sceCesUtf32leToSJis(const uint32_t* utf32, const void* profile, uint8_t* sjisBuffer, uint32_t sjisMax, uint32_t* sjisSize) {
    return utf32ToProfiled<false>(isSjis, utf32, profile, sjisBuffer, sjisMax, sjisSize);
}

int APS5_VABI sceCesUcs2ToEucJp(uint16_t ucs2, const void* profile, uint8_t* eucBuffer, uint32_t eucMax, uint32_t* eucSize) {
    return scalarToProfiled(isEucJp, ucs2, profile, eucBuffer, eucMax, eucSize);
}

int APS5_VABI sceCesUtf8ToEucJp(const uint8_t* utf8Buffer, uint32_t utf8Max, uint32_t* utf8Length, const void* profile, uint8_t* eucBuffer, uint32_t eucMax, uint32_t* eucSize) {
    return bufferToProfiled(isEucJp, utf8Buffer, utf8Max, utf8Length, profile, eucBuffer, eucMax, eucSize, decodeScalarUtf8);
}

int APS5_VABI sceCesUtf16ToEucJp(const uint16_t* utf16Buffer, uint32_t utf16Max, uint32_t* utf16Length, const void* profile, uint8_t* eucBuffer, uint32_t eucMax, uint32_t* eucSize) {
    return bufferToProfiled(isEucJp, utf16Buffer, utf16Max, utf16Length, profile, eucBuffer, eucMax, eucSize, decodeUtf16<NATIVE_BIG_ENDIAN>);
}

int APS5_VABI sceCesUtf16beToEucJp(const uint16_t* utf16Buffer, uint32_t utf16Max, uint32_t* utf16Length, const void* profile, uint8_t* eucBuffer, uint32_t eucMax, uint32_t* eucSize) {
    return bufferToProfiled(isEucJp, utf16Buffer, utf16Max, utf16Length, profile, eucBuffer, eucMax, eucSize, decodeUtf16<true>);
}

int APS5_VABI sceCesUtf16leToEucJp(const uint16_t* utf16Buffer, uint32_t utf16Max, uint32_t* utf16Length, const void* profile, uint8_t* eucBuffer, uint32_t eucMax, uint32_t* eucSize) {
    return bufferToProfiled(isEucJp, utf16Buffer, utf16Max, utf16Length, profile, eucBuffer, eucMax, eucSize, decodeUtf16<false>);
}

int APS5_VABI sceCesUtf32ToEucJp(uint32_t utf32, const void* profile, uint8_t* eucBuffer, uint32_t eucMax, uint32_t* eucSize) {
    return scalarToProfiled(isEucJp, utf32, profile, eucBuffer, eucMax, eucSize);
}

int APS5_VABI sceCesUtf32beToEucJp(const uint32_t* utf32, const void* profile, uint8_t* eucBuffer, uint32_t eucMax, uint32_t* eucSize) {
    return utf32ToProfiled<true>(isEucJp, utf32, profile, eucBuffer, eucMax, eucSize);
}

int APS5_VABI sceCesUtf32leToEucJp(const uint32_t* utf32, const void* profile, uint8_t* eucBuffer, uint32_t eucMax, uint32_t* eucSize) {
    return utf32ToProfiled<false>(isEucJp, utf32, profile, eucBuffer, eucMax, eucSize);
}

int APS5_VABI sceCesSJisCodeToJisX0208(uint16_t sjisCode, uint8_t* ku, uint8_t* ten) {
    if (!ku || !ten) return CES_ERROR_INVALID_DST_BUFFER;
    int row = 0;
    int cell = 0;
    if (!sjisToJis(static_cast<uint8_t>(sjisCode >> 8), static_cast<uint8_t>(sjisCode), row, cell) || row > 94) return CES_ERROR_INVALID_ENCODE;
    *ku = static_cast<uint8_t>(row);
    *ten = static_cast<uint8_t>(cell);
    return 0;
}

int APS5_VABI sceCesJisX0208ToSJisCode(uint8_t ku, uint8_t ten, uint16_t* sjisCode) {
    if (!sjisCode) return CES_ERROR_INVALID_DST_BUFFER;
    if (ku < 1 || ku > 94 || ten < 1 || ten > 94) return CES_ERROR_INVALID_ENCODE;
    uint8_t lead = 0;
    uint8_t trail = 0;
    jisToSjis(ku, ten, lead, trail);
    *sjisCode = static_cast<uint16_t>(lead << 8 | trail);
    return 0;
}

int APS5_VABI sceCesSJisCodeToJisX0213(uint16_t sjisCode, uint8_t* men, uint8_t* ku, uint8_t* ten) {
    if (!men || !ku || !ten) return CES_ERROR_INVALID_DST_BUFFER;
    int row = 0;
    int cell = 0;
    if (!sjisToJis(static_cast<uint8_t>(sjisCode >> 8), static_cast<uint8_t>(sjisCode), row, cell)) return CES_ERROR_INVALID_ENCODE;
    *men = row <= 94 ? 1 : 2;
    *ku = row <= 94 ? static_cast<uint8_t>(row) : JIS_X0213_PLANE2_ROWS[row - 95];
    *ten = static_cast<uint8_t>(cell);
    return 0;
}

int APS5_VABI sceCesJisX0213ToSJisCode(uint8_t men, uint8_t ku, uint8_t ten, uint16_t* sjisCode) {
    if (!sjisCode) return CES_ERROR_INVALID_DST_BUFFER;
    if (men < 1 || men > 2 || ku < 1 || ku > 94 || ten < 1 || ten > 94) return CES_ERROR_INVALID_ENCODE;
    int row = ku;
    if (men == 2) {
        const auto* found = std::find(std::begin(JIS_X0213_PLANE2_ROWS), std::end(JIS_X0213_PLANE2_ROWS), ku);
        if (found == std::end(JIS_X0213_PLANE2_ROWS)) return CES_ERROR_INVALID_ENCODE;
        row = 95 + static_cast<int>(found - std::begin(JIS_X0213_PLANE2_ROWS));
    }
    uint8_t lead = 0;
    uint8_t trail = 0;
    jisToSjis(row, ten, lead, trail);
    *sjisCode = static_cast<uint16_t>(lead << 8 | trail);
    return 0;
}

int APS5_VABI sceCesSJisGetCode(const uint8_t* sjisBuffer, uint32_t sjisMax, uint32_t* sjisSize, uint16_t* sjisCode) {
    if (!sjisBuffer) return CES_ERROR_INVALID_SRC_BUFFER;
    if (sjisMax == 0) return CES_ERROR_SRC_BUFFER_END;
    if (!sjisCode) return CES_ERROR_INVALID_DST_BUFFER;
    const uint8_t lead = sjisBuffer[0];
    if (isSjisSingle(lead)) {
        *sjisCode = lead;
        if (sjisSize) *sjisSize = 1;
        return 0;
    }
    if (!((lead >= 0x81 && lead <= 0x9F) || (lead >= 0xE0 && lead <= 0xFC))) return CES_ERROR_INVALID_ENCODE;
    if (sjisMax < 2) return CES_ERROR_SRC_BUFFER_END;
    int row = 0;
    int cell = 0;
    if (!sjisToJis(lead, sjisBuffer[1], row, cell)) return CES_ERROR_INVALID_ENCODE;
    *sjisCode = static_cast<uint16_t>(lead << 8 | sjisBuffer[1]);
    if (sjisSize) *sjisSize = 2;
    return 0;
}

int APS5_VABI sceCesSJisPutCode(uint16_t sjisCode, uint8_t* sjisBuffer, uint32_t sjisMax, uint32_t* sjisSize) {
    if (!sjisBuffer) return CES_ERROR_INVALID_DST_BUFFER;
    const uint8_t lead = static_cast<uint8_t>(sjisCode >> 8);
    const uint8_t trail = static_cast<uint8_t>(sjisCode);
    int row = 0;
    int cell = 0;
    const uint32_t size = lead == 0 ? 1 : 2;
    if (size == 1 ? !isSjisSingle(trail) : !sjisToJis(lead, trail, row, cell)) return CES_ERROR_INVALID_ENCODE;
    if (sjisMax < size) return CES_ERROR_DST_BUFFER_END;
    if (size == 2) sjisBuffer[0] = lead;
    sjisBuffer[size - 1] = trail;
    if (sjisSize) *sjisSize = size;
    return 0;
}

int APS5_VABI sceCesMbcToUcs2(const void* profile, const uint8_t* mbcBuffer, uint32_t mbcMax, uint32_t* mbcLen, uint16_t* ucs2) {
    return profiledToUcs2(isMbcs, profile, mbcBuffer, mbcMax, mbcLen, ucs2);
}

int APS5_VABI sceCesMbcToUtf8(const void* profile, const uint8_t* mbcBuffer, uint32_t mbcMax, uint32_t* mbcLen, uint8_t* utf8Buffer, uint32_t utf8Max, uint32_t* utf8Len) {
    return profiledToUtf8(isMbcs, profile, mbcBuffer, mbcMax, mbcLen, utf8Buffer, utf8Max, utf8Len);
}

int APS5_VABI sceCesMbcToUtf16(const void* profile, const uint8_t* mbcBuffer, uint32_t mbcMax, uint32_t* mbcLen, uint16_t* utf16Buffer, uint32_t utf16Max, uint32_t* utf16Len) {
    return profiledToUtf16<NATIVE_BIG_ENDIAN>(isMbcs, profile, mbcBuffer, mbcMax, mbcLen, utf16Buffer, utf16Max, utf16Len);
}

int APS5_VABI sceCesMbcToUtf16be(const void* profile, const uint8_t* mbcBuffer, uint32_t mbcMax, uint32_t* mbcLen, uint16_t* utf16Buffer, uint32_t utf16Max, uint32_t* utf16Len) {
    return profiledToUtf16<true>(isMbcs, profile, mbcBuffer, mbcMax, mbcLen, utf16Buffer, utf16Max, utf16Len);
}

int APS5_VABI sceCesMbcToUtf16le(const void* profile, const uint8_t* mbcBuffer, uint32_t mbcMax, uint32_t* mbcLen, uint16_t* utf16Buffer, uint32_t utf16Max, uint32_t* utf16Len) {
    return profiledToUtf16<false>(isMbcs, profile, mbcBuffer, mbcMax, mbcLen, utf16Buffer, utf16Max, utf16Len);
}

int APS5_VABI sceCesMbcToUtf32(const void* profile, const uint8_t* mbcBuffer, uint32_t mbcMax, uint32_t* mbcLen, uint32_t* utf32) {
    return profiledToUtf32<NATIVE_BIG_ENDIAN>(isMbcs, profile, mbcBuffer, mbcMax, mbcLen, utf32);
}

int APS5_VABI sceCesMbcToUtf32be(const void* profile, const uint8_t* mbcBuffer, uint32_t mbcMax, uint32_t* mbcLen, uint32_t* utf32) {
    return profiledToUtf32<true>(isMbcs, profile, mbcBuffer, mbcMax, mbcLen, utf32);
}

int APS5_VABI sceCesMbcToUtf32le(const void* profile, const uint8_t* mbcBuffer, uint32_t mbcMax, uint32_t* mbcLen, uint32_t* utf32) {
    return profiledToUtf32<false>(isMbcs, profile, mbcBuffer, mbcMax, mbcLen, utf32);
}

int APS5_VABI sceCesUcs2ToMbc(uint16_t ucs2, const void* profile, uint8_t* mbcBuffer, uint32_t mbcMax, uint32_t* mbcLen) {
    return scalarToProfiled(isMbcs, ucs2, profile, mbcBuffer, mbcMax, mbcLen);
}

int APS5_VABI sceCesUtf8ToMbc(const uint8_t* utf8Buffer, uint32_t utf8Max, uint32_t* utf8Len, const void* profile, uint8_t* mbcBuffer, uint32_t mbcMax, uint32_t* mbcLen) {
    return bufferToProfiled(isMbcs, utf8Buffer, utf8Max, utf8Len, profile, mbcBuffer, mbcMax, mbcLen, decodeScalarUtf8);
}

int APS5_VABI sceCesUtf16ToMbc(const uint16_t* utf16Buffer, uint32_t utf16Max, uint32_t* utf16Len, const void* profile, uint8_t* mbcBuffer, uint32_t mbcMax, uint32_t* mbcLen) {
    return bufferToProfiled(isMbcs, utf16Buffer, utf16Max, utf16Len, profile, mbcBuffer, mbcMax, mbcLen, decodeUtf16<NATIVE_BIG_ENDIAN>);
}

int APS5_VABI sceCesUtf16beToMbc(const uint16_t* utf16Buffer, uint32_t utf16Max, uint32_t* utf16Len, const void* profile, uint8_t* mbcBuffer, uint32_t mbcMax, uint32_t* mbcLen) {
    return bufferToProfiled(isMbcs, utf16Buffer, utf16Max, utf16Len, profile, mbcBuffer, mbcMax, mbcLen, decodeUtf16<true>);
}

int APS5_VABI sceCesUtf16leToMbc(const uint16_t* utf16Buffer, uint32_t utf16Max, uint32_t* utf16Len, const void* profile, uint8_t* mbcBuffer, uint32_t mbcMax, uint32_t* mbcLen) {
    return bufferToProfiled(isMbcs, utf16Buffer, utf16Max, utf16Len, profile, mbcBuffer, mbcMax, mbcLen, decodeUtf16<false>);
}

int APS5_VABI sceCesUtf32ToMbc(uint32_t utf32, const void* profile, uint8_t* mbcBuffer, uint32_t mbcMax, uint32_t* mbcLen) {
    return scalarToProfiled(isMbcs, utf32, profile, mbcBuffer, mbcMax, mbcLen);
}

int APS5_VABI sceCesUtf32beToMbc(const uint32_t* utf32, const void* profile, uint8_t* mbcBuffer, uint32_t mbcMax, uint32_t* mbcLen) {
    return utf32ToProfiled<true>(isMbcs, utf32, profile, mbcBuffer, mbcMax, mbcLen);
}

int APS5_VABI sceCesUtf32leToMbc(const uint32_t* utf32, const void* profile, uint8_t* mbcBuffer, uint32_t mbcMax, uint32_t* mbcLen) {
    return utf32ToProfiled<false>(isMbcs, utf32, profile, mbcBuffer, mbcMax, mbcLen);
}

int APS5_VABI sceCesSbcToUcs2(const uint8_t* profile, uint8_t sbc, uint16_t* ucs2) {
    uint32_t code = 0;
    if (const int result = sbcToUnit(profile, sbc, ucs2, code); result != 0) return result;
    *ucs2 = static_cast<uint16_t>(code);
    return 0;
}

int APS5_VABI sceCesSbcToUtf16(const uint8_t* profile, uint8_t sbc, uint16_t* utf16, uint32_t utf16max, uint32_t* utf16Len) {
    return sbcToUtf16<NATIVE_BIG_ENDIAN>(profile, sbc, utf16, utf16max, utf16Len);
}

int APS5_VABI sceCesSbcToUtf16be(const uint8_t* profile, uint8_t sbc, uint16_t* utf16, uint32_t utf16max, uint32_t* utf16Len) {
    return sbcToUtf16<true>(profile, sbc, utf16, utf16max, utf16Len);
}

int APS5_VABI sceCesSbcToUtf16le(const uint8_t* profile, uint8_t sbc, uint16_t* utf16, uint32_t utf16max, uint32_t* utf16Len) {
    return sbcToUtf16<false>(profile, sbc, utf16, utf16max, utf16Len);
}

int APS5_VABI sceCesSbcToUtf32(const uint8_t* profile, uint8_t sbc, uint32_t* utf32) {
    return sbcToUtf32<NATIVE_BIG_ENDIAN>(profile, sbc, utf32);
}

int APS5_VABI sceCesSbcToUtf32be(const uint8_t* profile, uint8_t sbc, uint32_t* utf32) {
    return sbcToUtf32<true>(profile, sbc, utf32);
}

int APS5_VABI sceCesSbcToUtf32le(const uint8_t* profile, uint8_t sbc, uint32_t* utf32) {
    return sbcToUtf32<false>(profile, sbc, utf32);
}

int APS5_VABI sceCesUcs2ToSbc(uint16_t ucs2, const uint8_t* profile, uint8_t* sbc) {
    return scalarToSbc(ucs2, profile, sbc);
}

int APS5_VABI sceCesUtf16ToSbc(const uint16_t* utf16, uint32_t utf16max, uint32_t* utf16Len, const uint8_t* profile, uint8_t* sbc) {
    return utf16ToSbc<NATIVE_BIG_ENDIAN>(utf16, utf16max, utf16Len, profile, sbc);
}

int APS5_VABI sceCesUtf16beToSbc(const uint16_t* utf16, uint32_t utf16max, uint32_t* utf16Len, const uint8_t* profile, uint8_t* sbc) {
    return utf16ToSbc<true>(utf16, utf16max, utf16Len, profile, sbc);
}

int APS5_VABI sceCesUtf16leToSbc(const uint16_t* utf16, uint32_t utf16max, uint32_t* utf16Len, const uint8_t* profile, uint8_t* sbc) {
    return utf16ToSbc<false>(utf16, utf16max, utf16Len, profile, sbc);
}

int APS5_VABI sceCesUtf32ToSbc(uint32_t utf32, const uint8_t* profile, uint8_t* sbc) {
    return scalarToSbc(utf32, profile, sbc);
}

int APS5_VABI sceCesUtf32beToSbc(const uint32_t* utf32, const uint8_t* profile, uint8_t* sbc) {
    return utf32ToSbc<true>(utf32, profile, sbc);
}

int APS5_VABI sceCesUtf32leToSbc(const uint32_t* utf32, const uint8_t* profile, uint8_t* sbc) {
    return utf32ToSbc<false>(utf32, profile, sbc);
}

int APS5_VABI sceCesUtf16StrToMbcsStr(void* context, const uint16_t* utf16, uint32_t utf16max, uint32_t* utf16Len, uint8_t* mbcs, uint32_t mbcsMax, uint32_t* mbcsLen) {
    return convertString(context, utf16, utf16max, utf16Len, mbcs, mbcsMax, mbcsLen, false, unitSource<decodeUtf16<NATIVE_BIG_ENDIAN>, uint16_t>, encodeMbcs);
}

int APS5_VABI sceCesUtf16StrGetMbcsLen(void* context, const uint16_t* utf16, uint32_t utf16max, uint32_t* utf16Len, uint32_t* mbcsLen) {
    return convertString(context, utf16, utf16max, utf16Len, static_cast<uint8_t*>(nullptr), 0, mbcsLen, true, unitSource<decodeUtf16<NATIVE_BIG_ENDIAN>, uint16_t>, encodeMbcs);
}

int APS5_VABI sceCesMbcsStrToUtf16Str(void* context, const uint8_t* mbcs, uint32_t mbcsMax, uint32_t* mbcsLen, uint16_t* utf16, uint32_t utf16max, uint32_t* utf16Len) {
    return convertString(context, mbcs, mbcsMax, mbcsLen, utf16, utf16max, utf16Len, false, decodeMbcs, unitTarget<encodeUtf16, uint16_t>);
}

int APS5_VABI sceCesMbcsStrGetUtf16Len(void* context, const uint8_t* mbcs, uint32_t mbcsMax, uint32_t* mbcsLen, uint32_t* utf16Len) {
    return convertString(context, mbcs, mbcsMax, mbcsLen, static_cast<uint16_t*>(nullptr), 0, utf16Len, true, decodeMbcs, unitTarget<encodeUtf16, uint16_t>);
}

int APS5_VABI sceCesUtf32StrToMbcsStr(void* context, const uint32_t* utf32, uint32_t utf32max, uint32_t* utf32Len, uint8_t* mbcs, uint32_t mbcsMax, uint32_t* mbcsLen) {
    return convertString(context, utf32, utf32max, utf32Len, mbcs, mbcsMax, mbcsLen, false, unitSource<decodeUtf32, uint32_t>, encodeMbcs);
}

int APS5_VABI sceCesUtf32StrGetMbcsLen(void* context, const uint32_t* utf32, uint32_t utf32max, uint32_t* utf32Len, uint32_t* mbcsLen) {
    return convertString(context, utf32, utf32max, utf32Len, static_cast<uint8_t*>(nullptr), 0, mbcsLen, true, unitSource<decodeUtf32, uint32_t>, encodeMbcs);
}

int APS5_VABI sceCesMbcsStrToUtf32Str(void* context, const uint8_t* mbcs, uint32_t mbcsMax, uint32_t* mbcsLen, uint32_t* utf32, uint32_t utf32max, uint32_t* utf32Len) {
    return convertString(context, mbcs, mbcsMax, mbcsLen, utf32, utf32max, utf32Len, false, decodeMbcs, unitTarget<encodeUtf32, uint32_t>);
}

int APS5_VABI sceCesMbcsStrGetUtf32Len(void* context, const uint8_t* mbcs, uint32_t mbcsMax, uint32_t* mbcsLen, uint32_t* utf32Len) {
    return convertString(context, mbcs, mbcsMax, mbcsLen, static_cast<uint32_t*>(nullptr), 0, utf32Len, true, decodeMbcs, unitTarget<encodeUtf32, uint32_t>);
}

void* APS5_VABI sceCesUcsProfileInitEucCnGb2312(void* sheet) {
    if (!sheet) return nullptr;
    std::memcpy(sheet, EUC_CN_TAG, sizeof(EUC_CN_TAG));
    return sheet;
}

void* APS5_VABI sceCesUcsProfileInitGbkCp936(void* sheet) {
    if (!sheet) return nullptr;
    std::memcpy(sheet, GBK_TAG, sizeof(GBK_TAG));
    return sheet;
}

void* APS5_VABI sceCesUcsProfileInitGb18030_2000(void* sheet) {
    if (!sheet) return nullptr;
    std::memcpy(sheet, GB18030_2000_TAG, sizeof(GB18030_2000_TAG));
    return sheet;
}

void* APS5_VABI sceCesUcsProfileInitGb18030_2005(void* sheet) {
    if (!sheet) return nullptr;
    std::memcpy(sheet, GB18030_2005_TAG, sizeof(GB18030_2005_TAG));
    return sheet;
}

int APS5_VABI sceCesEucCnToUcs2(const void* profile, const uint8_t* eucBuffer, uint32_t eucMax, uint32_t* eucSize, uint16_t* ucs2) {
    return profiledToUcs2(isEucCn, profile, eucBuffer, eucMax, eucSize, ucs2);
}

int APS5_VABI sceCesEucCnToUtf8(const void* profile, const uint8_t* eucBuffer, uint32_t eucMax, uint32_t* eucSize, uint8_t* utf8Buffer, uint32_t utf8Max, uint32_t* utf8Length) {
    return profiledToUtf8(isEucCn, profile, eucBuffer, eucMax, eucSize, utf8Buffer, utf8Max, utf8Length);
}

int APS5_VABI sceCesEucCnToUtf16(const void* profile, const uint8_t* eucBuffer, uint32_t eucMax, uint32_t* eucSize, uint16_t* utf16Buffer, uint32_t utf16Max, uint32_t* utf16Length) {
    return profiledToUtf16<NATIVE_BIG_ENDIAN>(isEucCn, profile, eucBuffer, eucMax, eucSize, utf16Buffer, utf16Max, utf16Length);
}

int APS5_VABI sceCesEucCnToUtf16be(const void* profile, const uint8_t* eucBuffer, uint32_t eucMax, uint32_t* eucSize, uint16_t* utf16Buffer, uint32_t utf16Max, uint32_t* utf16Length) {
    return profiledToUtf16<true>(isEucCn, profile, eucBuffer, eucMax, eucSize, utf16Buffer, utf16Max, utf16Length);
}

int APS5_VABI sceCesEucCnToUtf16le(const void* profile, const uint8_t* eucBuffer, uint32_t eucMax, uint32_t* eucSize, uint16_t* utf16Buffer, uint32_t utf16Max, uint32_t* utf16Length) {
    return profiledToUtf16<false>(isEucCn, profile, eucBuffer, eucMax, eucSize, utf16Buffer, utf16Max, utf16Length);
}

int APS5_VABI sceCesEucCnToUtf32(const void* profile, const uint8_t* eucBuffer, uint32_t eucMax, uint32_t* eucSize, uint32_t* utf32) {
    return profiledToUtf32<NATIVE_BIG_ENDIAN>(isEucCn, profile, eucBuffer, eucMax, eucSize, utf32);
}

int APS5_VABI sceCesEucCnToUtf32be(const void* profile, const uint8_t* eucBuffer, uint32_t eucMax, uint32_t* eucSize, uint32_t* utf32) {
    return profiledToUtf32<true>(isEucCn, profile, eucBuffer, eucMax, eucSize, utf32);
}

int APS5_VABI sceCesEucCnToUtf32le(const void* profile, const uint8_t* eucBuffer, uint32_t eucMax, uint32_t* eucSize, uint32_t* utf32) {
    return profiledToUtf32<false>(isEucCn, profile, eucBuffer, eucMax, eucSize, utf32);
}

int APS5_VABI sceCesUcs2ToEucCn(uint16_t ucs2, const void* profile, uint8_t* eucBuffer, uint32_t eucMax, uint32_t* eucSize) {
    return scalarToProfiled(isEucCn, ucs2, profile, eucBuffer, eucMax, eucSize);
}

int APS5_VABI sceCesUtf8ToEucCn(const uint8_t* utf8Buffer, uint32_t utf8Max, uint32_t* utf8Length, const void* profile, uint8_t* eucBuffer, uint32_t eucMax, uint32_t* eucSize) {
    return bufferToProfiled(isEucCn, utf8Buffer, utf8Max, utf8Length, profile, eucBuffer, eucMax, eucSize, decodeScalarUtf8);
}

int APS5_VABI sceCesUtf16ToEucCn(const uint16_t* utf16Buffer, uint32_t utf16Max, uint32_t* utf16Length, const void* profile, uint8_t* eucBuffer, uint32_t eucMax, uint32_t* eucSize) {
    return bufferToProfiled(isEucCn, utf16Buffer, utf16Max, utf16Length, profile, eucBuffer, eucMax, eucSize, decodeUtf16<NATIVE_BIG_ENDIAN>);
}

int APS5_VABI sceCesUtf16beToEucCn(const uint16_t* utf16Buffer, uint32_t utf16Max, uint32_t* utf16Length, const void* profile, uint8_t* eucBuffer, uint32_t eucMax, uint32_t* eucSize) {
    return bufferToProfiled(isEucCn, utf16Buffer, utf16Max, utf16Length, profile, eucBuffer, eucMax, eucSize, decodeUtf16<true>);
}

int APS5_VABI sceCesUtf16leToEucCn(const uint16_t* utf16Buffer, uint32_t utf16Max, uint32_t* utf16Length, const void* profile, uint8_t* eucBuffer, uint32_t eucMax, uint32_t* eucSize) {
    return bufferToProfiled(isEucCn, utf16Buffer, utf16Max, utf16Length, profile, eucBuffer, eucMax, eucSize, decodeUtf16<false>);
}

int APS5_VABI sceCesUtf32ToEucCn(uint32_t utf32, const void* profile, uint8_t* eucBuffer, uint32_t eucMax, uint32_t* eucSize) {
    return scalarToProfiled(isEucCn, utf32, profile, eucBuffer, eucMax, eucSize);
}

int APS5_VABI sceCesUtf32beToEucCn(const uint32_t* utf32, const void* profile, uint8_t* eucBuffer, uint32_t eucMax, uint32_t* eucSize) {
    return utf32ToProfiled<true>(isEucCn, utf32, profile, eucBuffer, eucMax, eucSize);
}

int APS5_VABI sceCesUtf32leToEucCn(const uint32_t* utf32, const void* profile, uint8_t* eucBuffer, uint32_t eucMax, uint32_t* eucSize) {
    return utf32ToProfiled<false>(isEucCn, utf32, profile, eucBuffer, eucMax, eucSize);
}

int APS5_VABI sceCesGbToUcs2(const void* profile, const uint8_t* gbBuffer, uint32_t gbMax, uint32_t* gbSize, uint16_t* ucs2) {
    return profiledToUcs2(isGb, profile, gbBuffer, gbMax, gbSize, ucs2);
}

int APS5_VABI sceCesGbToUtf8(const void* profile, const uint8_t* gbBuffer, uint32_t gbMax, uint32_t* gbSize, uint8_t* utf8Buffer, uint32_t utf8Max, uint32_t* utf8Length) {
    return profiledToUtf8(isGb, profile, gbBuffer, gbMax, gbSize, utf8Buffer, utf8Max, utf8Length);
}

int APS5_VABI sceCesGbToUtf16(const void* profile, const uint8_t* gbBuffer, uint32_t gbMax, uint32_t* gbSize, uint16_t* utf16Buffer, uint32_t utf16Max, uint32_t* utf16Length) {
    return profiledToUtf16<NATIVE_BIG_ENDIAN>(isGb, profile, gbBuffer, gbMax, gbSize, utf16Buffer, utf16Max, utf16Length);
}

int APS5_VABI sceCesGbToUtf16be(const void* profile, const uint8_t* gbBuffer, uint32_t gbMax, uint32_t* gbSize, uint16_t* utf16Buffer, uint32_t utf16Max, uint32_t* utf16Length) {
    return profiledToUtf16<true>(isGb, profile, gbBuffer, gbMax, gbSize, utf16Buffer, utf16Max, utf16Length);
}

int APS5_VABI sceCesGbToUtf16le(const void* profile, const uint8_t* gbBuffer, uint32_t gbMax, uint32_t* gbSize, uint16_t* utf16Buffer, uint32_t utf16Max, uint32_t* utf16Length) {
    return profiledToUtf16<false>(isGb, profile, gbBuffer, gbMax, gbSize, utf16Buffer, utf16Max, utf16Length);
}

int APS5_VABI sceCesGbToUtf32(const void* profile, const uint8_t* gbBuffer, uint32_t gbMax, uint32_t* gbSize, uint32_t* utf32) {
    return profiledToUtf32<NATIVE_BIG_ENDIAN>(isGb, profile, gbBuffer, gbMax, gbSize, utf32);
}

int APS5_VABI sceCesGbToUtf32be(const void* profile, const uint8_t* gbBuffer, uint32_t gbMax, uint32_t* gbSize, uint32_t* utf32) {
    return profiledToUtf32<true>(isGb, profile, gbBuffer, gbMax, gbSize, utf32);
}

int APS5_VABI sceCesGbToUtf32le(const void* profile, const uint8_t* gbBuffer, uint32_t gbMax, uint32_t* gbSize, uint32_t* utf32) {
    return profiledToUtf32<false>(isGb, profile, gbBuffer, gbMax, gbSize, utf32);
}

int APS5_VABI sceCesUcs2ToGb(uint16_t ucs2, const void* profile, uint8_t* gbBuffer, uint32_t gbMax, uint32_t* gbSize) {
    return scalarToProfiled(isGb, ucs2, profile, gbBuffer, gbMax, gbSize);
}

int APS5_VABI sceCesUtf8ToGb(const uint8_t* utf8Buffer, uint32_t utf8Max, uint32_t* utf8Length, const void* profile, uint8_t* gbBuffer, uint32_t gbMax, uint32_t* gbSize) {
    return bufferToProfiled(isGb, utf8Buffer, utf8Max, utf8Length, profile, gbBuffer, gbMax, gbSize, decodeScalarUtf8);
}

int APS5_VABI sceCesUtf16ToGb(const uint16_t* utf16Buffer, uint32_t utf16Max, uint32_t* utf16Length, const void* profile, uint8_t* gbBuffer, uint32_t gbMax, uint32_t* gbSize) {
    return bufferToProfiled(isGb, utf16Buffer, utf16Max, utf16Length, profile, gbBuffer, gbMax, gbSize, decodeUtf16<NATIVE_BIG_ENDIAN>);
}

int APS5_VABI sceCesUtf16beToGb(const uint16_t* utf16Buffer, uint32_t utf16Max, uint32_t* utf16Length, const void* profile, uint8_t* gbBuffer, uint32_t gbMax, uint32_t* gbSize) {
    return bufferToProfiled(isGb, utf16Buffer, utf16Max, utf16Length, profile, gbBuffer, gbMax, gbSize, decodeUtf16<true>);
}

int APS5_VABI sceCesUtf16leToGb(const uint16_t* utf16Buffer, uint32_t utf16Max, uint32_t* utf16Length, const void* profile, uint8_t* gbBuffer, uint32_t gbMax, uint32_t* gbSize) {
    return bufferToProfiled(isGb, utf16Buffer, utf16Max, utf16Length, profile, gbBuffer, gbMax, gbSize, decodeUtf16<false>);
}

int APS5_VABI sceCesUtf32ToGb(uint32_t utf32, const void* profile, uint8_t* gbBuffer, uint32_t gbMax, uint32_t* gbSize) {
    return scalarToProfiled(isGb, utf32, profile, gbBuffer, gbMax, gbSize);
}

int APS5_VABI sceCesUtf32beToGb(const uint32_t* utf32, const void* profile, uint8_t* gbBuffer, uint32_t gbMax, uint32_t* gbSize) {
    return utf32ToProfiled<true>(isGb, utf32, profile, gbBuffer, gbMax, gbSize);
}

int APS5_VABI sceCesUtf32leToGb(const uint32_t* utf32, const void* profile, uint8_t* gbBuffer, uint32_t gbMax, uint32_t* gbSize) {
    return utf32ToProfiled<false>(isGb, utf32, profile, gbBuffer, gbMax, gbSize);
}

int APS5_VABI sceCesEucKrToUcs2(const void* profile, const uint8_t* eucBuffer, uint32_t eucMax, uint32_t* eucSize, uint16_t* ucs2) {
    return profiledToUcs2(isEucKr, profile, eucBuffer, eucMax, eucSize, ucs2);
}

int APS5_VABI sceCesEucKrToUtf16(const void* profile, const uint8_t* eucBuffer, uint32_t eucMax, uint32_t* eucSize, uint16_t* utf16Buffer, uint32_t utf16Max, uint32_t* utf16Length) {
    return profiledToUtf16<NATIVE_BIG_ENDIAN>(isEucKr, profile, eucBuffer, eucMax, eucSize, utf16Buffer, utf16Max, utf16Length);
}

int APS5_VABI sceCesEucKrToUtf16be(const void* profile, const uint8_t* eucBuffer, uint32_t eucMax, uint32_t* eucSize, uint16_t* utf16Buffer, uint32_t utf16Max, uint32_t* utf16Length) {
    return profiledToUtf16<true>(isEucKr, profile, eucBuffer, eucMax, eucSize, utf16Buffer, utf16Max, utf16Length);
}

int APS5_VABI sceCesEucKrToUtf16le(const void* profile, const uint8_t* eucBuffer, uint32_t eucMax, uint32_t* eucSize, uint16_t* utf16Buffer, uint32_t utf16Max, uint32_t* utf16Length) {
    return profiledToUtf16<false>(isEucKr, profile, eucBuffer, eucMax, eucSize, utf16Buffer, utf16Max, utf16Length);
}

int APS5_VABI sceCesEucKrToUtf32(const void* profile, const uint8_t* eucBuffer, uint32_t eucMax, uint32_t* eucSize, uint32_t* utf32) {
    return profiledToUtf32<NATIVE_BIG_ENDIAN>(isEucKr, profile, eucBuffer, eucMax, eucSize, utf32);
}

int APS5_VABI sceCesEucKrToUtf32be(const void* profile, const uint8_t* eucBuffer, uint32_t eucMax, uint32_t* eucSize, uint32_t* utf32) {
    return profiledToUtf32<true>(isEucKr, profile, eucBuffer, eucMax, eucSize, utf32);
}

int APS5_VABI sceCesEucKrToUtf32le(const void* profile, const uint8_t* eucBuffer, uint32_t eucMax, uint32_t* eucSize, uint32_t* utf32) {
    return profiledToUtf32<false>(isEucKr, profile, eucBuffer, eucMax, eucSize, utf32);
}

int APS5_VABI sceCesUhcToUcs2(const void* profile, const uint8_t* uhcBuffer, uint32_t uhcMax, uint32_t* uhcSize, uint16_t* ucs2) {
    return profiledToUcs2(isUhc, profile, uhcBuffer, uhcMax, uhcSize, ucs2);
}

int APS5_VABI sceCesUhcToUtf16(const void* profile, const uint8_t* uhcBuffer, uint32_t uhcMax, uint32_t* uhcSize, uint16_t* utf16Buffer, uint32_t utf16Max, uint32_t* utf16Length) {
    return profiledToUtf16<NATIVE_BIG_ENDIAN>(isUhc, profile, uhcBuffer, uhcMax, uhcSize, utf16Buffer, utf16Max, utf16Length);
}

int APS5_VABI sceCesUhcToUtf16be(const void* profile, const uint8_t* uhcBuffer, uint32_t uhcMax, uint32_t* uhcSize, uint16_t* utf16Buffer, uint32_t utf16Max, uint32_t* utf16Length) {
    return profiledToUtf16<true>(isUhc, profile, uhcBuffer, uhcMax, uhcSize, utf16Buffer, utf16Max, utf16Length);
}

int APS5_VABI sceCesUhcToUtf16le(const void* profile, const uint8_t* uhcBuffer, uint32_t uhcMax, uint32_t* uhcSize, uint16_t* utf16Buffer, uint32_t utf16Max, uint32_t* utf16Length) {
    return profiledToUtf16<false>(isUhc, profile, uhcBuffer, uhcMax, uhcSize, utf16Buffer, utf16Max, utf16Length);
}

int APS5_VABI sceCesUhcToUtf32(const void* profile, const uint8_t* uhcBuffer, uint32_t uhcMax, uint32_t* uhcSize, uint32_t* utf32) {
    return profiledToUtf32<NATIVE_BIG_ENDIAN>(isUhc, profile, uhcBuffer, uhcMax, uhcSize, utf32);
}

int APS5_VABI sceCesUhcToUtf32be(const void* profile, const uint8_t* uhcBuffer, uint32_t uhcMax, uint32_t* uhcSize, uint32_t* utf32) {
    return profiledToUtf32<true>(isUhc, profile, uhcBuffer, uhcMax, uhcSize, utf32);
}

int APS5_VABI sceCesUhcToUtf32le(const void* profile, const uint8_t* uhcBuffer, uint32_t uhcMax, uint32_t* uhcSize, uint32_t* utf32) {
    return profiledToUtf32<false>(isUhc, profile, uhcBuffer, uhcMax, uhcSize, utf32);
}

int APS5_VABI sceCesBig5ToUcs2(const void* profile, const uint8_t* big5Buffer, uint32_t big5Max, uint32_t* big5Size, uint16_t* ucs2) {
    return profiledToUcs2(isBig5, profile, big5Buffer, big5Max, big5Size, ucs2);
}

int APS5_VABI sceCesBig5ToUtf16(const void* profile, const uint8_t* big5Buffer, uint32_t big5Max, uint32_t* big5Size, uint16_t* utf16Buffer, uint32_t utf16Max, uint32_t* utf16Length) {
    return profiledToUtf16<NATIVE_BIG_ENDIAN>(isBig5, profile, big5Buffer, big5Max, big5Size, utf16Buffer, utf16Max, utf16Length);
}

int APS5_VABI sceCesBig5ToUtf16be(const void* profile, const uint8_t* big5Buffer, uint32_t big5Max, uint32_t* big5Size, uint16_t* utf16Buffer, uint32_t utf16Max, uint32_t* utf16Length) {
    return profiledToUtf16<true>(isBig5, profile, big5Buffer, big5Max, big5Size, utf16Buffer, utf16Max, utf16Length);
}

int APS5_VABI sceCesBig5ToUtf16le(const void* profile, const uint8_t* big5Buffer, uint32_t big5Max, uint32_t* big5Size, uint16_t* utf16Buffer, uint32_t utf16Max, uint32_t* utf16Length) {
    return profiledToUtf16<false>(isBig5, profile, big5Buffer, big5Max, big5Size, utf16Buffer, utf16Max, utf16Length);
}

int APS5_VABI sceCesBig5ToUtf32(const void* profile, const uint8_t* big5Buffer, uint32_t big5Max, uint32_t* big5Size, uint32_t* utf32) {
    return profiledToUtf32<NATIVE_BIG_ENDIAN>(isBig5, profile, big5Buffer, big5Max, big5Size, utf32);
}

int APS5_VABI sceCesBig5ToUtf32be(const void* profile, const uint8_t* big5Buffer, uint32_t big5Max, uint32_t* big5Size, uint32_t* utf32) {
    return profiledToUtf32<true>(isBig5, profile, big5Buffer, big5Max, big5Size, utf32);
}

int APS5_VABI sceCesBig5ToUtf32le(const void* profile, const uint8_t* big5Buffer, uint32_t big5Max, uint32_t* big5Size, uint32_t* utf32) {
    return profiledToUtf32<false>(isBig5, profile, big5Buffer, big5Max, big5Size, utf32);
}

int APS5_VABI sceCesUcs2ToEucKr(uint16_t ucs2, const void* profile, uint8_t* eucBuffer, uint32_t eucMax, uint32_t* eucSize) {
    return scalarToProfiled(isEucKr, ucs2, profile, eucBuffer, eucMax, eucSize);
}

int APS5_VABI sceCesUtf8ToEucKr(const uint8_t* utf8Buffer, uint32_t utf8Max, uint32_t* utf8Length, const void* profile, uint8_t* eucBuffer, uint32_t eucMax, uint32_t* eucSize) {
    return bufferToProfiled(isEucKr, utf8Buffer, utf8Max, utf8Length, profile, eucBuffer, eucMax, eucSize, decodeScalarUtf8);
}

int APS5_VABI sceCesUtf16ToEucKr(const uint16_t* utf16Buffer, uint32_t utf16Max, uint32_t* utf16Length, const void* profile, uint8_t* eucBuffer, uint32_t eucMax, uint32_t* eucSize) {
    return bufferToProfiled(isEucKr, utf16Buffer, utf16Max, utf16Length, profile, eucBuffer, eucMax, eucSize, decodeUtf16<NATIVE_BIG_ENDIAN>);
}

int APS5_VABI sceCesUtf16beToEucKr(const uint16_t* utf16Buffer, uint32_t utf16Max, uint32_t* utf16Length, const void* profile, uint8_t* eucBuffer, uint32_t eucMax, uint32_t* eucSize) {
    return bufferToProfiled(isEucKr, utf16Buffer, utf16Max, utf16Length, profile, eucBuffer, eucMax, eucSize, decodeUtf16<true>);
}

int APS5_VABI sceCesUtf16leToEucKr(const uint16_t* utf16Buffer, uint32_t utf16Max, uint32_t* utf16Length, const void* profile, uint8_t* eucBuffer, uint32_t eucMax, uint32_t* eucSize) {
    return bufferToProfiled(isEucKr, utf16Buffer, utf16Max, utf16Length, profile, eucBuffer, eucMax, eucSize, decodeUtf16<false>);
}

int APS5_VABI sceCesUtf32ToEucKr(uint32_t utf32, const void* profile, uint8_t* eucBuffer, uint32_t eucMax, uint32_t* eucSize) {
    return scalarToProfiled(isEucKr, utf32, profile, eucBuffer, eucMax, eucSize);
}

int APS5_VABI sceCesUtf32beToEucKr(const uint32_t* utf32, const void* profile, uint8_t* eucBuffer, uint32_t eucMax, uint32_t* eucSize) {
    return utf32ToProfiled<true>(isEucKr, utf32, profile, eucBuffer, eucMax, eucSize);
}

int APS5_VABI sceCesUtf32leToEucKr(const uint32_t* utf32, const void* profile, uint8_t* eucBuffer, uint32_t eucMax, uint32_t* eucSize) {
    return utf32ToProfiled<false>(isEucKr, utf32, profile, eucBuffer, eucMax, eucSize);
}

int APS5_VABI sceCesUcs2ToUhc(uint16_t ucs2, const void* profile, uint8_t* uhcBuffer, uint32_t uhcMax, uint32_t* uhcSize) {
    return scalarToProfiled(isUhc, ucs2, profile, uhcBuffer, uhcMax, uhcSize);
}

int APS5_VABI sceCesUtf8ToUhc(const uint8_t* utf8Buffer, uint32_t utf8Max, uint32_t* utf8Length, const void* profile, uint8_t* uhcBuffer, uint32_t uhcMax, uint32_t* uhcSize) {
    return bufferToProfiled(isUhc, utf8Buffer, utf8Max, utf8Length, profile, uhcBuffer, uhcMax, uhcSize, decodeScalarUtf8);
}

int APS5_VABI sceCesUtf16ToUhc(const uint16_t* utf16Buffer, uint32_t utf16Max, uint32_t* utf16Length, const void* profile, uint8_t* uhcBuffer, uint32_t uhcMax, uint32_t* uhcSize) {
    return bufferToProfiled(isUhc, utf16Buffer, utf16Max, utf16Length, profile, uhcBuffer, uhcMax, uhcSize, decodeUtf16<NATIVE_BIG_ENDIAN>);
}

int APS5_VABI sceCesUtf16beToUhc(const uint16_t* utf16Buffer, uint32_t utf16Max, uint32_t* utf16Length, const void* profile, uint8_t* uhcBuffer, uint32_t uhcMax, uint32_t* uhcSize) {
    return bufferToProfiled(isUhc, utf16Buffer, utf16Max, utf16Length, profile, uhcBuffer, uhcMax, uhcSize, decodeUtf16<true>);
}

int APS5_VABI sceCesUtf16leToUhc(const uint16_t* utf16Buffer, uint32_t utf16Max, uint32_t* utf16Length, const void* profile, uint8_t* uhcBuffer, uint32_t uhcMax, uint32_t* uhcSize) {
    return bufferToProfiled(isUhc, utf16Buffer, utf16Max, utf16Length, profile, uhcBuffer, uhcMax, uhcSize, decodeUtf16<false>);
}

int APS5_VABI sceCesUtf32ToUhc(uint32_t utf32, const void* profile, uint8_t* uhcBuffer, uint32_t uhcMax, uint32_t* uhcSize) {
    return scalarToProfiled(isUhc, utf32, profile, uhcBuffer, uhcMax, uhcSize);
}

int APS5_VABI sceCesUtf32beToUhc(const uint32_t* utf32, const void* profile, uint8_t* uhcBuffer, uint32_t uhcMax, uint32_t* uhcSize) {
    return utf32ToProfiled<true>(isUhc, utf32, profile, uhcBuffer, uhcMax, uhcSize);
}

int APS5_VABI sceCesUtf32leToUhc(const uint32_t* utf32, const void* profile, uint8_t* uhcBuffer, uint32_t uhcMax, uint32_t* uhcSize) {
    return utf32ToProfiled<false>(isUhc, utf32, profile, uhcBuffer, uhcMax, uhcSize);
}

int APS5_VABI sceCesUcs2ToBig5(uint16_t ucs2, const void* profile, uint8_t* big5Buffer, uint32_t big5Max, uint32_t* big5Size) {
    return scalarToProfiled(isBig5, ucs2, profile, big5Buffer, big5Max, big5Size);
}

int APS5_VABI sceCesUtf8ToBig5(const uint8_t* utf8Buffer, uint32_t utf8Max, uint32_t* utf8Length, const void* profile, uint8_t* big5Buffer, uint32_t big5Max, uint32_t* big5Size) {
    return bufferToProfiled(isBig5, utf8Buffer, utf8Max, utf8Length, profile, big5Buffer, big5Max, big5Size, decodeScalarUtf8);
}

int APS5_VABI sceCesUtf16ToBig5(const uint16_t* utf16Buffer, uint32_t utf16Max, uint32_t* utf16Length, const void* profile, uint8_t* big5Buffer, uint32_t big5Max, uint32_t* big5Size) {
    return bufferToProfiled(isBig5, utf16Buffer, utf16Max, utf16Length, profile, big5Buffer, big5Max, big5Size, decodeUtf16<NATIVE_BIG_ENDIAN>);
}

int APS5_VABI sceCesUtf16beToBig5(const uint16_t* utf16Buffer, uint32_t utf16Max, uint32_t* utf16Length, const void* profile, uint8_t* big5Buffer, uint32_t big5Max, uint32_t* big5Size) {
    return bufferToProfiled(isBig5, utf16Buffer, utf16Max, utf16Length, profile, big5Buffer, big5Max, big5Size, decodeUtf16<true>);
}

int APS5_VABI sceCesUtf16leToBig5(const uint16_t* utf16Buffer, uint32_t utf16Max, uint32_t* utf16Length, const void* profile, uint8_t* big5Buffer, uint32_t big5Max, uint32_t* big5Size) {
    return bufferToProfiled(isBig5, utf16Buffer, utf16Max, utf16Length, profile, big5Buffer, big5Max, big5Size, decodeUtf16<false>);
}

int APS5_VABI sceCesUtf32ToBig5(uint32_t utf32, const void* profile, uint8_t* big5Buffer, uint32_t big5Max, uint32_t* big5Size) {
    return scalarToProfiled(isBig5, utf32, profile, big5Buffer, big5Max, big5Size);
}

int APS5_VABI sceCesUtf32beToBig5(const uint32_t* utf32, const void* profile, uint8_t* big5Buffer, uint32_t big5Max, uint32_t* big5Size) {
    return utf32ToProfiled<true>(isBig5, utf32, profile, big5Buffer, big5Max, big5Size);
}

int APS5_VABI sceCesUtf32leToBig5(const uint32_t* utf32, const void* profile, uint8_t* big5Buffer, uint32_t big5Max, uint32_t* big5Size) {
    return utf32ToProfiled<false>(isBig5, utf32, profile, big5Buffer, big5Max, big5Size);
}


void* APS5_VABI sceCesUcsProfileInitEucJis2004(void* sheet) {
    if (!sheet) return nullptr;
    std::memcpy(sheet, EUC_JIS2004_TAG, sizeof(EUC_JIS2004_TAG));
    return sheet;
}

void* APS5_VABI sceCesUcsProfileInitSJis2004X0213(void* sheet) {
    if (!sheet) return nullptr;
    std::memcpy(sheet, SJIS2004_TAG, sizeof(SJIS2004_TAG));
    return sheet;
}

}
