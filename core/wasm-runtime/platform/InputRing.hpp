#ifndef CORE_WASM_RUNTIME_PLATFORM_INPUTRING_HPP
#define CORE_WASM_RUNTIME_PLATFORM_INPUTRING_HPP

#include "prx/libScePad/include/PadState.hpp"
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

namespace WasmPlatform {

inline constexpr std::uint32_t InputSlots = 64;
inline constexpr std::uint8_t TouchLeft = 1;
inline constexpr std::uint8_t TouchRight = 2;
inline constexpr std::uint8_t TouchCenter = 4;

struct InputSlot {
    std::uint32_t buttons = 0;
    std::array<std::uint8_t, 4> sticks{128, 128, 128, 128};
    std::uint8_t l2 = 0;
    std::uint8_t r2 = 0;
    std::uint8_t touch = 0;
    std::uint8_t device = 0;
};

struct InputRing {
    std::atomic<std::uint32_t> write{0};
    std::atomic<std::uint32_t> read{0};
    std::array<InputSlot, InputSlots> slots{};
};

static_assert(sizeof(InputSlot) == 12);
static_assert(offsetof(InputRing, slots) == 8);
static_assert(sizeof(InputRing) == 8 + InputSlots * sizeof(InputSlot));

inline bool PopInput(InputRing& ring, std::uint32_t written, InputSlot& slot) {
    const auto read = ring.read.load(std::memory_order_relaxed);
    if (read == written) return false;
    slot = ring.slots[read % InputSlots];
    ring.read.store(read + 1, std::memory_order_release);
    return true;
}

inline PadInputState ToPadInputState(const InputSlot& slot) {
    PadInputState state;
    state.buttons = slot.buttons;
    state.sticks = slot.sticks;
    state.analogButtonsL2 = slot.l2;
    state.analogButtonsR2 = slot.r2;
    state.touchLeft = (slot.touch & TouchLeft) != 0;
    state.touchRight = (slot.touch & TouchRight) != 0;
    if ((slot.touch & TouchCenter) != 0) state.touch[0] = {true, 960, 471, 0};
    state.deviceKind = slot.device;
    return state;
}

}  // namespace WasmPlatform

#endif
