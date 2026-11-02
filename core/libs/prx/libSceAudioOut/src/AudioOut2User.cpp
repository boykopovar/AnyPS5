#include "prx/libSceAudioOut/include/AudioOut2.hpp"
#include <cstdint>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include <limits>
#include <mutex>
#include <stdexcept>
#include <unordered_set>

static constexpr int SCE_AUDIO_OUT2_ERROR_INVALID_ARGUMENT = static_cast<int>(0x80260502);

static std::mutex g_usersLock;
static AudioOut2UserHandle g_nextUser = 1;
static std::unordered_set<AudioOut2UserHandle> g_users;

extern "C" {

int APS5_VABI sceAudioOut2UserCreate(uint32_t user_id, AudioOut2UserHandle* handle) {
    (void)user_id;
    if (!handle) return SCE_AUDIO_OUT2_ERROR_INVALID_ARGUMENT;
    std::lock_guard lock(g_usersLock);
    if (g_nextUser == std::numeric_limits<AudioOut2UserHandle>::max())
        throw std::runtime_error("AudioOut2: user handles exhausted");
    g_users.insert(g_nextUser);
    *handle = g_nextUser++;
    return 0;
}

int APS5_VABI sceAudioOut2UserDestroy(AudioOut2UserHandle handle) {
    std::lock_guard lock(g_usersLock);
    g_users.erase(handle);
    return 0;
}

int APS5_VABI sceAudioOut2UserGetSupportedAttributes(AudioOut2UserHandle handle, uint32_t* contextAttributes, uint32_t* portAttributes) {
    if (handle == 0 || handle == std::numeric_limits<AudioOut2UserHandle>::max()) return static_cast<int>(0x80268010);
    if (!contextAttributes && !portAttributes) return static_cast<int>(0x80268001);
    std::lock_guard lock(g_usersLock);
    if (!g_users.contains(handle))
        throw std::runtime_error("sceAudioOut2UserGetSupportedAttributes: unknown user handle");
    if (contextAttributes) *contextAttributes = 0;
    if (portAttributes) *portAttributes = (1u << 0) | (1u << 1);
    return 0;
}

}
