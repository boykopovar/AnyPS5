#include "SceTypes.hpp"
#include "prx/libScePad/include/Pad.hpp"
#include "prx/libScePad/include/PadState.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>

namespace {

void RequireFailed(const char* expr, const char* file, int line) {
    std::fprintf(stderr, "%s:%d: Require(%s) failed\n", file, line, expr);
    std::abort();
}

#define Require(condition) do { if (!(condition)) { RequireFailed(#condition, __FILE__, __LINE__); } } while (false)

} // namespace

int main() {
    PadVibrationParam vibParam{128, 64};
    PadTriggerEffectStateInformation triggerState{};
    std::array<std::uint8_t, 120> triggerBuffer{};

    for (int handle : {-9999, -1, 0, 1, PAD_REMOTE_HANDLE, 12345}) {
        Require(scePadDisableTriggerEffect(handle, true) == PAD_ERROR_NOT_INITIALIZED);
        Require(scePadDisableTriggerEffect(handle, false) == PAD_ERROR_NOT_INITIALIZED);
        Require(scePadGetTriggerEffectState(handle, &triggerState) == PAD_ERROR_NOT_INITIALIZED);
        Require(scePadGetTriggerEffectState(handle, nullptr) == PAD_ERROR_NOT_INITIALIZED);
        Require(scePadSetVibrationForce(handle, &vibParam) == PAD_ERROR_NOT_INITIALIZED);
        Require(scePadSetVibrationForce(handle, nullptr) == PAD_ERROR_NOT_INITIALIZED);
        Require(scePadSetVibration(handle, &vibParam) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetVibration(handle, nullptr) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadDisableVibration(handle, true) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadDisableVibration(handle, false) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetTriggerEffect(handle, triggerBuffer.data()) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetTriggerEffect(handle, nullptr) == PAD_ERROR_INVALID_HANDLE);
        Require(!scePadIsValidHandle(handle, PAD_PORT_TYPE_STANDARD));
        Require(!scePadIsValidHandle(handle, PAD_PORT_TYPE_SPECIAL));
    }

    Require(scePadInit_nid_postfix() == PAD_OK);
    Require(scePadSetVibrationTriggerEffectWeakWhileEmbeddedMicInUse(true) == PAD_OK);
    Require(scePadSetVibrationTriggerEffectWeakWhileEmbeddedMicInUse(false) == PAD_OK);

    for (int handle : {-9999, -1, 0, PAD_REMOTE_HANDLE, 12345}) {
        Require(scePadSetVibration(handle, &vibParam) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetVibration(handle, nullptr) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadDisableVibration(handle, true) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadDisableVibration(handle, false) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadDisableTriggerEffect(handle, true) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadDisableTriggerEffect(handle, false) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadGetTriggerEffectState(handle, &triggerState) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadGetTriggerEffectState(handle, nullptr) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetTriggerEffect(handle, triggerBuffer.data()) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetTriggerEffect(handle, nullptr) == PAD_ERROR_INVALID_HANDLE);
        Require(!scePadIsValidHandle(handle, PAD_PORT_TYPE_STANDARD));
        Require(!scePadIsValidHandle(handle, PAD_PORT_TYPE_SPECIAL));
        Require(scePadSetVibrationForce(handle, nullptr) == PAD_ERROR_INVALID_ARG);
    }

    const int handle = scePadOpen_nid_postfix(0x10000000, PAD_PORT_TYPE_STANDARD, 0, nullptr);
    Require(handle > 0);
    Require(scePadIsValidHandle(handle, PAD_PORT_TYPE_STANDARD));
    Require(!scePadIsValidHandle(handle, PAD_PORT_TYPE_SPECIAL));
    Require(!scePadIsValidHandle(handle, 99));

    Require(scePadSetVibration(handle, nullptr) == PAD_ERROR_INVALID_ARG);
    vibParam = {0, 0};
    Require(scePadSetVibration(handle, &vibParam) == PAD_OK);
    vibParam = {255, 255};
    Require(scePadSetVibration(handle, &vibParam) == PAD_OK);
    vibParam = {100, 200};
    Require(scePadSetVibration(handle, &vibParam) == PAD_OK);

    Require(scePadDisableVibration(handle, true) == PAD_OK);
    Require(scePadDisableVibration(handle, false) == PAD_OK);

    Require(scePadSetVibrationForce(handle, nullptr) == PAD_ERROR_INVALID_ARG);
    vibParam = {50, 75};
    Require(scePadSetVibrationForce(handle, &vibParam) == PAD_OK);

    Require(scePadDisableTriggerEffect(handle, true) == PAD_OK);
    Require(scePadDisableTriggerEffect(handle, false) == PAD_OK);

    Require(scePadGetTriggerEffectState(handle, nullptr) == PAD_ERROR_INVALID_ARG);
    std::memset(&triggerState, 0x5a, sizeof(triggerState));
    Require(scePadGetTriggerEffectState(handle, &triggerState) == PAD_OK);

    Require(scePadSetTriggerEffect(handle, nullptr) == PAD_ERROR_INVALID_ARG);

    for (int invalidMask : {0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x07, 0x0C, 0xFF}) {
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

    for (std::uint32_t invalidMode : {7u, 8u, 15u, 100u, 1000u, 0xFFFFFFFFu}) {
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
        triggerBuffer[0] = 0x01;
        std::memcpy(triggerBuffer.data() + 8, &validMode, 4);
        std::memcpy(triggerBuffer.data() + 8 + 56, &invalidMode, 4);
        Require(scePadSetTriggerEffect(handle, triggerBuffer.data()) == PAD_OK);

        triggerBuffer.fill(0);
        triggerBuffer[0] = 0x02;
        std::memcpy(triggerBuffer.data() + 8, &invalidMode, 4);
        std::memcpy(triggerBuffer.data() + 8 + 56, &validMode, 4);
        Require(scePadSetTriggerEffect(handle, triggerBuffer.data()) == PAD_OK);
    }

    const int special = scePadOpen_nid_postfix(0x10000000, PAD_PORT_TYPE_SPECIAL, 0, nullptr);
    Require(special > 0);
    Require(scePadIsValidHandle(special, PAD_PORT_TYPE_SPECIAL));
    Require(!scePadIsValidHandle(special, PAD_PORT_TYPE_STANDARD));

    Require(scePadSetVibration(special, nullptr) == PAD_ERROR_INVALID_ARG);
    vibParam = {10, 20};
    Require(scePadSetVibration(special, &vibParam) == PAD_OK);
    Require(scePadDisableVibration(special, true) == PAD_OK);
    Require(scePadDisableVibration(special, false) == PAD_OK);

    Require(scePadSetVibrationForce(special, nullptr) == PAD_ERROR_INVALID_ARG);
    Require(scePadSetVibrationForce(special, &vibParam) == PAD_OK);

    Require(scePadDisableTriggerEffect(special, true) == PAD_OK);
    Require(scePadDisableTriggerEffect(special, false) == PAD_OK);

    Require(scePadSetTriggerEffect(special, nullptr) == PAD_ERROR_INVALID_ARG);
    Require(scePadSetTriggerEffect(special, triggerBuffer.data()) == PAD_OK);

    Require(scePadGetTriggerEffectState(special, nullptr) == PAD_ERROR_INVALID_ARG);
    std::memset(&triggerState, 0x7f, sizeof(triggerState));
    Require(scePadGetTriggerEffectState(special, &triggerState) == PAD_OK);
    Require(triggerState.state[0] == 0 && triggerState.state[1] == 0);

    Require(scePadClose_nid_postfix(special) == PAD_OK);
    Require(!scePadIsValidHandle(special, PAD_PORT_TYPE_SPECIAL));
    Require(scePadSetVibration(special, &vibParam) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadDisableVibration(special, true) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadDisableTriggerEffect(special, true) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadSetTriggerEffect(special, triggerBuffer.data()) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadGetTriggerEffectState(special, &triggerState) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadClose_nid_postfix(special) == PAD_ERROR_INVALID_HANDLE);

    Require(scePadClose_nid_postfix(handle) == PAD_OK);
    Require(!scePadIsValidHandle(handle, PAD_PORT_TYPE_STANDARD));

    return 0;
}
