#ifndef CORE_LIBS_PRX_LIBSCEPAD_INCLUDE_HOSTAUDIOINCONTROL_HPP
#define CORE_LIBS_PRX_LIBSCEPAD_INCLUDE_HOSTAUDIOINCONTROL_HPP

#include <cstdint>

extern "C" void HostAudioInConfigure_nid_no_patch(const char* muteOnFocusLoss);
extern "C" std::uint64_t HostAudioInMuteState_nid_no_patch();

#endif
