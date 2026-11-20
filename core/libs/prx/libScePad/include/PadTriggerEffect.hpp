#ifndef CORE_LIBS_PRX_LIBSCEPAD_PADTRIGGEREFFECT_HPP
#define CORE_LIBS_PRX_LIBSCEPAD_PADTRIGGEREFFECT_HPP

#include "prx/libScePad/include/PadState.hpp"
#include <array>
#include <cstddef>
#include <cstdint>

namespace PadTriggerEffect {

constexpr std::size_t ReportSize = 47;
constexpr std::size_t RightOffset = 10;
constexpr std::size_t LeftOffset = 21;

inline std::array<std::uint8_t, ReportSize> Build(const PadOutputState& state) {
    std::array<std::uint8_t, ReportSize> report{};
    for (int trigger = 0; trigger < 2; ++trigger) {
        if (!state.triggerTouched[trigger]) continue;
        report[0] |= trigger == 0 ? 0x08 : 0x04;
        std::uint8_t* destination = report.data() + (trigger == 0 ? LeftOffset : RightOffset);
        if (state.triggerDisabled) {
            destination[0] = 0x05;
            continue;
        }
        for (int index = 0; index < 11; ++index) destination[index] = state.trigger[trigger].effect[index];
        if (destination[0] == 0) destination[0] = 0x05;
    }
    return report;
}

}

#endif
