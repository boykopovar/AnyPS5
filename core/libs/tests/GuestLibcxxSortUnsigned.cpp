#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/MallocManagedSize.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <stdexcept>

static void Require(bool value, const char* message) {
    if (!value) {
        std::fprintf(stderr, "%s\n", message);
        std::abort();
    }
}

int main() {
    const auto sort = _ZNSt3__16__sortIRNS_6__lessIjjEEPjEEvT0_S5_T__nid_postfix;
    std::array<std::uint32_t, 8> values{std::numeric_limits<std::uint32_t>::max(), 0, 3, 3, 0x80000000u, 1, 2, std::numeric_limits<std::uint32_t>::max()};
    sort(values.data(), values.data() + values.size(), nullptr);
    Require((values == std::array<std::uint32_t, 8>{0, 1, 2, 3, 3, 0x80000000u, std::numeric_limits<std::uint32_t>::max(), std::numeric_limits<std::uint32_t>::max()}),
        "unsigned sort order or duplicate count differs");
    std::array<std::uint32_t, 4> partial{99, 42, 7, 88};
    sort(partial.data() + 1, partial.data() + 3, nullptr);
    Require((partial == std::array<std::uint32_t, 4>{99, 7, 42, 88}), "sort changed values outside its range");
    sort(nullptr, nullptr, nullptr);
    std::uint32_t single = 123;
    sort(&single, &single + 1, nullptr);
    Require(single == 123, "one-element sort changed its value");
    try {
        sort(reinterpret_cast<std::uint32_t*>(16), reinterpret_cast<std::uint32_t*>(8), nullptr);
        Require(false, "reversed range was accepted");
    } catch (const std::invalid_argument&) {
    }
    try {
        sort(reinterpret_cast<std::uint32_t*>(1), reinterpret_cast<std::uint32_t*>(9), nullptr);
        Require(false, "unreadable range was accepted");
    } catch (const std::invalid_argument&) {
    }

    void* const sentinelPtr = reinterpret_cast<void*>(static_cast<std::uintptr_t>(0x12345678u));
    void* memalignResult = sentinelPtr;
    *__error_nid_postfix() = 77;
    Require(sceLibcMspacePosixMemalign_nid_postfix(nullptr, &memalignResult, 64, 64) == 22,
            "sceLibcMspacePosixMemalign with invalid handle did not return 22");
    Require(*__error_nid_postfix() == 77,
            "sceLibcMspacePosixMemalign with invalid handle modified guest errno");
    Require(memalignResult == sentinelPtr,
            "sceLibcMspacePosixMemalign with invalid handle modified output pointer");

    alignas(64) std::array<std::uint8_t, 512> arenaBuffer{};
    void* arenaHandle = sceLibcMspaceCreate_nid_postfix("test_mspace", arenaBuffer.data(), arenaBuffer.size(), 0);
    Require(arenaHandle == arenaBuffer.data(), "sceLibcMspaceCreate failed");

    memalignResult = sentinelPtr;
    *__error_nid_postfix() = 77;
    Require(sceLibcMspacePosixMemalign_nid_postfix(arenaHandle, &memalignResult, 64, 1024) == 12,
            "sceLibcMspacePosixMemalign with oversized allocation did not return 12");
    Require(*__error_nid_postfix() == 77,
            "sceLibcMspacePosixMemalign OOM modified guest errno");
    Require(memalignResult == sentinelPtr,
            "sceLibcMspacePosixMemalign OOM modified output pointer");

    memalignResult = sentinelPtr;
    *__error_nid_postfix() = 77;
    Require(sceLibcMspacePosixMemalign_nid_postfix(arenaHandle, &memalignResult, 64, 64) == 0,
            "sceLibcMspacePosixMemalign valid aligned allocation failed");
    Require(memalignResult != nullptr && memalignResult != sentinelPtr,
            "sceLibcMspacePosixMemalign did not write allocated pointer");
    Require((reinterpret_cast<std::uintptr_t>(memalignResult) & 63u) == 0,
            "sceLibcMspacePosixMemalign returned unaligned pointer");
    Require(*__error_nid_postfix() == 77,
            "sceLibcMspacePosixMemalign valid allocation modified guest errno");
    sceLibcMspaceFree_nid_postfix(arenaHandle, memalignResult);
    Require(sceLibcMspaceDestroy_nid_postfix(arenaHandle) == 0, "sceLibcMspaceDestroy failed");

    MallocManagedSize stats{};
    stats.size = sizeof(MallocManagedSize);
    *__error_nid_postfix() = 77;
    Require(sceLibcMspaceMallocStats_nid_postfix(nullptr, &stats) == 22,
            "sceLibcMspaceMallocStats with invalid handle did not return 22");
    Require(*__error_nid_postfix() == 77,
            "sceLibcMspaceMallocStats with invalid handle modified guest errno");
    *__error_nid_postfix() = 77;
    Require(sceLibcMspaceMallocStatsFast_nid_postfix(nullptr, &stats) == 22,
            "sceLibcMspaceMallocStatsFast with invalid handle did not return 22");
    Require(*__error_nid_postfix() == 77,
            "sceLibcMspaceMallocStatsFast with invalid handle modified guest errno");

    return 0;
}
