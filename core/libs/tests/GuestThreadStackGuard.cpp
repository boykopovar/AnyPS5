#include "prx/libc/include/exceptions/CompactUnwind.hpp"
#include "prx/libkernel/include/Api.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <span>

static constexpr int SCE_OK = 0;
static constexpr std::uintptr_t GUEST_PAGE = 0x4000;
static constexpr std::size_t GUARD_SIZE = 3 * GUEST_PAGE;
static constexpr std::size_t STACK_SIZES[] = {65536, 1u << 20, 16u << 20};

static void Require(bool value) { if (!value) std::abort(); }

static void VerifyCompactUnwindBounds() {
    LibcUnwind::Rules rules {};
    const unsigned char overflowFn[4] = {0xf8, 0xff, 0xff, 0xff};
    Require(!LibcUnwind::CompactUnwindRules(0x03004000u, overflowFn, rules));
    const unsigned char validFn[4] = {0x00, 0x01, 0x00, 0x00};
    Require(LibcUnwind::CompactUnwindRules(0x03004000u, validFn, rules));
    Require(rules.cfaOffset == 0x110);

    const std::uint32_t words[21] = {
        1u,
        28u,
        1u,
        0u,
        0u,
        32u,
        2u,
        0x02000000u,
        0x1000u,
        56u,
        0u,
        0x2000u,
        0u,
        0u,
        3u,
        (2u << 16) | 12u,
        (1u << 16) | 20u,
        (1u << 24) | 0x000000u,
        (2u << 24) | 0x000100u,
        0x02000000u,
        0x02000000u,
    };
    unsigned char infoBytes[sizeof(words)] {};
    std::memcpy(infoBytes, words, sizeof(words));
    const std::span<const unsigned char> info(infoBytes, sizeof(infoBytes));

    LibcUnwind::CompactEntry entry {};
    Require(LibcUnwind::ParseCompactUnwind(info, {}, 0x1100u, nullptr, nullptr, entry) == LibcUnwind::CompactParse::Miss);
    Require(LibcUnwind::ParseCompactUnwind(info, {}, 0x1000u, nullptr, nullptr, entry) == LibcUnwind::CompactParse::Found);
    Require(entry.encoding == 0x02000000u);
    Require(entry.functionOffset == 0x1000u);
    Require(entry.functionEndOffset == 0x1100u);
    Require(LibcUnwind::ParseCompactUnwind(info, {}, 0x0800u, nullptr, nullptr, entry) == LibcUnwind::CompactParse::Miss);
}

static void* APS5_VABI GuardStackBottom(void* requested) {
    PthreadAttr attr = nullptr;
    Require(scePthreadAttrInit(&attr) == SCE_OK);
    Require(scePthreadAttrGet(scePthreadSelf(), &attr) == SCE_OK);
    void* address = nullptr;
    std::size_t size = 0;
    Require(scePthreadAttrGetstack(&attr, &address, &size) == SCE_OK);
    Require(scePthreadAttrDestroy(&attr) == SCE_OK);
    const auto start = reinterpret_cast<std::uintptr_t>(address);
    Require(start % GUEST_PAGE == 0);
    if (requested != nullptr) Require(size == *static_cast<const std::size_t*>(requested));
    int local = 0;
    Require(reinterpret_cast<std::uintptr_t>(&local) > start + GUARD_SIZE && reinterpret_cast<std::uintptr_t>(&local) < start + size);
    void* low = nullptr;
    void* high = nullptr;
    Require(sceKernelIsStack(&local, &low, &high) == SCE_OK);
    Require(low == address && reinterpret_cast<std::uintptr_t>(high) == start + size);
    for (const int protection : {3, 0, 3}) Require(mprotect_nid_postfix(address, GUARD_SIZE, protection) == 0);
    return nullptr;
}

int main() {
    VerifyCompactUnwindBounds();
    GuardStackBottom(nullptr);
    for (auto stackSize : STACK_SIZES) {
        PthreadAttr attr = nullptr;
        Require(scePthreadAttrInit(&attr) == SCE_OK);
        Require(scePthreadAttrSetstacksize(&attr, stackSize) == SCE_OK);
        Pthread thread = nullptr;
        Require(scePthreadCreate(&thread, &attr, GuardStackBottom, &stackSize, nullptr) == SCE_OK);
        Require(scePthreadAttrDestroy(&attr) == SCE_OK);
        Require(scePthreadJoin(thread, nullptr) == SCE_OK);
    }
}
