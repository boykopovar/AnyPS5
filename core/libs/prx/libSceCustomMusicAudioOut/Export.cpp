#include "prx/libSceCustomMusicAudioOut/include/Api.hpp"
#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int ErrorInvalidArgument = static_cast<int>(0x81380001);
constexpr int ErrorNotInitialized = static_cast<int>(0x81380102);
constexpr std::int32_t DefaultLevel = 0x4000;
constexpr double LevelScale = 32768.0;

bool g_initialized = false;
std::int32_t g_portLevel = DefaultLevel;
std::int32_t g_allMusicLevel = DefaultLevel;
bool g_muted = false;

double VolumeFromLevel(std::int32_t level) {
    if (level < 0) {
        return 0.0;
    }
    return std::clamp(static_cast<double>(level) / LevelScale, 0.0, 1.0);
}

bool IsValidGuestWrite(void* pointer, std::size_t size) {
    if (pointer == nullptr) {
        return false;
    }
    const int savedErrno = errno;
    const bool writable = GuestWritable_nid_no_patch(pointer, size != 0 ? size : 1);
    errno = savedErrno;
    return writable;
}

}

extern "C" {

int APS5_VABI sceCustomMusicAudioOutInitialize(int a1, int a2, int a3) {
    if ((a3 != 1 && a3 != 4) || (a2 != 0 && a2 != 1)) {
        return ErrorInvalidArgument;
    }
    g_initialized = true;
    g_portLevel = g_allMusicLevel;
    g_muted = false;
    return 0;
}

int APS5_VABI sceCustomMusicAudioOutFinalize(void) {
    if (!g_initialized) {
        return ErrorNotInitialized;
    }
    g_initialized = false;
    g_portLevel = DefaultLevel;
    g_muted = false;
    return 0;
}

int APS5_VABI sceCustomMusicAudioOutGetVolume(double* out) {
    if (!IsValidGuestWrite(out, sizeof(*out))) {
        return ErrorInvalidArgument;
    }
    if (!g_initialized) {
        return ErrorNotInitialized;
    }
    const double val = VolumeFromLevel(g_portLevel);
    const int savedErrno = errno;
    if (!GuestCopyOut_nid_no_patch(out, &val, sizeof(val))) {
        errno = savedErrno;
        return ErrorInvalidArgument;
    }
    errno = savedErrno;
    return 0;
}

int APS5_VABI sceCustomMusicAudioOutSetVolume(double v) {
    v = std::clamp(v, 0.0, 1.0);
    if (!g_initialized) {
        return ErrorNotInitialized;
    }
    g_portLevel = static_cast<std::int32_t>(std::round(v * LevelScale));
    return 0;
}

int APS5_VABI sceCustomMusicAudioOutGetMuted(bool* out) {
    if (!IsValidGuestWrite(out, sizeof(*out))) {
        return ErrorInvalidArgument;
    }
    if (!g_initialized) {
        return ErrorNotInitialized;
    }
    const bool val = g_muted;
    const int savedErrno = errno;
    if (!GuestCopyOut_nid_no_patch(out, &val, sizeof(val))) {
        errno = savedErrno;
        return ErrorInvalidArgument;
    }
    errno = savedErrno;
    return 0;
}

int APS5_VABI sceCustomMusicAudioOutSetMuted(bool m) {
    if (!g_initialized) {
        return ErrorNotInitialized;
    }
    g_muted = m;
    return 0;
}

int APS5_VABI sceCustomMusicAudioOutGetAllMusicVolume(double* out) {
    if (!IsValidGuestWrite(out, sizeof(*out))) {
        return ErrorInvalidArgument;
    }
    if (!g_initialized) {
        return ErrorNotInitialized;
    }
    const double val = VolumeFromLevel(g_allMusicLevel);
    const int savedErrno = errno;
    if (!GuestCopyOut_nid_no_patch(out, &val, sizeof(val))) {
        errno = savedErrno;
        return ErrorInvalidArgument;
    }
    errno = savedErrno;
    return 0;
}

int APS5_VABI sceCustomMusicAudioOutSetAllMusicVolume(double v) {
    v = std::clamp(v, 0.0, 1.0);
    if (!g_initialized) {
        return ErrorNotInitialized;
    }
    g_allMusicLevel = static_cast<std::int32_t>(std::round(v * LevelScale));
    g_portLevel = g_allMusicLevel;
    return 0;
}

int APS5_VABI sceCustomMusicAudioOutGetLastOutputTime(void* out) {
    if (!IsValidGuestWrite(out, 1)) {
        return ErrorInvalidArgument;
    }
    if (!g_initialized) {
        return ErrorNotInitialized;
    }
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCustomMusicAudioOutOutput(void* buf) {
    if (!g_initialized) {
        return ErrorNotInitialized;
    }
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
