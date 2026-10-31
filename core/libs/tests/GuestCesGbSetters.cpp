#include "prx/libSceCesCs/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <climits>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

namespace {

constexpr int invalidParameter = static_cast<int>(0x805C0001);
constexpr int invalidProfile = static_cast<int>(0x805C0004);
constexpr int unassignedCode = static_cast<int>(0x805C0020);
constexpr std::uint64_t fnvBasis = 0xCBF29CE484222325ULL;
constexpr std::uint32_t poison = 0xDEADBEEF;
constexpr std::size_t sheetSize = 64;

void Require(bool value) { if (!value) std::abort(); }

template<typename Action>
void RequireThrows(Action action) {
    try { action(); } catch (const std::runtime_error&) { return; }
    std::abort();
}

std::uint64_t Fnv(std::uint64_t hash, std::uint32_t value) {
    for (int i = 0; i < 4; ++i) {
        hash ^= value >> (8 * i) & 0xFF;
        hash *= 0x100000001B3ULL;
    }
    return hash;
}

struct GbSample {
    std::uint32_t length;
    std::uint32_t max;
    std::uint8_t bytes[4];
};

struct UcsSample {
    std::uint32_t code;
    std::uint32_t max;
};

constexpr GbSample gbSamples[] = {
    {1, 1, {0x41}}, {1, 1, {0x80}}, {1, 1, {0xFF}}, {2, 2, {0xA1, 0xA1}}, {2, 2, {0xB0, 0xA1}}, {2, 2, {0xA2, 0xE3}}, {2, 2, {0xA8, 0xBC}},
    {2, 2, {0xA6, 0xD9}}, {2, 2, {0xFE, 0x51}}, {2, 2, {0xA1, 0x40}}, {2, 2, {0xA7, 0xA0}}, {2, 2, {0xAA, 0xA1}}, {2, 2, {0xAF, 0xFE}},
    {2, 2, {0xF8, 0xA1}}, {2, 2, {0xFE, 0xFE}}, {4, 4, {0x81, 0x30, 0x81, 0x30}}, {4, 4, {0x81, 0x35, 0xF4, 0x37}},
    {4, 4, {0x84, 0x31, 0xA4, 0x39}}, {4, 4, {0x84, 0x31, 0xA5, 0x30}}, {4, 4, {0x90, 0x30, 0x81, 0x30}}, {4, 4, {0xE3, 0x32, 0x9A, 0x35}},
    {4, 4, {0xE3, 0x32, 0x9A, 0x36}}, {4, 4, {0xFE, 0x39, 0xFE, 0x39}}, {1, 1, {0x81}}, {2, 2, {0x81, 0x30}}, {3, 3, {0x81, 0x30, 0x81}},
    {4, 2, {0x81, 0x30, 0x81, 0x30}}, {2, 2, {0x81, 0x7F}},
};

constexpr UcsSample ucsSamples[] = {
    {0x0, 4}, {0x41, 4}, {0x7F, 4}, {0x80, 4}, {0xA4, 4}, {0x20AC, 4}, {0x3000, 4}, {0x554A, 4}, {0xE000, 4}, {0xE233, 4}, {0xE234, 4},
    {0xE4C5, 4}, {0xE4C6, 4}, {0xE5E5, 4}, {0xE7C7, 4}, {0x1E3F, 4}, {0xFE10, 4}, {0xE816, 4}, {0xD800, 4}, {0xFFFF, 4}, {0x10000, 4},
    {0x10FFFF, 4}, {0x41, 0}, {0x554A, 1}, {0x10000, 2}, {0x10000, 3},
};

constexpr int setterValues[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 15, 16, 0x100, 0x10000, INT_MAX, -1, -2, INT_MIN};

enum Profile { Gb2000, Gb2005, Gbk, EucCn, ProfileCount };
enum Setter { Range, Uda, None };

struct Measured {
    Profile profile;
    Setter setter;
    int value;
    std::uint64_t decode;
    std::uint64_t encode;
};

constexpr Measured measured[] = {
    {Gb2000, None, 0, 0x3FF3C49591642ABBULL, 0x9D71B4F19CD60DBAULL},
    {Gb2000, Range, 0, 0xB586601B8A08DAA0ULL, 0x53C0982A53A2E2A6ULL},
    {Gb2000, Range, 2, 0xB586601B8A08DAA0ULL, 0x53C0982A53A2E2A6ULL},
    {Gb2000, Range, 3, 0x3FF3C49591642ABBULL, 0x9D71B4F19CD60DBAULL},
    {Gb2000, Range, 5, 0xF9AED1C4CA530CEEULL, 0xEDDA3C76EC1BDFDFULL},
    {Gb2000, Range, 7, 0x85C62451AA4E3358ULL, 0x08BF9AD297B11B37ULL},
    {Gb2000, Uda, 0, 0xCF32FDA36D354C73ULL, 0xEF6FEDD11E33BBF7ULL},
    {Gb2005, None, 0, 0x4376CA89BAD45BB7ULL, 0xF89C4EB9721D44B2ULL},
    {Gb2005, Range, 0, 0x2B0C4C564393B50FULL, 0x0223BACB039C7B96ULL},
    {Gb2005, Range, 1, 0x4376CA89BAD45BB7ULL, 0xF89C4EB9721D44B2ULL},
    {Gb2005, Range, 2, 0x2B0C4C564393B50FULL, 0x0223BACB039C7B96ULL},
    {Gb2005, Range, 5, 0xF34557B2DD2C67D2ULL, 0x9B139236380DC3A7ULL},
    {Gb2005, Range, 7, 0x21380769CF89FC94ULL, 0x3B13D441490F6CBFULL},
    {Gb2005, Uda, 0, 0xB240C06283C4BD83ULL, 0x01B124A571F808EFULL},
    {Gbk, None, 0, 0x00525BE32B83CA33ULL, 0x56AD86AE6D820329ULL},
    {Gbk, Uda, 1, 0x86E1BFB5FD199896ULL, 0x56AD86AE6D820329ULL},
};

void* (*const inits[ProfileCount])(void*) = {
    sceCesUcsProfileInitGb18030_2000, sceCesUcsProfileInitGb18030_2005, sceCesUcsProfileInitGbkCp936, sceCesUcsProfileInitEucCnGb2312,
};

int Set(Setter setter, void* profile, int value) {
    return setter == Range ? sceCesGbUcsProfileSet4ByteCharRange(profile, value) : sceCesGbUcsProfileSetUdaMapping(profile, value);
}

int ExpectedSet(Setter setter, Profile profile, int value) {
    if (profile == EucCn || (setter == Range && profile == Gbk)) return invalidProfile;
    const bool valid = setter == Uda ? value == 0 || value == 1 : value == 0 || value == 1 || value == 2 || value == 3 || value == 5 || value == 7;
    return valid ? 0 : invalidParameter;
}

int DefaultValue(Setter setter, Profile profile) {
    if (setter == Uda) return profile == Gbk ? 0 : 1;
    return profile == Gb2000 ? 1 : 3;
}

void Prepare(std::uint8_t (&sheet)[sheetSize], Profile profile, Setter setter, int value) {
    std::memset(sheet, 0, sheetSize);
    Require(inits[profile](sheet) == sheet);
    if (setter != None) Require(Set(setter, sheet, value) == 0);
}

int Decode(const void* profile, const GbSample& sample, std::uint32_t& size, std::uint32_t& out) {
    std::uint8_t input[8];
    std::memset(input, 0xA5, sizeof(input));
    std::memcpy(input, sample.bytes, sample.length);
    size = poison;
    out = poison;
    return sceCesGbToUtf32(profile, input, sample.max, &size, &out);
}

int Encode(const void* profile, const UcsSample& sample, std::uint32_t& size, std::uint32_t& out) {
    std::uint8_t output[8];
    std::memset(output, 0xA5, sizeof(output));
    size = poison;
    const int result = sceCesUtf32ToGb(sample.code, profile, output, sample.max, &size);
    for (std::uint32_t i = sample.max; i < sizeof(output); ++i) Require(output[i] == 0xA5);
    out = output[0] | output[1] << 8 | output[2] << 16 | static_cast<std::uint32_t>(output[3]) << 24;
    return result;
}

std::uint64_t DecodeHash(const void* profile) {
    std::uint64_t hash = fnvBasis;
    for (const GbSample& sample : gbSamples) {
        std::uint32_t size = 0;
        std::uint32_t out = 0;
        const int result = Decode(profile, sample, size, out);
        hash = Fnv(Fnv(Fnv(hash, static_cast<std::uint32_t>(result)), size), out);
    }
    return hash;
}

std::uint64_t EncodeHash(const void* profile) {
    std::uint64_t hash = fnvBasis;
    for (const UcsSample& sample : ucsSamples) {
        std::uint32_t size = 0;
        std::uint32_t out = 0;
        const int result = Encode(profile, sample, size, out);
        hash = Fnv(Fnv(Fnv(hash, static_cast<std::uint32_t>(result)), size), out);
    }
    return hash;
}

void RequireDecode(const void* profile, std::uint32_t bytes, int result, std::uint32_t size, std::uint32_t code) {
    GbSample sample{4, 4, {static_cast<std::uint8_t>(bytes >> 24), static_cast<std::uint8_t>(bytes >> 16), static_cast<std::uint8_t>(bytes >> 8), static_cast<std::uint8_t>(bytes)}};
    if (bytes <= 0xFFFF) sample = {2, 2, {static_cast<std::uint8_t>(bytes >> 8), static_cast<std::uint8_t>(bytes)}};
    std::uint32_t gotSize = 0;
    std::uint32_t out = 0;
    Require(Decode(profile, sample, gotSize, out) == result && gotSize == size && out == code);
}

void RequireEncode(const void* profile, std::uint32_t code, int result, std::uint32_t size) {
    std::uint32_t gotSize = 0;
    std::uint32_t out = 0;
    Require(Encode(profile, {code, 4}, gotSize, out) == result && gotSize == size);
}

}

int main() {
    alignas(8) static std::uint8_t base[ProfileCount][sheetSize] = {};
    for (int profile = 0; profile < ProfileCount; ++profile) Prepare(base[profile], static_cast<Profile>(profile), None, 0);

    for (Setter setter : {Range, Uda}) {
        for (int profile = 0; profile < ProfileCount; ++profile) {
            for (int value : setterValues) {
                alignas(8) std::uint8_t sheet[sheetSize];
                std::memcpy(sheet, base[profile], sheetSize);
                const int expected = ExpectedSet(setter, static_cast<Profile>(profile), value);
                Require(Set(setter, sheet, value) == expected);
                const bool changed = expected == 0 && value != DefaultValue(setter, static_cast<Profile>(profile));
                Require((std::memcmp(sheet, base[profile], sheetSize) != 0) == changed);
            }
        }
        Require(Set(setter, nullptr, 0) == invalidProfile);
        Require(Set(setter, nullptr, 1) == invalidProfile);
    }

    for (const Measured& entry : measured) {
        alignas(8) std::uint8_t sheet[sheetSize];
        Prepare(sheet, entry.profile, entry.setter, entry.value);
        Require(DecodeHash(sheet) == entry.decode);
        Require(EncodeHash(sheet) == entry.encode);
    }

    alignas(8) std::uint8_t sheet[sheetSize];
    Prepare(sheet, Gb2000, Range, 0);
    RequireDecode(sheet, 0x81308130, unassignedCode, 4, 0);
    RequireDecode(sheet, 0xA1A1, 0, 2, 0x3000);
    RequireEncode(sheet, 0x80, unassignedCode, 0);
    Require(sceCesGbUcsProfileSet4ByteCharRange(sheet, 1) == 0 && std::memcmp(sheet, base[Gb2000], sheetSize) == 0);
    RequireDecode(sheet, 0x81308130, 0, 4, 0x80);

    Prepare(sheet, Gb2000, Range, 5);
    RequireDecode(sheet, 0x90308130, 0, 4, 0x10000);
    RequireDecode(sheet, 0xE3329A35, unassignedCode, 4, 0);
    RequireEncode(sheet, 0x10000, 0, 4);
    RequireThrows([&] { RequireDecode(sheet, 0x90308131, 0, 4, 0x10001); });
    RequireThrows([&] { RequireEncode(sheet, 0x20000, 0, 4); });

    Prepare(sheet, Gb2005, Range, 7);
    RequireDecode(sheet, 0xE3329A35, 0, 4, 0x10FFFF);
    RequireDecode(sheet, 0x8135F437, 0, 4, 0xE7C7);
    RequireThrows([&] { RequireDecode(sheet, 0x95328236, 0, 4, 0x20000); });

    Prepare(sheet, Gb2005, Range, 1);
    RequireThrows([&] { RequireEncode(sheet, 0x20000, 0, 4); });
    Prepare(sheet, Gb2005, None, 0);
    RequireDecode(sheet, 0x95328236, 0, 4, 0x20000);

    Prepare(sheet, Gb2005, Uda, 0);
    RequireDecode(sheet, 0xAAA1, unassignedCode, 2, 0);
    RequireDecode(sheet, 0xA6D9, 0, 2, 0xE78D);
    RequireEncode(sheet, 0xE000, unassignedCode, 0);
    RequireEncode(sheet, 0xE816, 0, 2);

    Prepare(sheet, Gbk, Uda, 1);
    RequireDecode(sheet, 0xA140, 0, 2, 0xE4C6);
    RequireDecode(sheet, 0xFEFE, 0, 2, 0xE4C5);
    RequireDecode(sheet, 0xAAA1, unassignedCode, 2, 0);
    RequireEncode(sheet, 0xE4C6, unassignedCode, 0);

    alignas(8) std::uint8_t sjis[sheetSize] = {};
    Require(sceCesUcsProfileInitSJis1997Cp932(sjis) == sjis);
    RequireThrows([&] { sceCesGbUcsProfileSet4ByteCharRange(sjis, 0); });
    RequireThrows([&] { sceCesGbUcsProfileSetUdaMapping(sjis, 0); });
    return 0;
}
