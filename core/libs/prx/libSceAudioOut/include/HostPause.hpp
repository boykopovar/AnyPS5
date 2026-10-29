#ifndef CORE_LIBS_PRX_LIBSCEAUDIOOUT_INCLUDE_HOSTPAUSE_HPP
#define CORE_LIBS_PRX_LIBSCEAUDIOOUT_INCLUDE_HOSTPAUSE_HPP

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

extern "C" {

void AudioOutSetHostPause_nid_no_patch(bool paused);
bool AudioOutHostPaused_nid_no_patch();

}

inline const void* AudioOutSilenceWhilePaused(const void* data, std::uint32_t bytes, std::vector<std::uint8_t>& silence) {
    if (!AudioOutHostPaused_nid_no_patch()) return data;
    silence.assign(bytes, 0);
    return silence.data();
}

inline void AudioOutSilenceFloatsWhilePaused(float* samples, std::size_t count) {
    if (AudioOutHostPaused_nid_no_patch()) std::fill_n(samples, count, 0.0f);
}

#endif
