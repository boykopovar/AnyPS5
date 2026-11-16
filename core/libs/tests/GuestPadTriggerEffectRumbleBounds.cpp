#include "SceTypes.hpp"
#include "prx/libScePad/include/Pad.hpp"
#include "prx/libScePad/include/PadState.hpp"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <stdexcept>

namespace {

void Require(bool condition) {
    if (!condition) std::abort();
}

PadOutputState FetchOutput(std::uint32_t* sequence) {
    PadOutputState out{};
    Require(PadFetchOutput_nid_postfix(sequence, &out));
    return out;
}

void TestTriggerEffectHandles() {
    alignas(8) std::array<std::uint8_t, 120> param{};
    param[0] = 0x00;

    for (int invalidHandle : {-100, -1, 0, 5, 12345}) {
        Require(scePadSetTriggerEffect(invalidHandle, param.data()) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetTriggerEffect(invalidHandle, nullptr) == PAD_ERROR_INVALID_HANDLE);
    }
}

void TestTriggerEffectSpecialHandle() {
    const int special = scePadOpen_nid_postfix(0x10000000, PAD_PORT_TYPE_SPECIAL, 0, nullptr);
    Require(special > 0);

    Require(scePadSetTriggerEffect(special, nullptr) == PAD_ERROR_INVALID_ARG);

    alignas(8) std::array<std::uint8_t, 120> param{};
    param[0] = 0xFF;
    Require(scePadSetTriggerEffect(special, param.data()) == PAD_OK);

    param[0] = 0x01;
    std::uint32_t invalidMode = 99;
    std::memcpy(param.data() + 8, &invalidMode, 4);
    Require(scePadSetTriggerEffect(special, param.data()) == PAD_OK);

    Require(scePadClose_nid_postfix(special) == PAD_OK);
    Require(scePadSetTriggerEffect(special, param.data()) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadSetTriggerEffect(special, nullptr) == PAD_ERROR_INVALID_HANDLE);
}

void TestTriggerEffectMaskBounds(int handle) {
    alignas(8) std::array<std::uint8_t, 120> param{};

    for (std::uint8_t validMask : {0x00, 0x01, 0x02, 0x03}) {
        param[0] = validMask;
        Require(scePadSetTriggerEffect(handle, param.data()) == PAD_OK);
    }

    for (std::uint8_t invalidMask : {0x04, 0x05, 0x06, 0x07, 0x08, 0x10, 0x20, 0x40, 0x80, 0xFC, 0xFD, 0xFE, 0xFF}) {
        param[0] = invalidMask;
        Require(scePadSetTriggerEffect(handle, param.data()) == PAD_ERROR_INVALID_ARG);
    }
}

void TestTriggerEffectModeBoundsAndSelectiveMask(int handle) {
    alignas(8) std::array<std::uint8_t, 120> param{};

    for (std::uint32_t validMode : {0u, 1u, 2u, 3u, 4u, 5u, 6u}) {
        param[0] = 0x03;
        std::memcpy(param.data() + 8, &validMode, 4);
        std::memcpy(param.data() + 8 + 56, &validMode, 4);
        Require(scePadSetTriggerEffect(handle, param.data()) == PAD_OK);
    }

    for (std::uint32_t invalidMode : {7u, 8u, 9u, 10u, 100u, 0xFFFFFFFFu}) {
        param[0] = 0x01;
        std::memcpy(param.data() + 8, &invalidMode, 4);
        std::uint32_t zeroMode = 0;
        std::memcpy(param.data() + 8 + 56, &zeroMode, 4);
        Require(scePadSetTriggerEffect(handle, param.data()) == PAD_ERROR_INVALID_ARG);

        param[0] = 0x01;
        std::memcpy(param.data() + 8, &zeroMode, 4);
        std::memcpy(param.data() + 8 + 56, &invalidMode, 4);
        Require(scePadSetTriggerEffect(handle, param.data()) == PAD_OK);

        param[0] = 0x02;
        std::memcpy(param.data() + 8, &zeroMode, 4);
        std::memcpy(param.data() + 8 + 56, &invalidMode, 4);
        Require(scePadSetTriggerEffect(handle, param.data()) == PAD_ERROR_INVALID_ARG);

        param[0] = 0x02;
        std::memcpy(param.data() + 8, &invalidMode, 4);
        std::memcpy(param.data() + 8 + 56, &zeroMode, 4);
        Require(scePadSetTriggerEffect(handle, param.data()) == PAD_OK);

        param[0] = 0x03;
        std::memcpy(param.data() + 8, &invalidMode, 4);
        std::memcpy(param.data() + 8 + 56, &zeroMode, 4);
        Require(scePadSetTriggerEffect(handle, param.data()) == PAD_ERROR_INVALID_ARG);

        param[0] = 0x03;
        std::memcpy(param.data() + 8, &zeroMode, 4);
        std::memcpy(param.data() + 8 + 56, &invalidMode, 4);
        Require(scePadSetTriggerEffect(handle, param.data()) == PAD_ERROR_INVALID_ARG);

        param[0] = 0x03;
        std::memcpy(param.data() + 8, &invalidMode, 4);
        std::memcpy(param.data() + 8 + 56, &invalidMode, 4);
        Require(scePadSetTriggerEffect(handle, param.data()) == PAD_ERROR_INVALID_ARG);

        param[0] = 0x00;
        std::memcpy(param.data() + 8, &invalidMode, 4);
        std::memcpy(param.data() + 8 + 56, &invalidMode, 4);
        Require(scePadSetTriggerEffect(handle, param.data()) == PAD_OK);
    }
}

void TestTriggerEffectFrequencies(int handle, std::uint32_t* sequence) {
    for (std::uint8_t freq : {0, 1, 10, 20, 40, 100, 255}) {
        alignas(8) std::array<std::uint8_t, 120> param{};
        param[0] = 0x01;
        std::uint32_t mode = 3;
        std::memcpy(param.data() + 8, &mode, 4);
        param[16] = 4;
        param[17] = 6;
        param[18] = freq;
        Require(scePadSetTriggerEffect(handle, param.data()) == PAD_OK);

        const auto out = FetchOutput(sequence);
        Require(out.triggerTouched[0]);
        Require(out.trigger[0].mode == 3);
        Require(out.trigger[0].effect[0] == 0x26);
        Require(out.trigger[0].effect[9] == freq);
    }

    for (std::uint8_t freq : {0, 1, 5, 25, 60, 255}) {
        alignas(8) std::array<std::uint8_t, 120> param{};
        param[0] = 0x02;
        std::uint32_t mode = 6;
        std::memcpy(param.data() + 8 + 56, &mode, 4);
        param[16 + 56] = freq;
        for (int i = 0; i < 10; ++i) param[16 + 56 + 1 + i] = static_cast<std::uint8_t>(i < 8 ? i + 1 : 0);
        Require(scePadSetTriggerEffect(handle, param.data()) == PAD_OK);

        const auto out = FetchOutput(sequence);
        Require(out.triggerTouched[1]);
        Require(out.trigger[1].mode == 6);
        Require(out.trigger[1].effect[0] == 0x26);
        Require(out.trigger[1].effect[9] == freq);
        if (freq == 0) {
            Require(out.trigger[1].fallback == 0);
        } else {
            Require(out.trigger[1].fallback > 0);
        }
    }
}

void TestVibrationMotorParameterBounds(int handle, std::uint32_t* sequence) {
    Require(sizeof(PadVibrationParam) == 2);

    PadVibrationParam param{};
    for (int invalidHandle : {-100, -1, 0, 5, 12345}) {
        Require(scePadSetVibration(invalidHandle, &param) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetVibration(invalidHandle, nullptr) == PAD_ERROR_INVALID_HANDLE);
    }

    Require(scePadSetVibration(handle, nullptr) == PAD_ERROR_INVALID_ARG);

    struct MotorPair {
        std::uint8_t large;
        std::uint8_t small;
    };

    for (const auto& pair : {
        MotorPair{0, 0},
        MotorPair{0, 255},
        MotorPair{255, 0},
        MotorPair{255, 255},
        MotorPair{1, 1},
        MotorPair{128, 64},
        MotorPair{200, 100}
    }) {
        param.large_motor = pair.large;
        param.small_motor = pair.small;
        Require(scePadSetVibration(handle, &param) == PAD_OK);
        const auto out = FetchOutput(sequence);
        Require(out.vibrationLarge == pair.large);
        Require(out.vibrationSmall == pair.small);
    }

    const int special = scePadOpen_nid_postfix(0x10000000, PAD_PORT_TYPE_SPECIAL, 0, nullptr);
    Require(special > 0);
    Require(scePadSetVibration(special, nullptr) == PAD_ERROR_INVALID_ARG);
    param.large_motor = 250;
    param.small_motor = 250;
    Require(scePadSetVibration(special, &param) == PAD_OK);
    Require(scePadClose_nid_postfix(special) == PAD_OK);
    Require(scePadSetVibration(special, &param) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadSetVibration(special, nullptr) == PAD_ERROR_INVALID_HANDLE);
}

void TestVibrationForceAndDisable(int handle, std::uint32_t* sequence) {
    PadVibrationParam param{170, 85};
    Require(scePadSetVibration(handle, &param) == PAD_OK);
    auto out = FetchOutput(sequence);
    Require(out.vibrationLarge == 170 && out.vibrationSmall == 85);

    for (int invalidHandle : {-100, -1, 0, 5, 12345}) {
        Require(scePadDisableVibration(invalidHandle, true) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadDisableVibration(invalidHandle, false) == PAD_ERROR_INVALID_HANDLE);
    }

    const int special = scePadOpen_nid_postfix(0x10000000, PAD_PORT_TYPE_SPECIAL, 0, nullptr);
    Require(special > 0);
    Require(scePadDisableVibration(special, true) == PAD_OK);
    Require(scePadDisableVibration(special, false) == PAD_OK);
    Require(scePadClose_nid_postfix(special) == PAD_OK);
    Require(scePadDisableVibration(special, true) == PAD_ERROR_INVALID_HANDLE);

    Require(scePadDisableVibration(handle, true) == PAD_OK);
    out = FetchOutput(sequence);
    Require(out.vibrationLarge == 0 && out.vibrationSmall == 0);

    Require(scePadDisableVibration(handle, false) == PAD_OK);

    param.large_motor = 120;
    param.small_motor = 60;
    Require(scePadSetVibration(handle, &param) == PAD_OK);
    out = FetchOutput(sequence);
    Require(out.vibrationLarge == 120 && out.vibrationSmall == 60);

    Require(scePadSetVibrationForce(handle, nullptr) == PAD_ERROR_INVALID_ARG);

    param.large_motor = 210;
    param.small_motor = 105;
    Require(scePadSetVibrationForce(handle, &param) == PAD_OK);
    out = FetchOutput(sequence);
    Require(out.vibrationLarge == 210 && out.vibrationSmall == 105);

    bool forceThrew = false;
    try {
        scePadSetVibrationForce(-1, &param);
    } catch (const std::runtime_error&) {
        forceThrew = true;
    }
    Require(forceThrew);

    Require(scePadSetVibrationTriggerEffectWeakWhileEmbeddedMicInUse(true) == PAD_OK);
    Require(scePadSetVibrationTriggerEffectWeakWhileEmbeddedMicInUse(false) == PAD_OK);

    for (int invalidHandle : {-100, -1, 0, 5, 12345}) {
        Require(scePadDisableTriggerEffect(invalidHandle, true) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadDisableTriggerEffect(invalidHandle, false) == PAD_ERROR_INVALID_HANDLE);
    }

    Require(scePadDisableTriggerEffect(handle, true) == PAD_OK);
    out = FetchOutput(sequence);
    Require(out.triggerDisabled);

    Require(scePadDisableTriggerEffect(handle, false) == PAD_OK);
    out = FetchOutput(sequence);
    Require(!out.triggerDisabled);
}

void TestLegacyAudioHapticsIntervals() {
    bool threw = false;
    try {
        PadQueueLegacyAudioHaptics_nid_postfix(0, 0, 100, 10, 10);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);

    threw = false;
    try {
        PadQueueLegacyAudioHaptics_nid_postfix(-1, 0, 100, 10, 10);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);

    threw = false;
    try {
        PadQueueLegacyAudioHaptics_nid_postfix(1, 100, 50, 10, 10);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);

    threw = false;
    try {
        PadQueueLegacyAudioHaptics_nid_postfix(1, 100, 100, 10, 10);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);

    PadQueueLegacyAudioHaptics_nid_postfix(1, 0, 1000, 50, 50);

    threw = false;
    try {
        PadQueueLegacyAudioHaptics_nid_postfix(1, 500, 1500, 60, 60);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);

    PadClearLegacyAudioHaptics_nid_postfix(1);
}

void TestUnimplementedStubs(int handle) {
    alignas(8) std::array<std::uint8_t, 256> buffer{};
    auto* buf = buffer.data();

    auto requireThrow = [](auto fn) {
        bool threw = false;
        try {
            fn();
        } catch (const std::runtime_error&) {
            threw = true;
        }
        Require(threw);
    };

    requireThrow([&] { scePadSetTriggerEffectStrength(handle, 50); });
    requireThrow([&] { scePadGetTriggerEffectInternalState(handle, reinterpret_cast<PadTriggerInternalState*>(buf)); });
    requireThrow([&] { scePadSetTriggerEffectExt(handle, reinterpret_cast<const PadTriggerEffectExtended*>(buf)); });
    requireThrow([&] { scePadSetVibrationStrength(handle, 50); });
    requireThrow([&] { scePadSetTriggerTuningParam(handle, reinterpret_cast<const PadTriggerTuningParameter*>(buf)); });
    requireThrow([&] { scePadSetStickTriggerTuningMode(handle, 1); });
    requireThrow([&] { scePadVirtualDeviceInsertTriggerInternalState(1, reinterpret_cast<const PadVirtualTriggerInternalState*>(buf)); });
    requireThrow([&] { scePadVirtualDeviceSetTriggerEffectSetting(handle, reinterpret_cast<const PadVirtualTriggerSetting*>(buf)); });
    requireThrow([&] { scePadVrControllerSetVibration(handle, 50); });
    requireThrow([&] { scePadVrControllerSetVibrationMode(handle, 1); });
    requireThrow([&] { scePadVrControllerSetTriggerEffect(handle, reinterpret_cast<const PadVrTriggerEffect*>(buf)); });
    requireThrow([&] { scePadVrControllerSetTriggerEffects(handle, handle, reinterpret_cast<const PadVrTriggerEffects*>(buf)); });
    requireThrow([&] { scePadVrControllerGetTriggerEffectState(handle, handle, reinterpret_cast<PadTriggerEffectStateInformation*>(buf)); });
    requireThrow([&] { scePadVrControllerGetTriggerEffectStates(handle, handle, reinterpret_cast<PadTriggerEffectStateInformation*>(buf)); });
    requireThrow([&] { scePadSetPS4BcVibrationMode(1); });
}

}

int main() {
    Require(scePadSetVibrationForce(-1, nullptr) == PAD_ERROR_NOT_INITIALIZED);
    Require(scePadSetVibrationForce(1, nullptr) == PAD_ERROR_NOT_INITIALIZED);
    Require(scePadDisableTriggerEffect(1, true) == PAD_ERROR_NOT_INITIALIZED);
    Require(scePadSetPS4BcVibrationMode(0) == PAD_ERROR_NOT_INITIALIZED);

    Require(scePadInit_nid_postfix() == PAD_OK);

    TestTriggerEffectHandles();
    TestTriggerEffectSpecialHandle();

    const int handle = scePadOpen_nid_postfix(0x10000000, 0, 0, nullptr);
    Require(handle > 0);

    Require(scePadSetTriggerEffect(handle, nullptr) == PAD_ERROR_INVALID_ARG);

    std::uint32_t sequence = 0;
    TestTriggerEffectMaskBounds(handle);
    TestTriggerEffectModeBoundsAndSelectiveMask(handle);
    TestTriggerEffectFrequencies(handle, &sequence);
    TestVibrationMotorParameterBounds(handle, &sequence);
    TestVibrationForceAndDisable(handle, &sequence);
    TestLegacyAudioHapticsIntervals();
    TestUnimplementedStubs(handle);

    Require(scePadClose_nid_postfix(handle) == PAD_OK);
    return 0;
}
