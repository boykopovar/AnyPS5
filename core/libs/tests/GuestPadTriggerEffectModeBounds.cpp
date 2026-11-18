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
    if (!condition) {
        std::abort();
    }
}

template <typename Function>
void RequireThrowsUnimplemented(Function&& function) {
    try {
        function();
    } catch (const std::runtime_error&) {
        return;
    }
    std::abort();
}

} // namespace

int main() {
    PadTriggerEffectStateInformation triggerState{};
    std::array<std::uint8_t, 120> triggerBuffer{};

    for (int handle : {-99999, -1, 0, 1, 2, 3, 4, PAD_REMOTE_HANDLE, PAD_SPECIAL_HANDLE, 12345}) {
        Require(scePadDisableTriggerEffect(handle, true) == PAD_ERROR_NOT_INITIALIZED);
        Require(scePadDisableTriggerEffect(handle, false) == PAD_ERROR_NOT_INITIALIZED);
        Require(scePadGetTriggerEffectState(handle, &triggerState) == PAD_ERROR_NOT_INITIALIZED);
        Require(scePadGetTriggerEffectState(handle, nullptr) == PAD_ERROR_NOT_INITIALIZED);
        Require(scePadSetTriggerEffect(handle, triggerBuffer.data()) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetTriggerEffect(handle, nullptr) == PAD_ERROR_INVALID_HANDLE);
    }

    Require(scePadInit_nid_postfix() == PAD_OK);
    Require(scePadSetVibrationTriggerEffectWeakWhileEmbeddedMicInUse(true) == PAD_OK);
    Require(scePadSetVibrationTriggerEffectWeakWhileEmbeddedMicInUse(false) == PAD_OK);

    for (int handle : {-99999, -1, 0, 1, 2, 3, 4, PAD_REMOTE_HANDLE, PAD_SPECIAL_HANDLE, 12345}) {
        Require(scePadSetTriggerEffect(handle, triggerBuffer.data()) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetTriggerEffect(handle, nullptr) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadDisableTriggerEffect(handle, true) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadDisableTriggerEffect(handle, false) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadGetTriggerEffectState(handle, &triggerState) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadGetTriggerEffectState(handle, nullptr) == PAD_ERROR_INVALID_HANDLE);
        RequireThrowsUnimplemented([handle] {
            static_cast<void>(scePadSetTriggerEffectExt(handle, nullptr));
        });
        RequireThrowsUnimplemented([handle] {
            static_cast<void>(scePadSetTriggerEffectStrength(handle, 0));
        });
        RequireThrowsUnimplemented([handle] {
            static_cast<void>(scePadGetTriggerEffectInternalState(handle, nullptr));
        });
    }

    const int handle = scePadOpen_nid_postfix(0x10000000, PAD_PORT_TYPE_STANDARD, 0, nullptr);
    Require(handle > 0);

    Require(scePadSetTriggerEffect(handle, nullptr) == PAD_ERROR_INVALID_ARG);
    Require(scePadGetTriggerEffectState(handle, nullptr) == PAD_ERROR_INVALID_ARG);

    Require(scePadDisableTriggerEffect(handle, true) == PAD_OK);
    Require(scePadDisableTriggerEffect(handle, false) == PAD_OK);

    std::memset(&triggerState, 0x5a, sizeof(triggerState));
    Require(scePadGetTriggerEffectState(handle, &triggerState) == PAD_OK);

    for (int invalidMask : {0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x07, 0x0C, 0x13, 0x7F, 0x80, 0xFD, 0xFE, 0xFF}) {
        triggerBuffer.fill(0);
        triggerBuffer[0] = static_cast<std::uint8_t>(invalidMask);
        Require(scePadSetTriggerEffect(handle, triggerBuffer.data()) == PAD_ERROR_INVALID_ARG);
    }

    for (std::uint32_t mode : {0u, 1u, 2u, 3u, 4u, 5u, 6u}) {
        triggerBuffer.fill(0);
        triggerBuffer[0] = 0x01;
        std::memcpy(triggerBuffer.data() + 8, &mode, 4);
        Require(scePadSetTriggerEffect(handle, triggerBuffer.data()) == PAD_OK);

        triggerBuffer.fill(0);
        triggerBuffer[0] = 0x02;
        std::memcpy(triggerBuffer.data() + 8 + 56, &mode, 4);
        Require(scePadSetTriggerEffect(handle, triggerBuffer.data()) == PAD_OK);

        triggerBuffer.fill(0);
        triggerBuffer[0] = 0x03;
        std::memcpy(triggerBuffer.data() + 8, &mode, 4);
        std::memcpy(triggerBuffer.data() + 8 + 56, &mode, 4);
        Require(scePadSetTriggerEffect(handle, triggerBuffer.data()) == PAD_OK);
    }

    for (std::uint32_t invalidMode : {7u, 8u, 9u, 10u, 15u, 32u, 64u, 100u, 255u, 1000u, 0x7FFFFFFFu, 0x80000000u, 0xFFFFFFFFu}) {
        triggerBuffer.fill(0);
        triggerBuffer[0] = 0x01;
        std::memcpy(triggerBuffer.data() + 8, &invalidMode, 4);
        Require(scePadSetTriggerEffect(handle, triggerBuffer.data()) == PAD_ERROR_INVALID_ARG);

        triggerBuffer.fill(0);
        triggerBuffer[0] = 0x02;
        std::memcpy(triggerBuffer.data() + 8 + 56, &invalidMode, 4);
        Require(scePadSetTriggerEffect(handle, triggerBuffer.data()) == PAD_ERROR_INVALID_ARG);

        triggerBuffer.fill(0);
        triggerBuffer[0] = 0x03;
        const std::uint32_t validMode = 1;
        std::memcpy(triggerBuffer.data() + 8, &invalidMode, 4);
        std::memcpy(triggerBuffer.data() + 8 + 56, &validMode, 4);
        Require(scePadSetTriggerEffect(handle, triggerBuffer.data()) == PAD_ERROR_INVALID_ARG);

        triggerBuffer.fill(0);
        triggerBuffer[0] = 0x03;
        std::memcpy(triggerBuffer.data() + 8, &validMode, 4);
        std::memcpy(triggerBuffer.data() + 8 + 56, &invalidMode, 4);
        Require(scePadSetTriggerEffect(handle, triggerBuffer.data()) == PAD_ERROR_INVALID_ARG);

        triggerBuffer.fill(0);
        triggerBuffer[0] = 0x03;
        std::memcpy(triggerBuffer.data() + 8, &invalidMode, 4);
        std::memcpy(triggerBuffer.data() + 8 + 56, &invalidMode, 4);
        Require(scePadSetTriggerEffect(handle, triggerBuffer.data()) == PAD_ERROR_INVALID_ARG);

        triggerBuffer.fill(0);
        triggerBuffer[0] = 0x01;
        std::memcpy(triggerBuffer.data() + 8, &validMode, 4);
        std::memcpy(triggerBuffer.data() + 8 + 56, &invalidMode, 4);
        Require(scePadSetTriggerEffect(handle, triggerBuffer.data()) == PAD_OK);

        triggerBuffer.fill(0);
        triggerBuffer[0] = 0x02;
        std::memcpy(triggerBuffer.data() + 8, &invalidMode, 4);
        std::memcpy(triggerBuffer.data() + 8 + 56, &validMode, 4);
        Require(scePadSetTriggerEffect(handle, triggerBuffer.data()) == PAD_OK);

        triggerBuffer.fill(0);
        triggerBuffer[0] = 0x00;
        std::memcpy(triggerBuffer.data() + 8, &invalidMode, 4);
        std::memcpy(triggerBuffer.data() + 8 + 56, &invalidMode, 4);
        Require(scePadSetTriggerEffect(handle, triggerBuffer.data()) == PAD_OK);
    }

    for (std::uint32_t modeL = 0; modeL <= 6; ++modeL) {
        for (std::uint32_t modeR = 0; modeR <= 6; ++modeR) {
            triggerBuffer.fill(0);
            triggerBuffer[0] = 0x03;
            std::memcpy(triggerBuffer.data() + 8, &modeL, 4);
            std::memcpy(triggerBuffer.data() + 8 + 56, &modeR, 4);
            Require(scePadSetTriggerEffect(handle, triggerBuffer.data()) == PAD_OK);
        }
    }

    triggerBuffer.fill(0);
    triggerBuffer[0] = 0x03;
    Require(scePadSetTriggerEffect(handle, triggerBuffer.data()) == PAD_OK);
    Require(scePadGetTriggerEffectState(handle, &triggerState) == PAD_OK);

    triggerBuffer.fill(0);
    triggerBuffer[0] = 0x01;
    const std::uint32_t feedbackMode = 1;
    std::memcpy(triggerBuffer.data() + 8, &feedbackMode, 4);
    triggerBuffer[16] = 3;
    triggerBuffer[17] = 5;
    Require(scePadSetTriggerEffect(handle, triggerBuffer.data()) == PAD_OK);
    Require(scePadGetTriggerEffectState(handle, &triggerState) == PAD_OK);

    triggerBuffer.fill(0);
    triggerBuffer[0] = 0x02;
    const std::uint32_t weaponMode = 2;
    std::memcpy(triggerBuffer.data() + 8 + 56, &weaponMode, 4);
    triggerBuffer[16 + 56] = 3;
    triggerBuffer[17 + 56] = 6;
    triggerBuffer[18 + 56] = 7;
    Require(scePadSetTriggerEffect(handle, triggerBuffer.data()) == PAD_OK);
    Require(scePadGetTriggerEffectState(handle, &triggerState) == PAD_OK);

    triggerBuffer.fill(0);
    triggerBuffer[0] = 0x01;
    const std::uint32_t vibrationMode = 3;
    std::memcpy(triggerBuffer.data() + 8, &vibrationMode, 4);
    triggerBuffer[16] = 4;
    triggerBuffer[17] = 6;
    triggerBuffer[18] = 20;
    Require(scePadSetTriggerEffect(handle, triggerBuffer.data()) == PAD_OK);
    Require(scePadGetTriggerEffectState(handle, &triggerState) == PAD_OK);

    triggerBuffer.fill(0);
    triggerBuffer[0] = 0x01;
    const std::uint32_t multiFeedbackMode = 4;
    std::memcpy(triggerBuffer.data() + 8, &multiFeedbackMode, 4);
    for (int i = 0; i < 10; ++i) {
        triggerBuffer[16 + i] = static_cast<std::uint8_t>(i);
    }
    Require(scePadSetTriggerEffect(handle, triggerBuffer.data()) == PAD_OK);
    Require(scePadGetTriggerEffectState(handle, &triggerState) == PAD_OK);

    triggerBuffer.fill(0);
    triggerBuffer[0] = 0x02;
    const std::uint32_t slopeMode = 5;
    std::memcpy(triggerBuffer.data() + 8 + 56, &slopeMode, 4);
    triggerBuffer[16 + 56] = 2;
    triggerBuffer[17 + 56] = 7;
    triggerBuffer[18 + 56] = 2;
    triggerBuffer[19 + 56] = 8;
    Require(scePadSetTriggerEffect(handle, triggerBuffer.data()) == PAD_OK);
    Require(scePadGetTriggerEffectState(handle, &triggerState) == PAD_OK);

    triggerBuffer.fill(0);
    triggerBuffer[0] = 0x03;
    const std::uint32_t multiVibrationMode = 6;
    std::memcpy(triggerBuffer.data() + 8, &multiVibrationMode, 4);
    std::memcpy(triggerBuffer.data() + 8 + 56, &multiVibrationMode, 4);
    triggerBuffer[16] = 10;
    triggerBuffer[16 + 56] = 10;
    for (int i = 0; i < 10; ++i) {
        triggerBuffer[17 + i] = static_cast<std::uint8_t>(i + 1);
        triggerBuffer[17 + 56 + i] = static_cast<std::uint8_t>(i + 1);
    }
    Require(scePadSetTriggerEffect(handle, triggerBuffer.data()) == PAD_OK);
    Require(scePadGetTriggerEffectState(handle, &triggerState) == PAD_OK);

    for (int cycle = 0; cycle < 3; ++cycle) {
        for (std::uint32_t seqMode = 0; seqMode <= 6; ++seqMode) {
            triggerBuffer.fill(0);
            triggerBuffer[0] = 0x03;
            std::memcpy(triggerBuffer.data() + 8, &seqMode, 4);
            std::memcpy(triggerBuffer.data() + 8 + 56, &seqMode, 4);
            Require(scePadSetTriggerEffect(handle, triggerBuffer.data()) == PAD_OK);
            Require(scePadGetTriggerEffectState(handle, &triggerState) == PAD_OK);
        }
    }

    Require(scePadDisableTriggerEffect(handle, true) == PAD_OK);
    Require(scePadSetTriggerEffect(handle, triggerBuffer.data()) == PAD_OK);
    Require(scePadDisableTriggerEffect(handle, false) == PAD_OK);
    Require(scePadSetTriggerEffect(handle, triggerBuffer.data()) == PAD_OK);

    const int handle2 = scePadOpen_nid_postfix(0x10000001, PAD_PORT_TYPE_STANDARD, 0, nullptr);
    Require(handle2 > 0);
    Require(handle2 != handle);

    triggerBuffer.fill(0);
    triggerBuffer[0] = 0x01;
    const std::uint32_t modeP1 = 1;
    std::memcpy(triggerBuffer.data() + 8, &modeP1, 4);
    Require(scePadSetTriggerEffect(handle, triggerBuffer.data()) == PAD_OK);

    const std::uint32_t modeP2 = 2;
    std::memcpy(triggerBuffer.data() + 8, &modeP2, 4);
    Require(scePadSetTriggerEffect(handle2, triggerBuffer.data()) == PAD_OK);

    Require(scePadClose_nid_postfix(handle2) == PAD_OK);
    Require(scePadSetTriggerEffect(handle2, triggerBuffer.data()) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadSetTriggerEffect(handle, triggerBuffer.data()) == PAD_OK);

    const int special = scePadOpen_nid_postfix(0x10000000, PAD_PORT_TYPE_SPECIAL, 0, nullptr);
    Require(special > 0);

    Require(scePadSetTriggerEffect(special, nullptr) == PAD_ERROR_INVALID_ARG);
    Require(scePadSetTriggerEffect(special, triggerBuffer.data()) == PAD_OK);

    triggerBuffer[0] = 0xFF;
    Require(scePadSetTriggerEffect(special, triggerBuffer.data()) == PAD_OK);

    Require(scePadDisableTriggerEffect(special, true) == PAD_OK);
    Require(scePadDisableTriggerEffect(special, false) == PAD_OK);

    Require(scePadGetTriggerEffectState(special, nullptr) == PAD_ERROR_INVALID_ARG);
    std::memset(&triggerState, 0x7f, sizeof(triggerState));
    Require(scePadGetTriggerEffectState(special, &triggerState) == PAD_OK);
    Require(triggerState.state[0] == 0 && triggerState.state[1] == 0);

    Require(scePadClose_nid_postfix(special) == PAD_OK);
    Require(scePadSetTriggerEffect(special, triggerBuffer.data()) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadDisableTriggerEffect(special, false) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadGetTriggerEffectState(special, &triggerState) == PAD_ERROR_INVALID_HANDLE);

    Require(scePadClose_nid_postfix(handle) == PAD_OK);
    Require(scePadSetTriggerEffect(handle, triggerBuffer.data()) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadDisableTriggerEffect(handle, false) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadGetTriggerEffectState(handle, &triggerState) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadClose_nid_postfix(handle) == PAD_ERROR_INVALID_HANDLE);

    RequireThrowsUnimplemented([handle] {
        static_cast<void>(scePadSetTriggerEffectExt(handle, nullptr));
    });
    RequireThrowsUnimplemented([handle] {
        static_cast<void>(scePadSetTriggerEffectStrength(handle, 100));
    });
    RequireThrowsUnimplemented([handle] {
        static_cast<void>(scePadGetTriggerEffectInternalState(handle, nullptr));
    });

    return 0;
}
