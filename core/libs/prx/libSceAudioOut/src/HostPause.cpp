#include "prx/libSceAudioOut/include/HostPause.hpp"

#include <atomic>

namespace {

std::atomic<bool> hostPaused{false};

}

void AudioOutSetHostPause_nid_no_patch(bool paused) {
    hostPaused.store(paused, std::memory_order_release);
}

bool AudioOutHostPaused_nid_no_patch() {
    return hostPaused.load(std::memory_order_acquire);
}
