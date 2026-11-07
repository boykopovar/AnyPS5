#ifndef CORE_LIBS_PRX_LIBSCEPAD_INCLUDE_HOSTVOLUMECONTROL_HPP
#define CORE_LIBS_PRX_LIBSCEPAD_INCLUDE_HOSTVOLUMECONTROL_HPP

#include <cstdint>

#include "prx/libScePad/include/HostVolume.hpp"

enum class HostVolumeCommand {
    VolumeUp,
    VolumeDown,
    ToggleMute
};

extern "C" bool HostVolumeApply_nid_no_patch(HostVolumeCommand command, HostVolume* after);
extern "C" bool HostVolumeFocus_nid_no_patch(bool focused, HostVolume* after);
extern "C" void HostVolumeConfigure_nid_no_patch(const char* initialVolume, const char* muteOnFocusLoss);
extern "C" void HostVolumeConfigureFromEnvironment_nid_no_patch();
extern "C" std::uint32_t HostVolumeGain_nid_no_patch();

#endif
