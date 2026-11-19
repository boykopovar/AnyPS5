#include "prx/libScePad/include/HostVolumeControl.hpp"
#include "prx/libScePad/include/HostAudioInControl.hpp"

#include <atomic>
#include <cstdlib>
#include <mutex>

#include "prx/libScePad/include/HostVolumeSettings.hpp"

namespace {

std::mutex stateMutex;
HostVolume state;
std::atomic<std::uint32_t> publishedGain{HostVolume::UnityGain};
std::atomic<bool> publishedFocus{true};
bool muteAudioInOnFocusLoss = false;
std::atomic<std::uint64_t> publishedAudioInMuteState{0};

void publish() {
    publishedGain.store(state.Gain(), std::memory_order_release);
    publishedFocus.store(state.Focused(), std::memory_order_release);
    const bool muted = muteAudioInOnFocusLoss && !state.Focused();
    const std::uint64_t previous = publishedAudioInMuteState.load(std::memory_order_relaxed);
    const std::uint64_t generation = (previous >> 1) + ((previous & 1u) != muted ? 1u : 0u);
    publishedAudioInMuteState.store((generation << 1) | static_cast<std::uint64_t>(muted), std::memory_order_release);
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
    HostAudioInConfigure_nid_no_patch(std::getenv("ANYPS5_AUDIO_IN_MUTE_ON_FOCUS_LOSS"));
}

std::uint32_t HostVolumeGain_nid_no_patch() {
    return publishedGain.load(std::memory_order_acquire);
}

bool HostVolumeFocused_nid_no_patch() {
    return publishedFocus.load(std::memory_order_acquire);
}

void HostAudioInConfigure_nid_no_patch(const char* muteOnFocusLoss) {
    const bool enabled = muteOnFocusLoss != nullptr && ParseHostVolumeFlag("ANYPS5_AUDIO_IN_MUTE_ON_FOCUS_LOSS", muteOnFocusLoss);
    std::lock_guard lock(stateMutex);
    muteAudioInOnFocusLoss = enabled;
    publish();
}

std::uint64_t HostAudioInMuteState_nid_no_patch() {
    return publishedAudioInMuteState.load(std::memory_order_acquire);
}
