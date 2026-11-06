#include "prx/libScePad/include/HostVolumeControl.hpp"

#include <atomic>
#include <cstdlib>
#include <mutex>

#include "prx/libScePad/include/HostVolumeSettings.hpp"

namespace {

std::mutex stateMutex;
HostVolume state;
std::atomic<std::uint32_t> publishedGain{HostVolume::UnityGain};

void publish() {
    publishedGain.store(state.Gain(), std::memory_order_release);
}

}

bool HostVolumeApply_nid_no_patch(HostVolumeCommand command, HostVolume* after) {
    std::lock_guard lock(stateMutex);
    bool changed = false;
    switch (command) {
        case HostVolumeCommand::VolumeUp: changed = state.VolumeUp(); break;
        case HostVolumeCommand::VolumeDown: changed = state.VolumeDown(); break;
        case HostVolumeCommand::ToggleMute: changed = state.ToggleMute(); break;
    }
    publish();
    if (after != nullptr) *after = state;
    return changed;
}

bool HostVolumeFocus_nid_no_patch(bool focused, HostVolume* after) {
    std::lock_guard lock(stateMutex);
    const bool changed = state.FocusChanged(focused);
    publish();
    if (after != nullptr) *after = state;
    return changed;
}

void HostVolumeConfigure_nid_no_patch(const char* initialVolume, const char* muteOnFocusLoss) {
    HostVolume fresh;
    if (initialVolume != nullptr) fresh.SetVolume(ParseHostVolumeLevel("ANYPS5_VOLUME", initialVolume));
    if (muteOnFocusLoss != nullptr) fresh.SetMuteOnFocusLoss(ParseHostVolumeFlag("ANYPS5_MUTE_ON_FOCUS_LOSS", muteOnFocusLoss));
    std::lock_guard lock(stateMutex);
    state = fresh;
    publish();
}

void HostVolumeConfigureFromEnvironment_nid_no_patch() {
    HostVolumeConfigure_nid_no_patch(std::getenv("ANYPS5_VOLUME"), std::getenv("ANYPS5_MUTE_ON_FOCUS_LOSS"));
}

std::uint32_t HostVolumeGain_nid_no_patch() {
    return publishedGain.load(std::memory_order_acquire);
}
