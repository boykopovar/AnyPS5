#include <cstdint>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include <atomic>

static constexpr int SCE_AUDIO_OUT2_ERROR_INVALID_ARGUMENT = static_cast<int>(0x80260502);

static std::atomic<AudioOut2UserHandle> g_nextUser{1};

extern "C" {

int APS5_VABI sceAudioOut2UserCreate(uint32_t user_id, AudioOut2UserHandle* handle) {
    (void)user_id;
    if (!handle) return SCE_AUDIO_OUT2_ERROR_INVALID_ARGUMENT;
    *handle = g_nextUser.fetch_add(1, std::memory_order_relaxed);
    return 0;
}

int APS5_VABI sceAudioOut2UserDestroy(AudioOut2UserHandle handle) {
    (void)handle;
    return 0;
}

}
