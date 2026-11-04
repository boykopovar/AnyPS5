#include "prx/libSceVideoOut/include/HostVolumeInput.hpp"

#include <cstdio>
#include <exception>
#include <stdexcept>

namespace {

SDL_Event windowEvent(Uint32 windowId, Uint8 kind) {
    SDL_Event event{};
    event.type = SDL_WINDOWEVENT;
    event.window.windowID = windowId;
    event.window.event = kind;
    return event;
}

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void noRequestChangesNothing() {
    HostVolumeConfigure_nid_no_patch(nullptr, nullptr);
    HostVolume after;
    require(!ApplyHostVolumeRequests(Pad::InputRequests{}, &after), "an empty request set asks for nothing");
    require(HostVolumeGain_nid_no_patch() == HostVolume::UnityGain, "an empty request set leaves unity");
    require(after.Volume() == 100 && !after.Muted(), "the out parameter is untouched without a request");
}

void volumeRequestsMoveTheVolume() {
    HostVolumeConfigure_nid_no_patch(nullptr, nullptr);
    HostVolume after;
    Pad::InputRequests down;
    down.volumeDown = true;
    require(ApplyHostVolumeRequests(down, &after), "a volume down request is handled");
    require(after.Volume() == 95 && HostVolumeGain_nid_no_patch() == after.Gain(), "volume down lowers the shared volume by one step");
    Pad::InputRequests up;
    up.volumeUp = true;
    require(ApplyHostVolumeRequests(up, &after), "a volume up request is handled");
    require(after.Volume() == 100 && HostVolumeGain_nid_no_patch() == HostVolume::UnityGain, "volume up returns to exactly unity");
    require(ApplyHostVolumeRequests(up, &after), "a volume up request at the top is still handled");
    require(after.Volume() == 100, "volume up at the top stays at 100");
}

void muteRequestToggles() {
    HostVolumeConfigure_nid_no_patch(nullptr, nullptr);
    HostVolume after;
    Pad::InputRequests mute;
    mute.toggleMute = true;
    require(ApplyHostVolumeRequests(mute, &after) && after.Muted() && HostVolumeGain_nid_no_patch() == 0, "the first mute request silences the output");
    require(ApplyHostVolumeRequests(mute, &after) && !after.Muted() && HostVolumeGain_nid_no_patch() == HostVolume::UnityGain, "the second mute request restores the output");
}

void requestsRunInOrder() {
    HostVolumeConfigure_nid_no_patch(nullptr, nullptr);
    HostVolume after;
    Pad::InputRequests all;
    all.volumeUp = true;
    all.volumeDown = true;
    all.toggleMute = true;
    require(ApplyHostVolumeRequests(all, &after), "a combined request is handled");
    require(after.Volume() == 95 && after.Muted(), "up, then down, then mute leaves 95 muted");
}

void otherRequestsAreIgnored() {
    HostVolumeConfigure_nid_no_patch(nullptr, nullptr);
    HostVolume after;
    Pad::InputRequests other;
    other.toggleFullscreen = true;
    other.togglePerformanceOverlay = true;
    other.screenshot = true;
    other.toggleMouse = true;
    other.releaseMouse = true;
    require(!ApplyHostVolumeRequests(other, &after), "the other host requests are not volume requests");
    require(HostVolumeGain_nid_no_patch() == HostVolume::UnityGain, "the other host requests leave the volume alone");
}

void focusIsIgnoredByDefault() {
    HostVolumeConfigure_nid_no_patch(nullptr, nullptr);
    HostVolume after;
    require(!ApplyHostFocusEvent(windowEvent(7, SDL_WINDOWEVENT_FOCUS_LOST), 7, &after), "losing focus changes nothing by default");
    require(HostVolumeGain_nid_no_patch() == HostVolume::UnityGain, "the gain stays at unity without the option");
    require(!ApplyHostFocusEvent(windowEvent(7, SDL_WINDOWEVENT_FOCUS_GAINED), 7, &after), "gaining focus changes nothing by default");
    require(HostVolumeGain_nid_no_patch() == HostVolume::UnityGain, "the gain is still unity");
}

void focusLossMutesAndGainRestores() {
    HostVolumeConfigure_nid_no_patch(nullptr, "1");
    HostVolume after;
    require(ApplyHostFocusEvent(windowEvent(7, SDL_WINDOWEVENT_FOCUS_LOST), 7, &after), "losing focus with the option on is a change");
    require(after.FocusMuted() && HostVolumeGain_nid_no_patch() == 0, "the output goes silent");
    require(!ApplyHostFocusEvent(windowEvent(7, SDL_WINDOWEVENT_FOCUS_LOST), 7, &after), "a second focus lost is not a change");
    require(ApplyHostFocusEvent(windowEvent(7, SDL_WINDOWEVENT_FOCUS_GAINED), 7, &after), "gaining focus is a change");
    require(!after.FocusMuted() && HostVolumeGain_nid_no_patch() == HostVolume::UnityGain, "the output returns to exactly unity");
}

void otherWindowEventsAreIgnored() {
    HostVolumeConfigure_nid_no_patch(nullptr, "1");
    HostVolume after;
    for (const Uint8 kind : {SDL_WINDOWEVENT_MINIMIZED, SDL_WINDOWEVENT_RESTORED, SDL_WINDOWEVENT_HIDDEN, SDL_WINDOWEVENT_SHOWN, SDL_WINDOWEVENT_CLOSE, SDL_WINDOWEVENT_RESIZED, SDL_WINDOWEVENT_ENTER, SDL_WINDOWEVENT_LEAVE}) {
        require(!ApplyHostFocusEvent(windowEvent(7, kind), 7, &after), "a window event that is not a focus event changes nothing");
    }
    require(HostVolumeGain_nid_no_patch() == HostVolume::UnityGain, "none of them silenced the output");
    require(!ApplyHostFocusEvent(windowEvent(8, SDL_WINDOWEVENT_FOCUS_LOST), 7, &after), "another window losing focus changes nothing");
    SDL_Event key{};
    key.type = SDL_KEYDOWN;
    key.key.windowID = 7;
    require(!ApplyHostFocusEvent(key, 7, &after), "a key event is not a focus event");
    require(HostVolumeGain_nid_no_patch() == HostVolume::UnityGain, "the output is still at unity");
}

void focusMuteKeepsTheUsersMute() {
    HostVolumeConfigure_nid_no_patch(nullptr, "1");
    HostVolume after;
    Pad::InputRequests mute;
    mute.toggleMute = true;
    ApplyHostVolumeRequests(mute, &after);
    ApplyHostFocusEvent(windowEvent(7, SDL_WINDOWEVENT_FOCUS_LOST), 7, &after);
    ApplyHostFocusEvent(windowEvent(7, SDL_WINDOWEVENT_FOCUS_GAINED), 7, &after);
    require(after.Muted() && HostVolumeGain_nid_no_patch() == 0, "regaining focus does not undo the user's mute");
}

}

int main() try {
    noRequestChangesNothing();
    volumeRequestsMoveTheVolume();
    muteRequestToggles();
    requestsRunInOrder();
    otherRequestsAreIgnored();
    focusIsIgnoredByDefault();
    focusLossMutesAndGainRestores();
    otherWindowEventsAreIgnored();
    focusMuteKeepsTheUsersMute();
    HostVolumeConfigure_nid_no_patch(nullptr, nullptr);
    std::puts("host volume input tests passed");
    return 0;
} catch (const std::exception& error) {
    std::fprintf(stderr, "%s\n", error.what());
    return 1;
}
