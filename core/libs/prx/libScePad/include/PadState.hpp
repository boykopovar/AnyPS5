#ifndef CORE_LIBS_PRX_LIBSCEPAD_PADSTATE_HPP
#define CORE_LIBS_PRX_LIBSCEPAD_PADSTATE_HPP

#include <array>
#include <cstdint>
#include <exception>
#include "SceTypes.hpp"

struct PadInputState {
    std::uint32_t buttons = 0;
    std::array<std::uint8_t, 4> sticks{128, 128, 128, 128};
    std::uint8_t analogButtonsL2 = 0;
    std::uint8_t analogButtonsR2 = 0;
    bool touchLeft = false;
    bool touchRight = false;
};

namespace Pad {
void Initialize();
PadData ReadState();
}

extern "C" void PadPublishInput_nid_postfix(const PadInputState& input);
extern "C" void PadReportInputFailure_nid_postfix(std::exception_ptr error);

#endif
