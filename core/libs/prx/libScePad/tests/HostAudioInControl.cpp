#include "prx/libScePad/include/HostAudioInControl.hpp"
#include "prx/libScePad/include/HostVolumeControl.hpp"

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

int main() {
    TestFocusMuteState();
    return 0;
}
