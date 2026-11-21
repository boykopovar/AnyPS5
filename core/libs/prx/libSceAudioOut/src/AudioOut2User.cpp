#include "prx/libSceAudioOut/include/AudioOut2.hpp"
#include <cerrno>
#include <cstddef>
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

static bool CopyUserOutput_nid_no_patch(void* destination, const void* value, std::size_t size) {
    const int savedErrno = errno;
    try {
        const bool copied = GuestCopyOut_nid_no_patch(destination, value, size);
        errno = savedErrno;
        return copied;
    } catch (...) {
        errno = savedErrno;
        throw;
    }
}

extern "C" {

int APS5_VABI sceAudioOut2UserCreate(uint32_t user_id, AudioOut2UserHandle* handle) {
    (void)user_id;
    if (!handle) return SCE_AUDIO_OUT2_ERROR_INVALID_ARGUMENT;
    std::lock_guard lock(g_usersLock);
    if (g_nextUser == std::numeric_limits<AudioOut2UserHandle>::max())
        throw std::runtime_error("AudioOut2: user handles exhausted");
    g_users.insert(g_nextUser);
    bool copied = false;
    try {
        copied = CopyUserOutput_nid_no_patch(handle, &g_nextUser, sizeof(g_nextUser));
    } catch (...) {
        g_users.erase(g_nextUser);
        throw;
    }
    if (!copied) {
        g_users.erase(g_nextUser);
        return SCE_AUDIO_OUT2_ERROR_INVALID_ARGUMENT;
    }
    ++g_nextUser;
    return 0;
}

int APS5_VABI sceAudioOut2UserCreateEx(uint32_t user_id, const void* attributes, AudioOut2UserHandle* handle) {
    (void)attributes;
    return sceAudioOut2UserCreate(user_id, handle);
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
    const std::uint32_t context = 0;
    const std::uint32_t port = (1u << 0) | (1u << 1);
    if (contextAttributes && !CopyUserOutput_nid_no_patch(contextAttributes, &context, sizeof(context)))
        return static_cast<int>(0x80268001);
    if (portAttributes && !CopyUserOutput_nid_no_patch(portAttributes, &port, sizeof(port)))
        return static_cast<int>(0x80268001);
    return 0;
}

}
