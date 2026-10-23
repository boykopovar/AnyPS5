#include "SceTypes.hpp"
#include "prx/libScePad/include/PadState.hpp"
#include "prx/libScePad/include/PadTriggerEffect.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>

extern "C" {
int APS5_VABI scePadInit_nid_postfix(void);
int APS5_VABI scePadOpen_nid_postfix(int, int, int, const void*);
int APS5_VABI scePadSetTriggerEffect(int, const void*);
int APS5_VABI scePadGetTriggerEffectState(int, PadTriggerEffectStateInformation*);
int APS5_VABI scePadDisableTriggerEffect(int, bool);
}

namespace {

int handle = -1;
std::uint32_t seen = 0;

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "guest_pad_trigger_effects: %s\n", message);
    std::exit(1);
}

PadOutputState Fetch() {
    PadOutputState out{};
    Require(PadFetchOutput_nid_postfix(&seen, &out), "the controller output changed");
    return out;
}

void Set(std::uint8_t mask, int trigger, std::uint32_t mode, std::initializer_list<std::uint8_t> values) {
    std::array<std::uint8_t, 120> parameter{};
    parameter[0] = mask;
    std::memcpy(parameter.data() + 8 + 56 * trigger, &mode, 4);
    std::uint8_t* data = parameter.data() + 16 + 56 * trigger;
    std::size_t index = 0;
    for (std::uint8_t value : values) data[index++] = value;
    Require(scePadSetTriggerEffect(handle, parameter.data()) == 0, "scePadSetTriggerEffect succeeded");
}

void Reset() {
    std::array<std::uint8_t, 120> parameter{};
    parameter[0] = 0x03;
    Require(scePadSetTriggerEffect(handle, parameter.data()) == 0, "both triggers reset");
    static_cast<void>(Fetch());
}

std::uint16_t Zones(const PadOutputState& out, int trigger) {
    const auto& effect = out.trigger[trigger].effect;
    return static_cast<std::uint16_t>(effect[1] | (effect[2] << 8));
}

int Strength(const PadOutputState& out, int trigger, int zone) {
    const auto& effect = out.trigger[trigger].effect;
    const std::uint32_t packed = static_cast<std::uint32_t>(effect[3]) |
        (static_cast<std::uint32_t>(effect[4]) << 8u) |
        (static_cast<std::uint32_t>(effect[5]) << 16u) |
        (static_cast<std::uint32_t>(effect[6]) << 24u);
    return static_cast<int>((packed >> (3 * zone)) & 7u) + 1;
}

void LocalBuild() {
    PadOutputState out{};
    out.triggerTouched[0] = true;
    out.trigger[0].effect[0] = 0x21;
    auto report = PadTriggerEffect::Build(out);
    Require(report[0] == 0x08 && report[21] == 0x21, "an L2 effect enables only L2");
    Require(report[10] == 0, "an untouched R2 block keeps its enable bit clear");
    out.triggerTouched[1] = true;
    out.trigger[1].effect[0] = 0x25;
    report = PadTriggerEffect::Build(out);
    Require(report[0] == 0x0C && report[10] == 0x25 && report[21] == 0x21, "both trigger blocks are copied");
    out.triggerDisabled = true;
    report = PadTriggerEffect::Build(out);
    Require(report[0] == 0x0C && report[10] == 0x05 && report[21] == 0x05, "a disabled trigger effect turns both blocks off");
}

void Feedback() {
    Set(0x01, 0, 1, {3, 5});
    const auto out = Fetch();
    Require(out.trigger[0].effect[0] == 0x21, "feedback uses the rigid effect");
    Require(Zones(out, 0) == 0x03F8, "feedback starts at its position and reaches the last zone");
    for (int zone = 3; zone <= 9; ++zone) Require(Strength(out, 0, zone) == 5, "feedback strength is uniform past the position");
    Require(out.triggerTouched[0] && !out.triggerTouched[1], "only L2 was touched");
    const auto report = PadTriggerEffect::Build(out);
    Require(report[0] == 0x08 && (report[0] & 0x04) == 0, "the report enables only the touched trigger");
    Require(report[21] == 0x21, "the L2 block is placed at its report offset");
}

void Weapon() {
    Reset();
    Set(0x02, 1, 2, {3, 6, 7});
    const auto out = Fetch();
    Require(out.trigger[1].effect[0] == 0x25, "weapon uses the pulse effect");
    Require(Zones(out, 1) == ((1u << 3) | (1u << 6)), "weapon selects its start and end zones");
    Require(out.trigger[1].effect[3] == 6, "weapon strength is one less than the API value");
    const auto report = PadTriggerEffect::Build(out);
    Require(report[0] == 0x0C && report[10] == 0x25, "the R2 block is placed at its report offset");
}

void Vibration() {
    Reset();
    Set(0x01, 0, 3, {4, 6, 20});
    const auto out = Fetch();
    Require(out.trigger[0].effect[0] == 0x26, "vibration uses the pulse train effect");
    Require(Zones(out, 0) == 0x03F0, "vibration starts at its position and reaches the last zone");
    Require(Strength(out, 0, 4) == 6 && Strength(out, 0, 9) == 6, "vibration amplitude is uniform past the position");
    Require(out.trigger[0].effect[9] == 20, "vibration carries its frequency");
}

void MultiplePositionFeedback() {
    Reset();
    Set(0x01, 0, 4, {1, 2, 3, 4, 5, 6, 7, 8, 0, 0});
    const auto out = Fetch();
    Require(out.trigger[0].effect[0] == 0x21, "multiple position feedback uses the rigid effect");
    Require(Zones(out, 0) == 0x00FF, "multiple position feedback selects the configured zones");
    for (int zone = 0; zone < 8; ++zone) Require(Strength(out, 0, zone) == zone + 1, "multiple position feedback keeps each zone strength");
}

void SlopeFeedback() {
    Reset();
    Set(0x01, 0, 5, {2, 7, 2, 8});
    const auto out = Fetch();
    Require(out.trigger[0].effect[0] == 0x21, "slope feedback uses the rigid effect");
    Require(Strength(out, 0, 2) == 2, "slope feedback starts at the start strength");
    Require(Strength(out, 0, 3) == 3, "slope feedback rises between its endpoints");
    Require(Strength(out, 0, 7) == 8 && Strength(out, 0, 8) == 8, "slope feedback ends at the end strength");
}

void MultiplePositionVibration() {
    Reset();
    Set(0x02, 1, 6, {10, 0, 1, 2, 3, 4, 5, 6, 7, 8, 0});
    const auto out = Fetch();
    Require(out.trigger[1].effect[0] == 0x26, "multiple position vibration uses the pulse train effect");
    Require(out.trigger[1].effect[9] == 10, "multiple position vibration carries its frequency");
    Require(Zones(out, 1) == 0x01FE, "multiple position vibration selects the configured zones");
    Require(Strength(out, 1, 1) == 1 && Strength(out, 1, 8) == 8, "multiple position vibration keeps each zone amplitude");
}

void Off() {
    Reset();
    Set(0x01, 0, 0, {});
    const auto out = Fetch();
    Require(out.trigger[0].effect[0] == 0x05, "off mode writes the off effect");
    Require(!out.trigger[0].valid, "off mode is not a valid native effect");
    const auto report = PadTriggerEffect::Build(out);
    Require(report[21] == 0x05, "off mode is sent to the controller");
}

void Disable() {
    Reset();
    Set(0x01, 0, 1, {3, 5});
    static_cast<void>(Fetch());
    Require(scePadDisableTriggerEffect(handle, true) == 0, "trigger effects are disabled");
    auto out = Fetch();
    Require(out.triggerDisabled, "the disabled state reaches the output");
    auto report = PadTriggerEffect::Build(out);
    Require(report[0] == 0x0C && report[10] == 0x05 && report[21] == 0x05, "a disabled effect is sent as off");
    Require(scePadDisableTriggerEffect(handle, false) == 0, "trigger effects are enabled");
    out = Fetch();
    Require(!out.triggerDisabled, "the enabled state reaches the output");
    report = PadTriggerEffect::Build(out);
    Require(report[21] == 0x21, "the last effect is restored when trigger effects are enabled");
}

void State() {
    PadTriggerEffectStateInformation state{};
    PadPublishTriggerStatus_nid_postfix(0, 1, 0, 2, 1);
    Require(scePadGetTriggerEffectState(handle, &state) == 0, "trigger state is read");
    Require(state.state[0] == 1 && state.state[1] == 4, "feedback and weapon statuses are mapped");
    PadPublishTriggerStatus_nid_postfix(0, 1, 1, 2, 2);
    Require(scePadGetTriggerEffectState(handle, &state) == 0, "trigger state is read again");
    Require(state.state[0] == 2 && state.state[1] == 5, "pushing and fully pressed statuses are mapped");
    PadPublishTriggerStatus_nid_postfix(0, 3, 0, 3, 1);
    Require(scePadGetTriggerEffectState(handle, &state) == 0, "vibration state is read");
    Require(state.state[0] == 6 && state.state[1] == 7, "vibration statuses are mapped");
    PadPublishTriggerStatus_nid_postfix(0, 0, 0, 0, 0);
    Require(scePadGetTriggerEffectState(handle, &state) == 0, "the off state is read");
    Require(state.state[0] == 0 && state.state[1] == 0, "an inactive effect has no state");
}

}

int main() {
    Require(scePadInit_nid_postfix() == 0, "scePadInit succeeded");
    handle = scePadOpen_nid_postfix(1, 0, 0, nullptr);
    Require(handle > 0, "scePadOpen succeeded");
    LocalBuild();
    Feedback();
    Weapon();
    Vibration();
    MultiplePositionFeedback();
    SlopeFeedback();
    MultiplePositionVibration();
    Off();
    Disable();
    State();
    std::puts("guest pad trigger effects tests passed");
    return 0;
}
