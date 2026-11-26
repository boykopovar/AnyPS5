#include "prx/libSceCustomMusicAudioOut/include/Api.hpp"
#include "prx/libSceAudioOut/libSceAudioOut.hpp"
#include <algorithm>
#include <array>
#include <cerrno>
#include <cmath>
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int ErrorInvalidArgument = static_cast<int>(0x81380001);
constexpr int ErrorPortFull = static_cast<int>(0x81380002);
constexpr int ErrorInternal = static_cast<int>(0x81380003);
constexpr int ErrorOutOfMemory = static_cast<int>(0x81380004);
constexpr int ErrorNotInitialized = static_cast<int>(0x81380102);

constexpr int AudioOutErrorOutOfMemory = static_cast<int>(0x80260002);
constexpr int AudioOutErrorInvalidArg = static_cast<int>(0x80260006);
constexpr int AudioOutErrorPortFull = static_cast<int>(0x8026000D);
constexpr int AudioOutErrorAlreadyInitialized = static_cast<int>(0x8026000E);

constexpr std::int32_t DefaultLevel = 0x4000;
constexpr double LevelScale = 32768.0;
constexpr std::uint32_t DefaultSampleRate = 48000;

bool g_audioOutInited = false;
bool g_initialized = false;
int g_portHandle = -1;
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

int MapAudioOutError(int rc) {
    if (rc >= 0) {
        return 0;
    }
    if (rc == AudioOutErrorOutOfMemory) {
        return ErrorOutOfMemory;
    }
    if (rc == AudioOutErrorInvalidArg) {
        return ErrorInvalidArgument;
    }
    if (rc == AudioOutErrorPortFull) {
        return ErrorPortFull;
    }
    return ErrorInternal;
}

void ApplyPortVolume() {
    if (g_portHandle < 0) {
        return;
    }
    std::array<int, 8> volumes{};
    volumes.fill(g_muted ? 0 : g_portLevel);
    sceAudioOutSetVolume(g_portHandle, 0xFFu, volumes.data());
}

}

extern "C" {

int APS5_VABI sceCustomMusicAudioOutInitialize(int a1, int a2, int a3) {
    if ((a3 != 1 && a3 != 4) || (a2 != 0 && a2 != 1)) {
        return ErrorInvalidArgument;
    }
    if (!g_audioOutInited) {
        const int initRc = sceAudioOutInit();
        if (initRc < 0 && initRc != AudioOutErrorAlreadyInitialized) {
            return MapAudioOutError(initRc);
        }
        g_audioOutInited = true;
    }
    if (g_portHandle >= 0) {
        sceAudioOutClose(g_portHandle);
        g_portHandle = -1;
        g_initialized = false;
    }
    const std::uint32_t param = a2 == 1 ? (static_cast<std::uint32_t>(a3) | 0x10000u) : static_cast<std::uint32_t>(a3);
    const int handle = sceAudioOutOpen(0xFF, 1, 0, static_cast<std::uint32_t>(a1), DefaultSampleRate, param);
    if (handle < 0) {
        return MapAudioOutError(handle);
    }
    g_portHandle = handle;
    g_initialized = true;
    g_portLevel = g_allMusicLevel;
    g_muted = false;
    ApplyPortVolume();
    return 0;
}

int APS5_VABI sceCustomMusicAudioOutFinalize(void) {
    if (!g_initialized) {
        return ErrorNotInitialized;
    }
    const int rc = g_portHandle >= 0 ? sceAudioOutClose(g_portHandle) : 0;
    g_portHandle = -1;
    g_initialized = false;
    g_portLevel = DefaultLevel;
    g_muted = false;
    return MapAudioOutError(rc);
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
    ApplyPortVolume();
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
    ApplyPortVolume();
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
    ApplyPortVolume();
    return 0;
}

int APS5_VABI sceCustomMusicAudioOutGetLastOutputTime(void* out) {
    if (!IsValidGuestWrite(out, sizeof(std::uint64_t))) {
        return ErrorInvalidArgument;
    }
    if (!g_initialized || g_portHandle < 0) {
        return ErrorNotInitialized;
    }
    const int rc = sceAudioOutGetLastOutputTime(g_portHandle, static_cast<std::uint64_t*>(out));
    return MapAudioOutError(rc);
}

int APS5_VABI sceCustomMusicAudioOutOutput(void* buf) {
    if (!g_initialized || g_portHandle < 0) {
        return ErrorNotInitialized;
    }
    const int rc = sceAudioOutOutput(g_portHandle, buf);
    if (rc > 0) {
        return rc;
    }
    return MapAudioOutError(rc);
}

}
