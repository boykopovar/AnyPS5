#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdlib>
#include <initializer_list>

extern "C" {
int APS5_VABI sceGnmInsertDingDongMarker(std::uint32_t*, std::uint32_t);
int APS5_VABI sceGnmDrawIndexAuto(std::uint32_t*, std::uint32_t, std::uint32_t, std::uint32_t);
int APS5_VABI sceGnmDrawIndex(std::uint32_t*, std::uint32_t, std::uint32_t, std::uint64_t, std::uint32_t, std::uint32_t);
int APS5_VABI sceGnmDrawIndexOffset(std::uint32_t*, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t);
int APS5_VABI sceGnmDispatchDirect(std::uint32_t*, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t);
int APS5_VABI sceGnmDispatchIndirect(std::uint32_t*, std::uint32_t, std::uint32_t, std::uint32_t);
int APS5_VABI sceGnmDispatchIndirectOnMec(std::uint32_t*, std::uint32_t, std::uint64_t, std::uint32_t);
int APS5_VABI sceGnmDrawIndirect(std::uint32_t*, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t);
int APS5_VABI sceGnmDrawIndexIndirect(std::uint32_t*, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t);
int APS5_VABI sceGnmDrawIndirectMulti(std::uint32_t*, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t);
int APS5_VABI sceGnmDrawIndexIndirectMulti(std::uint32_t*, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t);
int APS5_VABI sceGnmDrawIndirectCountMulti(std::uint32_t*, std::uint32_t, std::uint32_t, std::uint32_t, std::uint64_t, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t);
int APS5_VABI sceGnmDrawIndexIndirectCountMulti(std::uint32_t*, std::uint32_t, std::uint32_t, std::uint32_t, std::uint64_t, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t);
int APS5_VABI sceGnmComputeWaitSemaphore(std::uint32_t*, std::uint32_t, std::uint64_t, std::uint32_t);
int APS5_VABI sceGnmComputeWaitOnAddress(std::uint32_t*, std::uint32_t, std::uint64_t, std::uint32_t, std::uint32_t, std::uint32_t);
int APS5_VABI sceGnmSetVgtControl(std::uint32_t*, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t);
int APS5_VABI sceGnmResetVgtControl(std::uint32_t*, std::uint32_t);
}

namespace {

constexpr std::uint32_t Untouched = 0xa5a5a5a5u;
constexpr std::uint32_t U = Untouched;

std::uint32_t g_words[16];

void Require(bool value) { if (!value) std::abort(); }

std::uint32_t* Fresh() {
    for (auto& word : g_words) word = Untouched;
    return g_words;
}

void RequireWords(std::initializer_list<std::uint32_t> expected) {
    std::size_t at = 0;
    for (const auto word : expected) Require(g_words[at++] == word);
    for (; at < 16; ++at) Require(g_words[at] == Untouched);
}

void RequireRefused(int result) {
    Require(result == -1);
    RequireWords({});
}

void CheckDirectDraws() {
    Require(sceGnmInsertDingDongMarker(Fresh(), 4) == 0);
    RequireWords({0xc0021000u, 0, 0, 0});
    RequireRefused(sceGnmInsertDingDongMarker(Fresh(), 3));
    Require(sceGnmInsertDingDongMarker(nullptr, 4) == -1);

    Require(sceGnmDrawIndexAuto(Fresh(), 7, 36, 0xe0000001u) == 0);
    RequireWords({0xc0012d00u, 36, 0xe0000002u, 0xc0021000u, 0, U, U});
    RequireRefused(sceGnmDrawIndexAuto(Fresh(), 7, 36, 2));
    RequireRefused(sceGnmDrawIndexAuto(Fresh(), 6, 36, 0));

    Require(sceGnmDrawIndex(Fresh(), 10, 99, 0x123456789abcull, 0x20000000u, 0) == 0);
    RequireWords({0xc0042700u, 99, 0x56789abcu, 0x1234, 99, 0x20000000u, 0xc0021000u, 0, U, U});
    RequireRefused(sceGnmDrawIndex(Fresh(), 10, 99, 0x1001, 0, 0));
    RequireRefused(sceGnmDrawIndex(Fresh(), 10, 99, 0, 0, 0));
    RequireRefused(sceGnmDrawIndex(Fresh(), 10, 99, 0x1000, 0x10, 0));

    Require(sceGnmDrawIndexOffset(Fresh(), 9, 4, 12, 0x40000001u) == 0);
    RequireWords({0xc0033501u, 12, 4, 12, 0x40000000u, 0xc0021000u, 0, U, U});
    RequireRefused(sceGnmDrawIndexOffset(Fresh(), 10, 4, 12, 0));
}

void CheckDispatches() {
    Require(sceGnmDispatchDirect(Fresh(), 9, 8, 4, 2, 0x19) == 0);
    RequireWords({0xc0031503u, 8, 4, 2, 0x19, 0xc0021000u, 0, U, U});
    RequireRefused(sceGnmDispatchDirect(Fresh(), 9, 0x80000000u, 1, 1, 0));
    RequireRefused(sceGnmDispatchDirect(Fresh(), 8, 1, 1, 1, 0));

    Require(sceGnmDispatchIndirect(Fresh(), 7, 0x40, 0x8) == 0);
    RequireWords({0xc0011602u, 0x40, 9, 0xc0021000u, 0, U, U});
    RequireRefused(sceGnmDispatchIndirect(Fresh(), 9, 0x40, 0));

    Require(sceGnmDispatchIndirectOnMec(Fresh(), 8, 0x700001000ull, 0x19) == 0);
    RequireWords({0xc0021603u, 0x1000, 7, 0x19, 0xc0021000u, 0});
    RequireRefused(sceGnmDispatchIndirectOnMec(Fresh(), 8, 0x700001002ull, 0));
    RequireRefused(sceGnmDispatchIndirectOnMec(Fresh(), 8, 0, 0));
}

void CheckIndirectDraws() {
    Require(sceGnmDrawIndirect(Fresh(), 9, 0x20, 2, 3, 5, 0x20000001u) == 0);
    RequireWords({0xc0032401u, 0x20, 0x4f, 0x51, 0x20000002u, 0xc0021000u, 0, U, U});
    RequireRefused(sceGnmDrawIndirect(Fresh(), 9, 0x20, 7, 0, 0, 0));
    RequireRefused(sceGnmDrawIndirect(Fresh(), 9, 0x20, 2, 0x10, 0, 0));

    Require(sceGnmDrawIndexIndirect(Fresh(), 9, 0x60, 4, 1, 0, 0x1) == 0);
    RequireWords({0xc0032501u, 0x60, 0xcd, 0, 0, 0xc0021000u, 0, U, U});
    RequireRefused(sceGnmDrawIndexIndirect(Fresh(), 9, 0x60, 4, 0, 0x10, 0));

    Require(sceGnmDrawIndirectMulti(Fresh(), 11, 0x30, 6, 6, 2, 0, 0x80000000u) == 0);
    RequireWords({0xc0052c00u, 0x30, 0x14e, 0, 6, 0x10, 0x80000002u, 0xc0021000u, 0, U, U});
    RequireRefused(sceGnmDrawIndirectMulti(Fresh(), 9, 0x30, 6, 6, 2, 0, 0));

    Require(sceGnmDrawIndexIndirectMulti(Fresh(), 11, 0x30, 6, 2, 0, 1, 0x60000000u) == 0);
    RequireWords({0xc0053800u, 0x30, 0, 0x4d, 6, 0x14, 0x60000000u, 0xc0021000u, 0, U, U});
    RequireRefused(sceGnmDrawIndexIndirectMulti(Fresh(), 11, 0x30, 6, 0, 0, 0, 0));
    RequireRefused(sceGnmDrawIndexIndirectMulti(Fresh(), 11, 0x30, 6, 3, 0, 0, 0));

    Require(sceGnmDrawIndirectCountMulti(Fresh(), 16, 0, 5, 0x200000040ull, 2, 1, 1, 1) == 0);
    RequireWords({0xc0011000u, 0, U, 0xc0089c01u, 0, 0x4d, 0x4d, 0x40000000u, 5, 0x40, 2, 0x10, 2, 0xc0011000u, 0, U});
    RequireRefused(sceGnmDrawIndirectCountMulti(Fresh(), 15, 0, 5, 0, 2, 1, 1, 0));

    Require(sceGnmDrawIndexIndirectCountMulti(Fresh(), 16, 0x14, 3, 0, 6, 0, 0, 0xe0000000u) == 0);
    RequireWords({0xc0011000u, 0, U, 0xc0089d00u, 0x14, 0, 0, 0, 3, 0, 0, 0x14, 0xe0000000u, 0xc0011000u, 0, U});
    RequireRefused(sceGnmDrawIndexIndirectCountMulti(Fresh(), 16, 0, 3, 0, 5, 0, 0, 0));
}

void CheckComputeWaits() {
    Require(sceGnmComputeWaitSemaphore(Fresh(), 10, 0x123456780ull, 1) == 0);
    RequireWords({0xc0021000u, 0, U, U, 0xc0013900u, 0x23456780u, 0xe0100001u, 0xc0011000u, 0, U});
    Require(sceGnmComputeWaitSemaphore(Fresh(), 10, 0x123456780ull, 0) == 0);
    Require(g_words[6] == 0xe0000001u);
    RequireRefused(sceGnmComputeWaitSemaphore(Fresh(), 9, 0, 0));

    Require(sceGnmComputeWaitOnAddress(Fresh(), 14, 0x500001003ull, 0xff, 0xb, 7) == 0);
    RequireWords({0xc0021000u, 0, U, U, 0xc0053c00u, 0x13, 0x1000, 5, 7, 0xff, 10, 0xc0011000u, 0, U});
    Require(sceGnmComputeWaitOnAddress(Fresh(), 14, 0x1234, 1, 3, 0) == 0);
    RequireWords({0xc0021000u, 0, U, U, 0xc0053c00u, 3, 0x1234, 0, 0, 1, 10, 0xc0011000u, 0, U});
    RequireRefused(sceGnmComputeWaitOnAddress(Fresh(), 13, 0x1234, 1, 3, 0));
}

void CheckVgtControl() {
    Require(sceGnmSetVgtControl(Fresh(), 3, 0x7f, 1, 1) == 0);
    RequireWords({0xc0017900u, 0x40000258u, 0x75007fu});
    Require(sceGnmSetVgtControl(Fresh(), 3, 0x10, 1, 0) == 0);
    RequireWords({0xc0017900u, 0x40000258u, 0x6d0010u});
    RequireRefused(sceGnmSetVgtControl(Fresh(), 3, 0x100, 0, 0));
    RequireRefused(sceGnmSetVgtControl(Fresh(), 3, 0, 2, 0));
    RequireRefused(sceGnmSetVgtControl(Fresh(), 3, 0, 0, 2));
    RequireRefused(sceGnmSetVgtControl(Fresh(), 4, 0, 0, 0));

    Require(sceGnmResetVgtControl(Fresh(), 3) == 0);
    RequireWords({0xc0017900u, 0x40000258u, 0x6d007fu});
    RequireRefused(sceGnmResetVgtControl(Fresh(), 2));
    Require(sceGnmResetVgtControl(nullptr, 3) == -1);
}

} // namespace

int main() {
    CheckDirectDraws();
    CheckDispatches();
    CheckIndirectDraws();
    CheckComputeWaits();
    CheckVgtControl();
}
