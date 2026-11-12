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
    HostVolumeFocus_nid_no_patch(true, nullptr);
    HostAudioInConfigure_nid_no_patch(nullptr);
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

static void TestCaptureMuteGenerations() {
    audioin::detail::CaptureMuteGenerations generations;
    Require(!generations.Observe(4, 8));
    Require(!generations.Observe(4, 8));
    Require(generations.Observe(4, 9));
    Require(!generations.Observe(4, 9));
    Require(!generations.Observe(5, 9));
    generations.Forget(4);
    Require(!generations.Observe(4, 9));
}

int main() {
    TestFocusMuteState();
    TestCapturedSampleFilter();
    TestCaptureMuteGenerations();
    return 0;
}
