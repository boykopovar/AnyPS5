#ifndef CORE_LIBS_PRX_LIBSCEVIDEOOUT_INCLUDE_HOSTVOLUMEINPUT_HPP
#define CORE_LIBS_PRX_LIBSCEVIDEOOUT_INCLUDE_HOSTVOLUMEINPUT_HPP

#include "SDL_events.h"

#include "prx/libScePad/include/HostVolumeControl.hpp"
#include "prx/libScePad/include/KeyboardMouse.hpp"

inline bool ApplyHostVolumeRequests(const Pad::InputRequests& requests, HostVolume* after) {
    bool requested = false;
    if (requests.volumeUp) {
        HostVolumeApply_nid_no_patch(HostVolumeCommand::VolumeUp, after);
        requested = true;
    }
    if (requests.volumeDown) {
        HostVolumeApply_nid_no_patch(HostVolumeCommand::VolumeDown, after);
        requested = true;
    }
    if (requests.toggleMute) {
        HostVolumeApply_nid_no_patch(HostVolumeCommand::ToggleMute, after);
        requested = true;
    }
    return requested;
}

inline bool ApplyHostFocusEvent(const SDL_Event& event, Uint32 windowId, HostVolume* after) {
    if (event.type != SDL_WINDOWEVENT || event.window.windowID != windowId) return false;
    if (event.window.event == SDL_WINDOWEVENT_FOCUS_LOST) return HostVolumeFocus_nid_no_patch(false, after);
    if (event.window.event == SDL_WINDOWEVENT_FOCUS_GAINED) return HostVolumeFocus_nid_no_patch(true, after);
    return false;
}

#endif
