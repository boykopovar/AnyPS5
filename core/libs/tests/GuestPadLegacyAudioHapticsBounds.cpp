#include "SceTypes.hpp"
#include "prx/libScePad/include/Pad.hpp"
#include "prx/libScePad/include/PadState.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <source_location>
#include <stdexcept>

namespace {

constexpr int PAD_ERROR_INVALID_ARG = static_cast<int>(0x80920001u);
constexpr int PAD_ERROR_INVALID_HANDLE = static_cast<int>(0x80920003u);

void Require(bool condition, const std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "Requirement failed at %s:%u in %s\n", location.file_name(), location.line(), location.function_name());
        std::abort();
    }
}

PadOutputState OutputOf(int port) {
    std::uint32_t seen = 0xffffffffu;
    PadOutputState output{};
    Require(PadFetchOutputForPort_nid_postfix(port, &seen, &output));
    return output;
}

void TestQueueArgumentValidation() {
    const std::uint64_t future = 10000000000ULL;

    for (int badSource : {-100, -10, -1, 0}) {
        bool caught = false;
        try {
            PadQueueLegacyAudioHaptics_nid_postfix(badSource, future, future + 1000, 50, 50);
        } catch (const std::invalid_argument&) {
            caught = true;
        }
        Require(caught);
    }

    bool caught = false;
    try {
        PadQueueLegacyAudioHaptics_nid_postfix(1, future + 1000, future + 1000, 50, 50);
    } catch (const std::invalid_argument&) {
        caught = true;
    }
    Require(caught);

    caught = false;
    try {
        PadQueueLegacyAudioHaptics_nid_postfix(1, future + 2000, future + 1000, 50, 50);
    } catch (const std::invalid_argument&) {
        caught = true;
    }
    Require(caught);

    PadQueueLegacyAudioHaptics_nid_postfix(1, future + 100, future + 500, 30, 40);

    caught = false;
    try {
        PadQueueLegacyAudioHaptics_nid_postfix(1, future + 400, future + 800, 30, 40);
    } catch (const std::invalid_argument&) {
        caught = true;
    }
    Require(caught);

    PadQueueLegacyAudioHaptics_nid_postfix(1, future + 500, future + 900, 60, 70);

    PadClearLegacyAudioHaptics_nid_postfix(1);
    PadClearLegacyAudioHaptics_nid_postfix(999);
    PadClearLegacyAudioHaptics_nid_postfix(-1);
}

void TestPort0RoutingAndMultiPortIsolation() {
    Require(scePadInit_nid_postfix() == PAD_OK);

    const int handle0 = scePadOpen_nid_postfix(0x10000000, PAD_PORT_TYPE_STANDARD, 0, nullptr);
    Require(handle0 == 1);

    const int handle1 = scePadOpen_nid_postfix(0x10000001, PAD_PORT_TYPE_STANDARD, 0, nullptr);
    Require(handle1 == 2);

    for (int port = 0; port < PAD_MAX_CONTROLLERS; ++port) {
        const PadOutputState state = OutputOf(port);
        Require(state.audioVibrationLarge == 0);
        Require(state.audioVibrationSmall == 0);
    }

    PadQueueLegacyAudioHaptics_nid_postfix(1, 0, ~std::uint64_t{0}, 180, 90);

    const PadOutputState port0State = OutputOf(0);
    Require(port0State.audioVibrationLarge == 180);
    Require(port0State.audioVibrationSmall == 90);

    for (int port = 1; port < PAD_MAX_CONTROLLERS; ++port) {
        const PadOutputState otherState = OutputOf(port);
        Require(otherState.audioVibrationLarge == 0);
        Require(otherState.audioVibrationSmall == 0);
    }

    PadQueueLegacyAudioHaptics_nid_postfix(2, 0, ~std::uint64_t{0}, 70, 220);

    const PadOutputState multiSourceState = OutputOf(0);
    Require(multiSourceState.audioVibrationLarge == 180);
    Require(multiSourceState.audioVibrationSmall == 220);

    for (int port = 1; port < PAD_MAX_CONTROLLERS; ++port) {
        const PadOutputState otherState = OutputOf(port);
        Require(otherState.audioVibrationLarge == 0);
        Require(otherState.audioVibrationSmall == 0);
    }

    PadClearLegacyAudioHaptics_nid_postfix(1);
    const PadOutputState partialClearState = OutputOf(0);
    Require(partialClearState.audioVibrationLarge == 70);
    Require(partialClearState.audioVibrationSmall == 220);

    PadClearLegacyAudioHaptics_nid_postfix(2);
    const PadOutputState fullyClearedState = OutputOf(0);
    Require(fullyClearedState.audioVibrationLarge == 0);
    Require(fullyClearedState.audioVibrationSmall == 0);
}

void TestAudioHapticsDirectVsLegacyAndRegularVibration() {
    const int handle0 = 1;

    const PadVibrationParam vib{45, 65};
    Require(scePadSetVibration(handle0, &vib) == PAD_OK);

    PadOutputState state = OutputOf(0);
    Require(state.vibrationLarge == 45);
    Require(state.vibrationSmall == 65);
    Require(state.audioVibrationLarge == 0);
    Require(state.audioVibrationSmall == 0);

    PadQueueLegacyAudioHaptics_nid_postfix(10, 0, ~std::uint64_t{0}, 120, 140);
    state = OutputOf(0);
    Require(state.vibrationLarge == 45);
    Require(state.vibrationSmall == 65);
    Require(state.audioVibrationLarge == 120);
    Require(state.audioVibrationSmall == 140);

    PadSubmitAudioHapticsForPort_nid_postfix(0, 200, 30);
    state = OutputOf(0);
    Require(state.vibrationLarge == 45);
    Require(state.vibrationSmall == 65);
    Require(state.audioVibrationLarge == 200);
    Require(state.audioVibrationSmall == 140);

    PadClearLegacyAudioHaptics_nid_postfix(10);
    state = OutputOf(0);
    Require(state.vibrationLarge == 45);
    Require(state.vibrationSmall == 65);
    Require(state.audioVibrationLarge == 200);
    Require(state.audioVibrationSmall == 30);

    const PadVibrationParam zeroVib{0, 0};
    Require(scePadSetVibration(handle0, &zeroVib) == PAD_OK);
    state = OutputOf(0);
    Require(state.vibrationLarge == 0);
    Require(state.vibrationSmall == 0);
}

void TestInvalidHandleRejectionAndPortBounds() {
    const int handle0 = 1;
    const int handle1 = 2;

    for (int badPort : {-100, -1, 4, 5, 10, 999}) {
        PadSubmitAudioHapticsForPort_nid_postfix(badPort, 100, 100);
        std::int32_t triggerStates[2]{99, 99};
        PadGetTriggerEffectState_nid_postfix(badPort, triggerStates);
        Require(triggerStates[0] == 99);
        Require(triggerStates[1] == 99);
    }

    const PadVibrationParam testVib{10, 20};
    Require(scePadSetVibration(handle0, nullptr) == PAD_ERROR_INVALID_ARG);
    Require(scePadSetVibration(handle1, nullptr) == PAD_ERROR_INVALID_ARG);

    for (int badHandle : {-100, -1, 0, 3, 4, 10, PAD_REMOTE_HANDLE, PAD_SPECIAL_HANDLE, 9999}) {
        Require(scePadSetVibration(badHandle, &testVib) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetVibration(badHandle, nullptr) == PAD_ERROR_INVALID_HANDLE);
    }

    const int special = scePadOpen_nid_postfix(0x10000000, PAD_PORT_TYPE_SPECIAL, 0, nullptr);
    Require(special == PAD_SPECIAL_HANDLE);
    Require(scePadSetVibration(special, nullptr) == PAD_ERROR_INVALID_ARG);
    Require(scePadSetVibration(special, &testVib) == PAD_OK);

    Require(scePadClose_nid_postfix(special) == PAD_OK);
    Require(scePadSetVibration(special, &testVib) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadSetVibration(special, nullptr) == PAD_ERROR_INVALID_HANDLE);

    Require(scePadClose_nid_postfix(handle1) == PAD_OK);
    Require(scePadClose_nid_postfix(handle0) == PAD_OK);
}

} // namespace

int main() {
    TestQueueArgumentValidation();
    TestPort0RoutingAndMultiPortIsolation();
    TestAudioHapticsDirectVsLegacyAndRegularVibration();
    TestInvalidHandleRejectionAndPortBounds();
    return 0;
}
