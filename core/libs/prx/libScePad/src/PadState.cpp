#include "prx/libScePad/include/PadState.hpp"
#include "prx/libkernel/Time/include/Time.hpp"
#include <mutex>
#include <stdexcept>

namespace {
    std::mutex stateMutex;
    PadInputState state;
    std::uint64_t timestamp = 0;
    std::exception_ptr failure;
    bool initialized = false;
}

void Pad::Initialize() {
    std::lock_guard lock(stateMutex);
    if (failure) std::rethrow_exception(failure);
    if (!initialized) timestamp = sceKernelGetProcessTime();
    initialized = true;
}

PadData Pad::ReadState() {
    std::lock_guard lock(stateMutex);
    if (failure) std::rethrow_exception(failure);
    if (!initialized) throw std::runtime_error("Pad: read before initialization");
    PadData data{};
    data.buttons = state.buttons;
    data.left_stick_x = state.sticks[0];
    data.left_stick_y = state.sticks[1];
    data.right_stick_x = state.sticks[2];
    data.right_stick_y = state.sticks[3];
    data.analog_buttons_l2 = state.analogButtonsL2;
    data.analog_buttons_r2 = state.analogButtonsR2;
    data.acceleration_y = 1.0f;
    data.orientation_w = 1.0f;
    data.connected = true;
    data.connected_count = 1;
    data.timestamp = timestamp;
    if (state.touchLeft || state.touchRight) {
        data.buttons |= 0x100000;
        data.touch_data_touch_num = 1;
        data.touch_data_touch0_x = state.touchRight ? 1440 : 480;
        data.touch_data_touch0_y = 471;
    }
    return data;
}

extern "C" void PadPublishInput_nid_postfix(const PadInputState& input) {
    std::lock_guard lock(stateMutex);
    if (failure) std::rethrow_exception(failure);
    if (state.buttons == input.buttons && state.sticks == input.sticks &&
        state.analogButtonsL2 == input.analogButtonsL2 && state.analogButtonsR2 == input.analogButtonsR2 &&
        state.touchLeft == input.touchLeft && state.touchRight == input.touchRight) return;
    state = input;
    timestamp = sceKernelGetProcessTime();
}

extern "C" void PadReportInputFailure_nid_postfix(std::exception_ptr error) {
    if (!error) throw std::invalid_argument("Pad: missing input failure");
    std::lock_guard lock(stateMutex);
    if (!failure) failure = error;
}
