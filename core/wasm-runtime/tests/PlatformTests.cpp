#include "AudioRing.hpp"
#include "InputRing.hpp"
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <stdexcept>
#include <vector>

namespace {

void Require(bool value) {
    if (!value) std::abort();
}

void PushSlot(WasmPlatform::InputRing& ring, const WasmPlatform::InputSlot& slot) {
    const auto write = ring.write.load();
    ring.slots[write % WasmPlatform::InputSlots] = slot;
    ring.write.store(write + 1);
}

void TestInputRingKeepsOrder() {
    WasmPlatform::InputRing ring;
    for (std::uint32_t index = 0; index < 3; ++index) PushSlot(ring, {index + 1});
    WasmPlatform::InputSlot slot;
    for (std::uint32_t index = 0; index < 3; ++index) {
        Require(WasmPlatform::PopInput(ring, ring.write.load(), slot));
        Require(slot.buttons == index + 1);
    }
    Require(!WasmPlatform::PopInput(ring, ring.write.load(), slot));
    Require(ring.read.load() == 3);
}

void TestInputRingWraps() {
    WasmPlatform::InputRing ring;
    WasmPlatform::InputSlot slot;
    for (std::uint32_t index = 0; index < 3 * WasmPlatform::InputSlots + 5; ++index) {
        PushSlot(ring, {index});
        Require(WasmPlatform::PopInput(ring, ring.write.load(), slot));
        Require(slot.buttons == index);
    }
}

void TestInputRingWrapsAroundTheIndexRange() {
    WasmPlatform::InputRing ring;
    ring.write.store(0xffffffffu);
    ring.read.store(0xffffffffu);
    PushSlot(ring, {7});
    Require(ring.write.load() == 0);
    WasmPlatform::InputSlot slot;
    Require(WasmPlatform::PopInput(ring, ring.write.load(), slot) && slot.buttons == 7);
    Require(ring.read.load() == 0);
}

void TestInputRingStopsAtObservedWrite() {
    WasmPlatform::InputRing ring;
    PushSlot(ring, {1});
    const auto observed = ring.write.load();
    PushSlot(ring, {2});
    WasmPlatform::InputSlot slot;
    Require(WasmPlatform::PopInput(ring, observed, slot) && slot.buttons == 1);
    Require(!WasmPlatform::PopInput(ring, observed, slot));
}

void TestPadStateConversion() {
    const WasmPlatform::InputSlot slot{0x4010, {0, 255, 128, 64}, 10, 255, WasmPlatform::TouchLeft | WasmPlatform::TouchCenter, 1};
    const auto state = WasmPlatform::ToPadInputState(slot);
    Require(state.buttons == 0x4010);
    Require(state.sticks == std::array<std::uint8_t, 4>{0, 255, 128, 64});
    Require(state.analogButtonsL2 == 10 && state.analogButtonsR2 == 255);
    Require(state.touchLeft && !state.touchRight);
    Require(state.touch[0].active && state.touch[0].x == 960 && state.touch[0].y == 471 && state.touch[0].id == 0);
    Require(!state.touch[1].active);
    Require(state.deviceKind == 1 && !state.hasMotion);
}

void TestNeutralSlot() {
    const auto state = WasmPlatform::ToPadInputState({});
    Require(state.buttons == 0 && state.sticks == std::array<std::uint8_t, 4>{128, 128, 128, 128});
    Require(state.analogButtonsL2 == 0 && state.analogButtonsR2 == 0);
    Require(!state.touchLeft && !state.touchRight && !state.touch[0].active && state.deviceKind == 0);
}

bool Near(float value, float expected) {
    return std::fabs(value - expected) < 1e-5f;
}

template<typename TFunction>
bool Throws(TFunction function) {
    try {
        function();
    } catch (const std::exception&) {
        return true;
    }
    return false;
}

void TestDownmixMonoAndStereo() {
    const float mono[] = {0.25f};
    const auto monoFrame = WasmPlatform::DownmixFrame({mono, true, 1, 1}, 0);
    Require(monoFrame[0] == 0.25f && monoFrame[1] == 0.25f);
    const float stereo[] = {0.5f, -0.25f, 0.125f, 0.75f};
    const auto second = WasmPlatform::DownmixFrame({stereo, true, 2, 2}, 1);
    Require(second[0] == 0.125f && second[1] == 0.75f);
}

void TestDownmixEightChannels() {
    const float frame[] = {0.1f, 0.2f, 0.3f, 0.9f, 0.1f, 0.2f, 0.1f, 0.0f};
    const auto mixed = WasmPlatform::DownmixFrame({frame, true, 8, 1}, 0);
    Require(Near(mixed[0], 0.1f + WasmPlatform::SurroundLevel * 0.5f));
    Require(Near(mixed[1], 0.2f + WasmPlatform::SurroundLevel * 0.5f));
}

void TestDownmixIntegersAndClamp() {
    const std::int16_t integers[] = {16384, -32768};
    const auto converted = WasmPlatform::DownmixFrame({integers, false, 2, 1}, 0);
    Require(converted[0] == 0.5f && converted[1] == -1.0f);
    const float loud[] = {0.9f, -0.9f, 0.9f, 0.0f, 0.9f, -0.9f, 0.9f, -0.9f};
    const auto clamped = WasmPlatform::DownmixFrame({loud, true, 8, 1}, 0);
    Require(clamped[0] == 1.0f && clamped[1] == -1.0f);
}

void TestDownmixRejectsOtherLayouts() {
    const float six[6] = {};
    Require(Throws([&] { WasmPlatform::DownmixFrame({six, true, 6, 1}, 0); }));
}

void TestRingsOpenAndClose() {
    auto state = std::make_unique<WasmPlatform::AudioState>();
    for (int index = 0; index < static_cast<int>(WasmPlatform::AudioRings); ++index) Require(WasmPlatform::OpenRing(*state) == index);
    Require(WasmPlatform::OpenRing(*state) == -1);
    state->rings[2].active.store(0);
    state->rings[2].read.store(100);
    Require(WasmPlatform::OpenRing(*state) == 2);
    Require(state->rings[2].write.load() == 100 && WasmPlatform::QueuedFrames(state->rings[2]) == 0);
    Require(Throws([&] { WasmPlatform::RingAt(*state, 7); }));
    state->rings[1].active.store(0);
    Require(Throws([&] { WasmPlatform::RingAt(*state, 1); }));
    Require(&WasmPlatform::RingAt(*state, 3) == &state->rings[3]);
}

void TestRingQueuesUpToItsSize() {
    auto state = std::make_unique<WasmPlatform::AudioState>();
    auto& ring = state->rings[static_cast<std::size_t>(WasmPlatform::OpenRing(*state))];
    std::vector<float> block(2 * WasmPlatform::AudioRingFrames, 0.5f);
    Require(WasmPlatform::WriteFrames(ring, {block.data(), true, 2, 256}) == 256);
    Require(WasmPlatform::QueuedFrames(ring) == 256);
    ring.read.store(200);
    Require(WasmPlatform::QueuedFrames(ring) == 56);
    Require(WasmPlatform::WriteFrames(ring, {block.data(), true, 2, WasmPlatform::AudioRingFrames}) == WasmPlatform::AudioRingFrames - 56);
    Require(WasmPlatform::QueuedFrames(ring) == WasmPlatform::AudioRingFrames);
    Require(WasmPlatform::WriteFrames(ring, {block.data(), true, 2, 1}) == 0);
}

void TestRingWritesAcrossItsEnd() {
    auto state = std::make_unique<WasmPlatform::AudioState>();
    auto& ring = state->rings[static_cast<std::size_t>(WasmPlatform::OpenRing(*state))];
    ring.read.store(WasmPlatform::AudioRingFrames - 2);
    ring.write.store(WasmPlatform::AudioRingFrames - 2);
    const float frames[] = {0.125f, 0.25f, 0.375f, 0.5f, 0.625f, 0.75f};
    Require(WasmPlatform::WriteFrames(ring, {frames, true, 2, 3}) == 3);
    Require(ring.samples[2 * (WasmPlatform::AudioRingFrames - 2)] == 0.125f);
    Require(ring.samples[2 * (WasmPlatform::AudioRingFrames - 1) + 1] == 0.5f);
    Require(ring.samples[0] == 0.625f && ring.samples[1] == 0.75f);
    Require(ring.write.load() == WasmPlatform::AudioRingFrames + 1);
}

}  // namespace

int main() {
    TestInputRingKeepsOrder();
    TestInputRingWraps();
    TestInputRingWrapsAroundTheIndexRange();
    TestInputRingStopsAtObservedWrite();
    TestPadStateConversion();
    TestNeutralSlot();
    TestDownmixMonoAndStereo();
    TestDownmixEightChannels();
    TestDownmixIntegersAndClamp();
    TestDownmixRejectsOtherLayouts();
    TestRingsOpenAndClose();
    TestRingQueuesUpToItsSize();
    TestRingWritesAcrossItsEnd();
    return 0;
}
