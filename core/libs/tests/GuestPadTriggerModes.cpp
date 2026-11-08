#include "SceTypes.hpp"
#include "prx/libScePad/include/Pad.hpp"
#include "prx/libScePad/include/PadState.hpp"
#include "prx/libScePad/include/PadTriggerEffect.hpp"
#include "prx/libScePad/libScePad.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>

namespace {

constexpr int SCE_PAD_ERROR_INVALID_ARG = static_cast<int>(0x80920001);
constexpr int SCE_PAD_ERROR_INVALID_HANDLE = static_cast<int>(0x80920003);
constexpr int SCE_PAD_ERROR_NOT_INITIALIZED = static_cast<int>(0x80920005);
constexpr int SCE_PAD_ERROR_NOT_CONNECTED = static_cast<int>(0x80920007);
constexpr int SCE_PAD_ERROR_NO_HANDLE = static_cast<int>(0x80920008);

int handle = -1;
std::uint32_t sequence = 0;

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "guest_pad_trigger_modes failed: %s\n", message);
    std::exit(1);
}

PadOutputState FetchOutput() {
    PadOutputState out{};
    Require(PadFetchOutput_nid_postfix(&sequence, &out), "PadFetchOutput succeeded");
    return out;
}

void SetTriggerParam(std::uint8_t mask, int trigger, std::uint32_t mode, std::initializer_list<std::uint8_t> values) {
    std::array<std::uint8_t, 120> param{};
    param[0] = mask;
    std::memcpy(param.data() + 8 + 56 * trigger, &mode, 4);
    std::uint8_t* target = param.data() + 16 + 56 * trigger;
    std::size_t index = 0;
    for (std::uint8_t v : values) target[index++] = v;
    Require(scePadSetTriggerEffect(handle, param.data()) == PAD_OK, "scePadSetTriggerEffect succeeded");
}

void ResetTriggers() {
    std::array<std::uint8_t, 120> param{};
    param[0] = 0x03;
    Require(scePadSetTriggerEffect(handle, param.data()) == PAD_OK, "reset both triggers");
    static_cast<void>(FetchOutput());
}

int GetStrength(const PadOutputState& out, int trigger, int zone) {
    const auto& effect = out.trigger[trigger].effect;
    const std::uint32_t packed = static_cast<std::uint32_t>(effect[3]) |
        (static_cast<std::uint32_t>(effect[4]) << 8u) |
        (static_cast<std::uint32_t>(effect[5]) << 16u) |
        (static_cast<std::uint32_t>(effect[6]) << 24u);
    return static_cast<int>((packed >> (3 * zone)) & 7u) + 1;
}

std::uint16_t GetZones(const PadOutputState& out, int trigger) {
    const auto& effect = out.trigger[trigger].effect;
    return static_cast<std::uint16_t>(effect[1] | (effect[2] << 8));
}

void TestErrorConstantsAndUninitialized() {
    Require(PAD_ERROR_INVALID_ARG == SCE_PAD_ERROR_INVALID_ARG, "PAD_ERROR_INVALID_ARG match");
    Require(PAD_ERROR_INVALID_HANDLE == SCE_PAD_ERROR_INVALID_HANDLE, "PAD_ERROR_INVALID_HANDLE match");
    Require(PAD_ERROR_NOT_INITIALIZED == SCE_PAD_ERROR_NOT_INITIALIZED, "PAD_ERROR_NOT_INITIALIZED match");

    const PadVibrationParam vib{10, 20};
    int busType = 0;
    int jackState = 0;
    PadContainerIdInformation idInfo{};

    for (int invalidHandle : {-1, 0, 1, 100}) {
        Require(scePadSetVibrationMode(invalidHandle, 1) == SCE_PAD_ERROR_NOT_INITIALIZED, "vibration mode not initialized");
        Require(scePadSetVibrationForce(invalidHandle, &vib) == SCE_PAD_ERROR_NOT_INITIALIZED, "vibration force not initialized");
        Require(scePadGetControllerBusType(invalidHandle, &busType) == SCE_PAD_ERROR_NOT_INITIALIZED, "bus type not initialized");
        Require(scePadGetJackState(invalidHandle, &jackState) == SCE_PAD_ERROR_NOT_INITIALIZED, "jack state not initialized");
        Require(scePadIsControllerUpdateRequired(invalidHandle) == SCE_PAD_ERROR_NOT_INITIALIZED, "update required not initialized");
        Require(scePadGetContainerIdInformation(invalidHandle, &idInfo) == SCE_PAD_ERROR_NOT_INITIALIZED, "container id not initialized");
        Require(scePadIsSupportedAudioFunction(invalidHandle) == SCE_PAD_ERROR_NOT_INITIALIZED, "audio function not initialized");
    }

    Require(scePadInit_nid_postfix() == PAD_OK, "scePadInit succeeded");
    handle = scePadOpen_nid_postfix(0x10000000, 0, 0, nullptr);
    Require(handle > 0, "scePadOpen succeeded");
}

void TestInvalidHandleRejection() {
    std::array<std::uint8_t, 120> param{};
    param[0] = 0x01;
    const PadVibrationParam vib{50, 60};
    PadTriggerEffectStateInformation stateInfo{};
    int busType = 0;
    int jackState = 0;
    PadContainerIdInformation idInfo{};

    for (int invalid : {-9999, -100, -2, -1, 0, handle + 10, handle + 100, 9999}) {
        Require(scePadSetTriggerEffect(invalid, param.data()) == SCE_PAD_ERROR_INVALID_HANDLE, "invalid handle trigger effect");
        Require(scePadSetVibration(invalid, &vib) == SCE_PAD_ERROR_INVALID_HANDLE, "invalid handle vibration");
        Require(scePadSetVibration(invalid, nullptr) == SCE_PAD_ERROR_INVALID_HANDLE, "invalid handle vibration null param");
        Require(scePadSetVibrationMode(invalid, 1) == SCE_PAD_ERROR_INVALID_HANDLE, "invalid handle vibration mode");
        Require(scePadDisableTriggerEffect(invalid, true) == SCE_PAD_ERROR_INVALID_HANDLE, "invalid handle disable trigger");
        Require(scePadGetTriggerEffectState(invalid, &stateInfo) == SCE_PAD_ERROR_INVALID_HANDLE, "invalid handle trigger state");
        Require(scePadGetControllerBusType(invalid, &busType) == SCE_PAD_ERROR_INVALID_HANDLE, "invalid handle bus type");
        Require(scePadGetJackState(invalid, &jackState) == SCE_PAD_ERROR_INVALID_HANDLE, "invalid handle jack state");
        Require(scePadIsControllerUpdateRequired(invalid) == SCE_PAD_ERROR_INVALID_HANDLE, "invalid handle update required");
        Require(scePadGetContainerIdInformation(invalid, &idInfo) == SCE_PAD_ERROR_INVALID_HANDLE, "invalid handle container id");
        Require(scePadIsSupportedAudioFunction(invalid) == SCE_PAD_ERROR_INVALID_HANDLE, "invalid handle audio support");
        Require(!scePadIsValidHandle(invalid, PAD_PORT_TYPE_STANDARD), "invalid handle not valid standard");
    }
}

void TestPortConnectionStateAndRejection() {
    int busType = 0;
    int jackState = 0;
    PadContainerIdInformation idInfo{};

    Require(scePadGetControllerBusType(handle, &busType) == PAD_OK, "bus type ok when connected");
    Require(busType == PAD_CONNECT_TYPE_BLUETOOTH, "default bluetooth connection");
    Require(scePadGetJackState(handle, &jackState) == PAD_OK, "jack state ok when connected");
    Require(jackState == 0, "default jack state zero");
    Require(scePadIsControllerUpdateRequired(handle) == PAD_OK, "controller update required ok");
    Require(scePadGetContainerIdInformation(handle, &idInfo) == PAD_OK, "container id ok when connected");
    Require(scePadIsSupportedAudioFunction(handle) == 1, "supported audio function returns 1");

    PadSetPortConnected_nid_postfix(0, false);

    Require(scePadGetControllerBusType(handle, &busType) == SCE_PAD_ERROR_NOT_CONNECTED, "bus type disconnected rejection");
    Require(scePadGetJackState(handle, &jackState) == SCE_PAD_ERROR_NOT_CONNECTED, "jack state disconnected rejection");
    Require(scePadIsControllerUpdateRequired(handle) == SCE_PAD_ERROR_NOT_CONNECTED, "update check disconnected rejection");
    Require(scePadGetContainerIdInformation(handle, &idInfo) == SCE_PAD_ERROR_NOT_CONNECTED, "container id disconnected rejection");
    Require(scePadIsSupportedAudioFunction(handle) == SCE_PAD_ERROR_NOT_CONNECTED, "audio function disconnected rejection");

    PadSetPortConnected_nid_postfix(0, true);
    Require(scePadGetControllerBusType(handle, &busType) == PAD_OK, "bus type restored on reconnect");

    const int special = scePadOpen_nid_postfix(0x10000000, PAD_PORT_TYPE_SPECIAL, 0, nullptr);
    Require(special >= PAD_SPECIAL_HANDLE, "special handle opened");
    Require(scePadGetControllerBusType(special, &busType) == SCE_PAD_ERROR_NOT_CONNECTED, "special handle not connected bus type");
    Require(scePadGetJackState(special, &jackState) == SCE_PAD_ERROR_NOT_CONNECTED, "special handle not connected jack state");
    Require(scePadIsControllerUpdateRequired(special) == SCE_PAD_ERROR_NOT_CONNECTED, "special handle not connected update check");
    Require(scePadGetContainerIdInformation(special, &idInfo) == SCE_PAD_ERROR_NOT_CONNECTED, "special handle not connected container id");
    Require(scePadIsSupportedAudioFunction(special) == SCE_PAD_ERROR_NOT_CONNECTED, "special handle not connected audio support");
    Require(scePadClose_nid_postfix(special) == PAD_OK, "special handle closed");
    Require(scePadGetControllerBusType(special, &busType) == SCE_PAD_ERROR_INVALID_HANDLE, "closed special handle invalid");
}

void TestTriggerEffectMaskBounds() {
    Require(scePadSetTriggerEffect(handle, nullptr) == SCE_PAD_ERROR_INVALID_ARG, "null param trigger effect rejected");

    std::array<std::uint8_t, 120> param{};
    for (std::uint8_t invalidMask : {0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x05, 0x06, 0x07, 0x0C, 0xFC, 0xFF}) {
        param[0] = invalidMask;
        Require(scePadSetTriggerEffect(handle, param.data()) == SCE_PAD_ERROR_INVALID_ARG, "invalid trigger mask rejected");
    }

    for (std::uint8_t validMask : {0x00, 0x01, 0x02, 0x03}) {
        param[0] = validMask;
        Require(scePadSetTriggerEffect(handle, param.data()) == PAD_OK, "valid trigger mask accepted");
    }
}

void TestTriggerResistanceModesAndModeBounds() {
    std::array<std::uint8_t, 120> param{};

    for (std::uint32_t invalidMode : {7u, 8u, 9u, 10u, 16u, 100u, 0xFFFFFFFFu}) {
        param[0] = 0x01;
        std::memcpy(param.data() + 8, &invalidMode, 4);
        Require(scePadSetTriggerEffect(handle, param.data()) == SCE_PAD_ERROR_INVALID_ARG, "left trigger invalid mode rejected");

        param[0] = 0x02;
        std::memcpy(param.data() + 8, &invalidMode, 4);
        std::memcpy(param.data() + 8 + 56, &invalidMode, 4);
        Require(scePadSetTriggerEffect(handle, param.data()) == SCE_PAD_ERROR_INVALID_ARG, "right trigger invalid mode rejected");

        param[0] = 0x03;
        const std::uint32_t validMode = 1;
        std::memcpy(param.data() + 8, &validMode, 4);
        std::memcpy(param.data() + 8 + 56, &invalidMode, 4);
        Require(scePadSetTriggerEffect(handle, param.data()) == SCE_PAD_ERROR_INVALID_ARG, "mixed valid and invalid mode rejected");

        param[0] = 0x00;
        Require(scePadSetTriggerEffect(handle, param.data()) == PAD_OK, "untouched mask ignores mode bytes");
    }

    ResetTriggers();
    SetTriggerParam(0x01, 0, 1, {0, 1});
    auto out = FetchOutput();
    Require(out.trigger[0].effect[0] == 0x21, "mode 1 min pos and strength rigid effect");
    Require(out.trigger[0].valid, "mode 1 min pos and strength valid");
    Require(GetZones(out, 0) == 0x03FF, "mode 1 pos 0 covers all 10 zones");
    for (int z = 0; z < 10; ++z) Require(GetStrength(out, 0, z) == 1, "uniform min strength across all zones");

    ResetTriggers();
    SetTriggerParam(0x01, 0, 1, {9, 8});
    out = FetchOutput();
    Require(out.trigger[0].effect[0] == 0x21, "mode 1 max pos and strength rigid effect");
    Require(out.trigger[0].valid, "mode 1 max pos and strength valid");
    Require(GetZones(out, 0) == (1u << 9), "mode 1 pos 9 covers zone 9 only");
    Require(GetStrength(out, 0, 9) == 8, "zone 9 max strength 8");

    ResetTriggers();
    SetTriggerParam(0x01, 0, 1, {10, 5});
    out = FetchOutput();
    Require(!out.trigger[0].valid, "mode 1 out-of-range pos > 9 invalid");
    Require(out.trigger[0].effect[0] == 0x05, "mode 1 out-of-range pos disabled");

    ResetTriggers();
    SetTriggerParam(0x01, 0, 1, {3, 0});
    out = FetchOutput();
    Require(!out.trigger[0].valid, "mode 1 strength 0 invalid");

    ResetTriggers();
    SetTriggerParam(0x01, 0, 1, {3, 9});
    out = FetchOutput();
    Require(!out.trigger[0].valid, "mode 1 strength 9 invalid");

    ResetTriggers();
    SetTriggerParam(0x02, 1, 2, {2, 8, 8});
    out = FetchOutput();
    Require(out.trigger[1].effect[0] == 0x25, "mode 2 weapon pulse effect");
    Require(out.trigger[1].valid, "mode 2 boundary parameters valid");
    Require(GetZones(out, 1) == ((1u << 2) | (1u << 8)), "mode 2 boundary zones 2 and 8");
    Require(out.trigger[1].effect[3] == 7, "mode 2 strength byte encoded as value - 1");

    ResetTriggers();
    SetTriggerParam(0x02, 1, 2, {7, 8, 1});
    out = FetchOutput();
    Require(out.trigger[1].valid, "mode 2 start 7 end 8 valid");
    Require(out.trigger[1].effect[3] == 0, "mode 2 min strength encoded as 0");

    ResetTriggers();
    SetTriggerParam(0x02, 1, 2, {1, 5, 5});
    out = FetchOutput();
    Require(!out.trigger[1].valid, "mode 2 start < 2 invalid");

    ResetTriggers();
    SetTriggerParam(0x02, 1, 2, {5, 5, 5});
    out = FetchOutput();
    Require(!out.trigger[1].valid, "mode 2 end == start invalid");

    ResetTriggers();
    SetTriggerParam(0x02, 1, 2, {4, 9, 5});
    out = FetchOutput();
    Require(!out.trigger[1].valid, "mode 2 end > 8 invalid");

    ResetTriggers();
    SetTriggerParam(0x01, 0, 4, {8, 7, 6, 5, 4, 3, 2, 1, 8, 7});
    out = FetchOutput();
    Require(out.trigger[0].effect[0] == 0x21, "mode 4 rigid effect");
    Require(out.trigger[0].valid, "mode 4 valid");
    Require(GetStrength(out, 0, 0) == 8, "zone 0 strength 8");
    Require(GetStrength(out, 0, 3) == 5, "zone 3 strength 5");
    Require(GetStrength(out, 0, 7) == 1, "zone 7 strength 1");

    ResetTriggers();
    SetTriggerParam(0x01, 0, 4, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0});
    out = FetchOutput();
    Require(!out.trigger[0].valid, "mode 4 all zero strengths invalid");

    ResetTriggers();
    SetTriggerParam(0x01, 0, 5, {0, 9, 1, 8});
    out = FetchOutput();
    Require(out.trigger[0].effect[0] == 0x21, "mode 5 slope full range rigid effect");
    Require(out.trigger[0].valid, "mode 5 full range valid");
    Require(GetStrength(out, 0, 0) == 1, "slope start strength");
    Require(GetStrength(out, 0, 9) == 8, "slope end strength");

    ResetTriggers();
    SetTriggerParam(0x01, 0, 5, {0, 10, 1, 8});
    out = FetchOutput();
    Require(!out.trigger[0].valid, "mode 5 end > 9 invalid");
}

void TestVibrationMotorTriggersAndFrequencyBounds() {
    Require(scePadSetVibration(handle, nullptr) == SCE_PAD_ERROR_INVALID_ARG, "null vibration param rejected");
    Require(scePadSetVibrationForce(handle, nullptr) == SCE_PAD_ERROR_INVALID_ARG, "null vibration force rejected");

    for (int mode : {-100, -5, -1, 0}) {
        Require(scePadSetVibrationMode(handle, mode) == SCE_PAD_ERROR_INVALID_ARG, "negative/zero vibration mode rejected");
    }
    for (int mode : {6, 7, 8, 100}) {
        Require(scePadSetVibrationMode(handle, mode) == SCE_PAD_ERROR_INVALID_ARG, "out-of-range vibration mode rejected");
    }
    for (int mode : {1, 2, 3, 4, 5}) {
        Require(scePadSetVibrationMode(handle, mode) == PAD_OK, "valid vibration mode accepted");
    }

    struct VibPair { std::uint8_t large; std::uint8_t small; };
    for (const auto& pair : std::initializer_list<VibPair>{
        {0, 0}, {255, 255}, {0, 255}, {255, 0}, {1, 1}, {128, 64}, {64, 128}, {254, 1}
    }) {
        const PadVibrationParam param{pair.large, pair.small};
        Require(scePadSetVibration(handle, &param) == PAD_OK, "scePadSetVibration succeeded");
        const auto out = FetchOutput();
        Require(out.vibrationLarge == pair.large, "vibrationLarge matches");
        Require(out.vibrationSmall == pair.small, "vibrationSmall matches");
    }

    for (std::uint8_t freq : {0, 1, 5, 20, 50, 100, 150, 200, 255}) {
        ResetTriggers();
        SetTriggerParam(0x01, 0, 3, {2, 6, freq});
        const auto out = FetchOutput();
        Require(out.trigger[0].effect[0] == 0x26, "mode 3 vibration pulse train effect");
        Require(out.trigger[0].valid, "mode 3 vibration valid");
        Require(out.trigger[0].effect[9] == freq, "mode 3 frequency byte matches");
        Require(GetStrength(out, 0, 2) == 6, "mode 3 strength matched");
    }

    for (std::uint8_t freq : {0, 1, 10, 40, 128, 255}) {
        ResetTriggers();
        SetTriggerParam(0x02, 1, 6, {freq, 1, 2, 3, 4, 5, 6, 7, 8, 7, 6});
        const auto out = FetchOutput();
        Require(out.trigger[1].effect[0] == 0x26, "mode 6 pulse train effect");
        Require(out.trigger[1].effect[9] == freq, "mode 6 frequency byte matches");
        Require(out.trigger[1].valid, "mode 6 valid");
        if (freq == 0) {
            Require(out.trigger[1].fallback == 0, "mode 6 zero frequency fallback is zero");
        } else {
            Require(out.trigger[1].fallback > 0, "mode 6 non-zero frequency fallback active");
        }
    }

    Require(scePadDisableVibration(handle, true) == PAD_OK, "disable vibration ok");
    const auto disabledOut = FetchOutput();
    Require(disabledOut.vibrationLarge == 0 && disabledOut.vibrationSmall == 0, "vibration zeroed on disable");
    Require(scePadDisableVibration(handle, false) == PAD_OK, "enable vibration ok");
}

void TestTriggerLifecycleAndStateTransitions() {
    ResetTriggers();
    SetTriggerParam(0x03, 0, 1, {3, 5});
    SetTriggerParam(0x03, 1, 2, {3, 6, 7});

    Require(scePadDisableTriggerEffect(handle, true) == PAD_OK, "disable trigger effect succeeded");
    auto out = FetchOutput();
    Require(out.triggerDisabled, "triggerDisabled is true");
    auto report = PadTriggerEffect::Build(out);
    Require(report[PadTriggerEffect::LeftOffset] == 0x05 && report[PadTriggerEffect::RightOffset] == 0x05, "disabled triggers reported as off");

    Require(scePadDisableTriggerEffect(handle, false) == PAD_OK, "re-enable trigger effect succeeded");
    out = FetchOutput();
    Require(!out.triggerDisabled, "triggerDisabled is false");
    report = PadTriggerEffect::Build(out);
    Require(report[PadTriggerEffect::LeftOffset] == 0x21 && report[PadTriggerEffect::RightOffset] == 0x25, "active triggers restored in report");

    PadTriggerEffectStateInformation stateInfo{};
    Require(scePadGetTriggerEffectState(handle, nullptr) == SCE_PAD_ERROR_INVALID_ARG, "null state info rejected");
    PadPublishTriggerStatus_nid_postfix(0, 1, 0, 2, 1);
    Require(scePadGetTriggerEffectState(handle, &stateInfo) == PAD_OK, "get trigger effect state succeeded");
    Require(stateInfo.state[0] == 1 && stateInfo.state[1] == 4, "feedback and weapon states mapped");

    PadClearTriggerStatus_nid_postfix(0);
    Require(scePadGetTriggerEffectState(handle, &stateInfo) == PAD_OK, "get trigger effect state after clear");
    Require(stateInfo.state[0] == 0 && stateInfo.state[1] == 0, "cleared states are zero");

    Require(scePadClose_nid_postfix(handle) == PAD_OK, "scePadClose succeeded");
    Require(scePadSetTriggerEffect(handle, out.trigger[0].effect) == SCE_PAD_ERROR_INVALID_HANDLE, "closed handle rejected");
    Require(scePadSetVibration(handle, nullptr) == SCE_PAD_ERROR_INVALID_HANDLE, "closed handle vibration rejected");
}

} // namespace

int main() {
    TestErrorConstantsAndUninitialized();
    TestInvalidHandleRejection();
    TestPortConnectionStateAndRejection();
    TestTriggerEffectMaskBounds();
    TestTriggerResistanceModesAndModeBounds();
    TestVibrationMotorTriggersAndFrequencyBounds();
    TestTriggerLifecycleAndStateTransitions();
    std::puts("guest pad trigger modes tests passed");
    return 0;
}
