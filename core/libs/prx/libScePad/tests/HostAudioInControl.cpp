#include "prx/libSceAudioIn/AudioInCapture.hpp"
#include "prx/libScePad/include/HostAudioInControl.hpp"
#include "prx/libScePad/include/HostVolumeControl.hpp"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <stdexcept>

static void Require(bool value) {
    if (!value) std::abort();
}

static void TestFocusMuteState() {
    HostVolumeConfigure_nid_no_patch(nullptr, nullptr);
    HostAudioInConfigure_nid_no_patch(nullptr);
    std::uint64_t state = HostAudioInMuteState_nid_no_patch();
    Require((state & 1u) == 0);
    const std::uint64_t initialGeneration = state >> 1;

    HostAudioInConfigure_nid_no_patch("1");
    state = HostAudioInMuteState_nid_no_patch();
    Require((state & 1u) == 0 && (state >> 1) == initialGeneration);

    HostVolumeFocus_nid_no_patch(false, nullptr);
    state = HostAudioInMuteState_nid_no_patch();
    Require((state & 1u) != 0 && (state >> 1) == initialGeneration + 1);

    HostVolumeFocus_nid_no_patch(false, nullptr);
    Require(HostAudioInMuteState_nid_no_patch() == state);

    HostVolumeFocus_nid_no_patch(true, nullptr);
    state = HostAudioInMuteState_nid_no_patch();
    Require((state & 1u) == 0 && (state >> 1) == initialGeneration + 2);

    HostVolumeFocus_nid_no_patch(false, nullptr);
    state = HostAudioInMuteState_nid_no_patch();
    Require((state & 1u) != 0 && (state >> 1) == initialGeneration + 3);
    HostAudioInConfigure_nid_no_patch("0");
    state = HostAudioInMuteState_nid_no_patch();
    Require((state & 1u) == 0 && (state >> 1) == initialGeneration + 4);

    bool rejected = false;
    try {
        HostAudioInConfigure_nid_no_patch("true");
    } catch (const std::runtime_error&) {
        rejected = true;
    }
    Require(rejected && HostAudioInMuteState_nid_no_patch() == state);

    rejected = false;
    try {
        HostAudioInConfigure_nid_no_patch("");
    } catch (const std::runtime_error&) {
        rejected = true;
    }
    Require(rejected && HostAudioInMuteState_nid_no_patch() == state);
    HostVolumeFocus_nid_no_patch(true, nullptr);
    HostAudioInConfigure_nid_no_patch(nullptr);
}

static void TestAudioMuteTracksFocusIndependentlyFromHostVolume() {
    HostVolumeConfigure_nid_no_patch("100", "1");
    HostAudioInConfigure_nid_no_patch("0");
    const std::uint64_t initial = HostAudioInMuteState_nid_no_patch();

    HostVolumeApply_nid_no_patch(HostVolumeCommand::ToggleMute, nullptr);
    Require(HostAudioInMuteState_nid_no_patch() == initial);

    HostAudioInConfigure_nid_no_patch("1");
    const std::uint64_t enabled = HostAudioInMuteState_nid_no_patch();
    Require((enabled & 1u) == 0 && (enabled >> 1) == (initial >> 1));

    HostVolumeFocus_nid_no_patch(false, nullptr);
    const std::uint64_t unfocused = HostAudioInMuteState_nid_no_patch();
    Require((unfocused & 1u) != 0 && (unfocused >> 1) == (enabled >> 1) + 1);

    HostVolumeApply_nid_no_patch(HostVolumeCommand::ToggleMute, nullptr);
    Require(HostAudioInMuteState_nid_no_patch() == unfocused);

    HostVolumeFocus_nid_no_patch(true, nullptr);
    const std::uint64_t focused = HostAudioInMuteState_nid_no_patch();
    Require((focused & 1u) == 0 && (focused >> 1) == (unfocused >> 1) + 1);

    HostAudioInConfigure_nid_no_patch(nullptr);
    HostVolumeConfigure_nid_no_patch(nullptr, nullptr);
}

static void TestCapturedSampleFilter() {
    std::array<std::uint8_t, 4> samples{0x11, 0x22, 0x33, 0x44};
    Require(audioin::detail::filterCapturedSamples(samples.data(), samples.size(), 2, false) == 2);
    Require(samples == std::array<std::uint8_t, 4>{0x11, 0x22, 0, 0});

    samples = {0x11, 0x22, 0x33, 0x44};
    Require(audioin::detail::filterCapturedSamples(samples.data(), samples.size(), samples.size(), true) == 0);
    Require(samples == std::array<std::uint8_t, 4>{0, 0, 0, 0});

    samples = {0x11, 0x22, 0x33, 0x44};
    Require(audioin::detail::filterCapturedSamples(samples.data(), samples.size(), samples.size() + 1, false) == samples.size());
    Require(samples == std::array<std::uint8_t, 4>{0x11, 0x22, 0x33, 0x44});
}

static void TestDiscardQueuedAudioBelowThreshold() {
    std::array<std::uint8_t, 8> discarded{};
    std::size_t calls = 0;
    audioin::detail::discardQueuedAudio(0, discarded.data(), discarded.size(),
        [&calls](std::uint8_t*, std::uint32_t) {
            ++calls;
            return 0u;
        });
    Require(calls == 0);
}

static void TestDiscardQueuedAudioInChunks() {
    std::array<std::uint8_t, 4> discarded{};
    std::array<std::uint32_t, 3> requested{};
    std::size_t calls = 0;
    audioin::detail::discardQueuedAudio(10, discarded.data(), discarded.size(),
        [&requested, &calls](std::uint8_t*, std::uint32_t chunk) {
            requested[calls++] = chunk;
            return chunk;
        });
    Require(calls == 3);
    Require(requested == std::array<std::uint32_t, 3>{4, 4, 2});
}

static void TestDiscardQueuedAudioContinuesAfterPartialRead() {
    std::array<std::uint8_t, 4> discarded{};
    std::array<std::uint32_t, 3> requested{};
    std::size_t calls = 0;
    audioin::detail::discardQueuedAudio(10, discarded.data(), discarded.size(),
        [&requested, &calls](std::uint8_t*, std::uint32_t chunk) {
            requested[calls++] = chunk;
            return calls == 1 ? 2u : chunk;
        });
    Require(calls == 3);
    Require(requested == std::array<std::uint32_t, 3>{4, 4, 4});
}

static void TestDiscardQueuedAudioStopsOnNoProgress() {
    std::array<std::uint8_t, 4> discarded{};
    std::size_t calls = 0;
    audioin::detail::discardQueuedAudio(10, discarded.data(), discarded.size(),
        [&calls](std::uint8_t*, std::uint32_t) {
            ++calls;
            return 0u;
        });
    Require(calls == 1);
}

static void TestCaptureMuteGenerations() {
    audioin::detail::CaptureMuteGenerations generations;
    Require(!generations.Observe(4, 8));
    Require(!generations.Observe(4, 8));
    Require(generations.Observe(4, 9));
    Require(!generations.Observe(4, 9));
    Require(!generations.Observe(5, 9));
    Require(generations.Observe(5, 10));
    Require(!generations.Observe(4, 9));
    Require(generations.Observe(4, 10));
    generations.Forget(4);
    Require(!generations.Observe(4, 10));
    Require(!generations.Observe(5, 10));
}

int main() {
    TestFocusMuteState();
    TestCapturedSampleFilter();
    TestAudioMuteTracksFocusIndependentlyFromHostVolume();
    TestCaptureMuteGenerations();
    TestDiscardQueuedAudioBelowThreshold();
    TestDiscardQueuedAudioInChunks();
    TestDiscardQueuedAudioContinuesAfterPartialRead();
    TestDiscardQueuedAudioStopsOnNoProgress();
    return 0;
}
