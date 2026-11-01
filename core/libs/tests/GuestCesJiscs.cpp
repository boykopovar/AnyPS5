#include "prx/libSceCesCs/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdlib>
#include <stdexcept>

namespace {

constexpr int invalidProfile = static_cast<int>(0x805C0004);
constexpr int illegalCode = static_cast<int>(0x805C0015);
constexpr int unassignedCode = static_cast<int>(0x805C0020);
constexpr int invalidDstBuffer = static_cast<int>(0x805C0030);
constexpr std::uint64_t fnvBasis = 0xCBF29CE484222325ULL;
constexpr std::uint32_t poison = 0xDEADBEEF;

void Require(bool value) { if (!value) std::abort(); }

template<typename Action>
void RequireThrows(Action action) {
    try { action(); } catch (const std::runtime_error&) { return; }
    std::abort();
}

std::uint64_t Fnv(std::uint64_t hash, std::uint64_t value, int bytes) {
    for (int i = 0; i < bytes; ++i) {
        hash ^= value >> (8 * i) & 0xFF;
        hash *= 0x100000001B3ULL;
    }
    return hash;
}

std::uint64_t Pair(std::uint64_t hash, int result, std::uint32_t out) {
    return Fnv(Fnv(hash, static_cast<std::uint32_t>(result), 4), out, 4);
}

std::uint64_t JisRows(const void* profile) {
    std::uint64_t fold = fnvBasis;
    for (std::uint32_t plane = 0; plane < 3; ++plane) {
        for (std::uint32_t row = 0x21; row <= 0x7E; ++row) {
            std::uint64_t hash = fnvBasis;
            for (std::uint32_t cell = 0x21; cell <= 0x7E; ++cell) {
                std::uint32_t out = poison;
                const int result = sceCesJiscsToUcs(profile, plane << 16 | row << 8 | cell, &out);
                hash = Pair(hash, result, out);
            }
            fold = Fnv(fold, hash, 8);
        }
    }
    return fold;
}

std::uint64_t UcsBlocks(const void* profile, std::uint32_t plane) {
    std::uint64_t fold = fnvBasis;
    for (std::uint32_t base = plane << 16; base < (plane + 1) << 16; base += 256) {
        std::uint64_t hash = fnvBasis;
        for (std::uint32_t code = base; code < base + 256; ++code) {
            std::uint32_t out = poison;
            const int result = sceCesUcsToJiscs(code, profile, &out);
            hash = Pair(hash, result, out);
        }
        fold = Fnv(fold, hash, 8);
    }
    return fold;
}

std::uint64_t LowScan(const void* profile, bool fromUcs) {
    std::uint64_t hash = fnvBasis;
    for (std::uint32_t value = 0; value < 256; ++value) {
        std::uint32_t out = poison;
        const int result = fromUcs ? sceCesUcsToJiscs(value, profile, &out) : sceCesJiscsToUcs(profile, value, &out);
        hash = Pair(hash, result, out);
    }
    return hash;
}

using ProfileInit = void* (APS5_VABI*)(void*);

struct Expected {
    ProfileInit init;
    std::uint64_t rows;
    std::uint64_t bmp;
    std::uint64_t bytes;
    std::uint64_t low;
};

const Expected expected[8] = {
    {sceCesUcsProfileInitSJis1997X0208, 0x0A43EFAC27D27F0BULL, 0xC95A209A94B4733DULL, 0xF318D248701074E5ULL, 0xFE1F5C05D3553969ULL},
    {sceCesUcsProfileInitSJis1997Cp932, 0xB41491CD924A5628ULL, 0x7B9BC09A31B566BBULL, 0x6FBF52A74848866CULL, 0xC3FEB7F6573CCCE8ULL},
    {sceCesUcsProfileInitEucJpX0208, 0x6039B027421C7715ULL, 0xB808FE1D63F85985ULL, 0xF30BF109ED45C325ULL, 0xC3FEB7F6573CCCE8ULL},
    {sceCesUcsProfileInitEucJpX0208Ss2, 0x6039B027421C7715ULL, 0xD35C4BD66A76E55BULL, 0x6FBF52A74848866CULL, 0xC3FEB7F6573CCCE8ULL},
    {sceCesUcsProfileInitEucJpX0208Ss2Ss3, 0x6039B027421C7715ULL, 0x0775772C00CF90CCULL, 0x6FBF52A74848866CULL, 0x43D15427FBD6742FULL},
    {sceCesUcsProfileInitEucJpCp51932, 0xB41491CD924A5628ULL, 0x7B9BC09A31B566BBULL, 0x6FBF52A74848866CULL, 0xC3FEB7F6573CCCE8ULL},
    {sceCesUcsProfileInitSJis2004X0213, 0x2D347DA414591DB8ULL, 0x5E9E46E4D0A6B7E1ULL, 0x6FBF52A74848866CULL, 0x1D71EC505E9C63E2ULL},
    {sceCesUcsProfileInitEucJis2004, 0x2D347DA414591DB8ULL, 0xC5C280A43F74E4ECULL, 0x6FBF52A74848866CULL, 0x1D71EC505E9C63E2ULL},
};

constexpr std::uint64_t jis2004Plane2 = 0xBE5A8D1261719CCAULL;

void RequireDecode(const void* profile, std::uint32_t jiscs, int error, std::uint32_t code) {
    std::uint32_t out = poison;
    Require(sceCesJiscsToUcs(profile, jiscs, &out) == error && out == code);
}

void RequireEncode(const void* profile, std::uint32_t code, int error, std::uint32_t jiscs) {
    std::uint32_t out = poison;
    Require(sceCesUcsToJiscs(code, profile, &out) == error && out == jiscs);
}

}

int main() {
    alignas(8) static std::uint8_t sheets[8][64] = {};
    const void* profiles[8] = {};
    for (int i = 0; i < 8; ++i) {
        profiles[i] = expected[i].init(sheets[i]);
        Require(profiles[i] == sheets[i]);
        Require(JisRows(profiles[i]) == expected[i].rows);
        Require(UcsBlocks(profiles[i], 0) == expected[i].bmp);
        Require(LowScan(profiles[i], false) == expected[i].bytes);
        Require(LowScan(profiles[i], true) == expected[i].low);
    }
    Require(UcsBlocks(profiles[6], 2) == jis2004Plane2);
    Require(UcsBlocks(profiles[7], 2) == jis2004Plane2);

    RequireDecode(profiles[0], 0x2422, 0, 0x3042);
    RequireDecode(profiles[0], 0x2140, 0, 0x005C);
    RequireDecode(profiles[1], 0x7921, 0, 0x7E8A);
    RequireDecode(profiles[6], 0x7E7E, 0, 0x7E6B);
    RequireDecode(profiles[0], 0x7E7E, unassignedCode, 0);
    RequireDecode(profiles[7], 0x12121, illegalCode, 0);
    RequireEncode(profiles[0], 0x3042, 0, 0x03010402);
    RequireEncode(profiles[0], 0x00A5, 0, 0x5C);
    RequireEncode(profiles[1], 0x2160, 0, 0x03010D15);
    RequireEncode(profiles[4], 0x4E02, 0, 0x04001001);
    RequireEncode(profiles[7], 0x4E02, 0, 0x03020102);
    RequireEncode(profiles[6], 0x20B9F, 0, 0x03012F34);
    RequireEncode(profiles[2], 0xFF61, unassignedCode, 0);
    RequireEncode(profiles[0], 0xD800, illegalCode, 0);
    RequireEncode(profiles[0], 0x110000, illegalCode, 0);

    RequireDecode(nullptr, 0x2422, invalidProfile, 0);
    RequireDecode(nullptr, 0x12121, invalidProfile, 0);
    RequireEncode(nullptr, 0x3042, invalidProfile, 0);
    RequireEncode(nullptr, 0x20B9F, invalidProfile, 0);
    RequireEncode(nullptr, 0xFFFFFFFF, illegalCode, 0);
    Require(sceCesJiscsToUcs(profiles[0], 0x2422, nullptr) == invalidDstBuffer);
    Require(sceCesUcsToJiscs(0x3042, profiles[0], nullptr) == invalidDstBuffer);
    Require(sceCesJiscsToUcs(nullptr, 0x2422, nullptr) == invalidProfile);

    RequireThrows([&] { std::uint32_t out = 0; sceCesJiscsToUcs(profiles[0], 0x82A0, &out); });
    RequireThrows([&] { std::uint32_t out = 0; sceCesJiscsToUcs(profiles[4], 0x8FA2AF, &out); });
    RequireThrows([&] { std::uint32_t out = 0; sceCesUcsToJiscs(0x1F600, profiles[6], &out); });
    RequireThrows([&] { std::uint32_t out = 0; sceCesUcsToJiscs(0x20B9F, profiles[0], &out); });
    RequireThrows([&] { std::uint32_t out = 0; sceCesUcsToJiscs(0x3042, sceCesRefersUcsProfileCp1252(), &out); });
    return 0;
}
