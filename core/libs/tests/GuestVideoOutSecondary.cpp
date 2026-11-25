#include "prx/libSceVideoOutSecondary/libSceVideoOutSecondary.hpp"
#include "prx/libc/include/GuestArena.hpp"

#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace {

void ClobberErrno(std::uintptr_t, std::size_t) {
    errno = 99;
}

void Require(bool value) {
    if (!value) std::abort();
}

constexpr int VideoOutBusy = static_cast<int>(0x80290009u);
constexpr int VideoOutInvalidHandle = static_cast<int>(0x8029000Bu);

}

int main() {
    GuestArena::GuestArenaSetHostReadHook_nid_no_patch(ClobberErrno);
    GuestArena::GuestArenaSetHostWriteHook_nid_no_patch(ClobberErrno);

    const std::int32_t missing = 10;
    const std::int32_t negative = -1;
    Require(libSceVideoOutSecondaryUnknown_s_plus_isXKLpwS0_nid_no_patch(&missing) == VideoOutInvalidHandle);
    Require(libSceVideoOutSecondaryUnknown_s_plus_isXKLpwS0_nid_no_patch(&negative) == VideoOutInvalidHandle);
    Require(libSceVideoOutSecondaryUnknown_s_plus_isXKLpwS0_nid_no_patch(nullptr) == VideoOutInvalidHandle);
    errno = 71;
    Require(libSceVideoOutSecondaryUnknown_s_plus_isXKLpwS0_nid_no_patch(reinterpret_cast<const std::int32_t*>(0x10)) == VideoOutInvalidHandle);
    Require(errno == 71);

    std::array<std::uint8_t, 32> cfg{};
    for (std::size_t i = 0; i < cfg.size(); ++i) {
        cfg[i] = static_cast<std::uint8_t>(0x20u + i);
    }

    errno = 71;
    Require(libSceVideoOutSecondaryUnknown_KJWkt9y7YC4_nid_no_patch(0, reinterpret_cast<const void*>(0x10)) == nullptr);
    Require(errno == 71);

    void* slot0 = libSceVideoOutSecondaryUnknown_KJWkt9y7YC4_nid_no_patch(0, cfg.data());
    Require(slot0 != nullptr);
    Require(libSceVideoOutSecondaryUnknown_KJWkt9y7YC4_nid_no_patch(0, cfg.data()) == nullptr);

    std::array<std::uint8_t, 32> stored{};
    std::memcpy(stored.data(), static_cast<const std::uint8_t*>(slot0) + 8, stored.size());
    Require(stored == cfg);

    void* slot1 = libSceVideoOutSecondaryUnknown_KJWkt9y7YC4_nid_no_patch(10, nullptr);
    void* slot2 = libSceVideoOutSecondaryUnknown_KJWkt9y7YC4_nid_no_patch(20, cfg.data());
    void* slot3 = libSceVideoOutSecondaryUnknown_KJWkt9y7YC4_nid_no_patch(30, cfg.data());
    Require(slot1 != nullptr && slot2 != nullptr && slot3 != nullptr);
    Require(reinterpret_cast<std::uintptr_t>(slot1) - reinterpret_cast<std::uintptr_t>(slot0) == 40);
    Require(reinterpret_cast<std::uintptr_t>(slot2) - reinterpret_cast<std::uintptr_t>(slot1) == 40);
    Require(reinterpret_cast<std::uintptr_t>(slot3) - reinterpret_cast<std::uintptr_t>(slot2) == 40);

    std::array<std::uint8_t, 32> zeros{};
    std::memcpy(stored.data(), static_cast<const std::uint8_t*>(slot1) + 8, stored.size());
    Require(stored == zeros);

    Require(libSceVideoOutSecondaryUnknown_KJWkt9y7YC4_nid_no_patch(40, cfg.data()) == nullptr);

    std::int32_t handle = 0;
    const std::int32_t busy = 1;
    std::memcpy(static_cast<std::uint8_t*>(slot0) + 4, &busy, sizeof(busy));
    Require(libSceVideoOutSecondaryUnknown_s_plus_isXKLpwS0_nid_no_patch(&handle) == VideoOutBusy);
    const std::int32_t idle = 0;
    std::memcpy(static_cast<std::uint8_t*>(slot0) + 4, &idle, sizeof(idle));
    Require(libSceVideoOutSecondaryUnknown_s_plus_isXKLpwS0_nid_no_patch(&handle) == 0);
    Require(libSceVideoOutSecondaryUnknown_s_plus_isXKLpwS0_nid_no_patch(&handle) == VideoOutInvalidHandle);

    void* reused = libSceVideoOutSecondaryUnknown_KJWkt9y7YC4_nid_no_patch(50, cfg.data());
    Require(reused == slot0);

    const std::int32_t highHandle = 10;
    std::memcpy(static_cast<std::uint8_t*>(slot1) + 4, &busy, sizeof(busy));
    Require(libSceVideoOutSecondaryUnknown_s_plus_isXKLpwS0_nid_no_patch(&highHandle) == VideoOutBusy);
    std::memcpy(static_cast<std::uint8_t*>(slot1) + 4, &idle, sizeof(idle));

    for (const std::int32_t remaining : {50, 10, 20, 30}) {
        Require(libSceVideoOutSecondaryUnknown_s_plus_isXKLpwS0_nid_no_patch(&remaining) == 0);
    }

    GuestArena::GuestArenaSetHostReadHook_nid_no_patch(nullptr);
    GuestArena::GuestArenaSetHostWriteHook_nid_no_patch(nullptr);
    return 0;
}
