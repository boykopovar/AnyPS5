#include "prx/libSceCustomMusicAudioOut/include/Api.hpp"
#include <algorithm>
#include <array>
#include <cerrno>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <new>
#include <vector>
#include "prx/libSceAudioOut/include/AudioOut.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int ErrorInvalidArgument = static_cast<int>(0x81380001);
constexpr int ErrorOutOfMemory = static_cast<int>(0x81380002);
constexpr int ErrorAudioOut = static_cast<int>(0x81380003);
constexpr int ErrorBusy = static_cast<int>(0x81380004);
constexpr int ErrorNotInitialized = static_cast<int>(0x81380102);
constexpr int AudioOutInvalidSize = static_cast<int>(0x80260006);
constexpr int AudioOutOutOfMemory = static_cast<int>(0x8026000d);
constexpr int AudioOutBusy = static_cast<int>(0x80260002);
constexpr int SystemUser = 0xff;
constexpr int BackgroundMusic = 1;
constexpr int StereoS16 = 1;
constexpr int StereoFloat = 4;
constexpr std::uint32_t ExtendedAttribute = 0x10000;
constexpr std::uint32_t SampleRate = 48000;
constexpr int MaxHostFrames = 32768;
constexpr std::int32_t DefaultLevel = 0x4000;
constexpr double LevelScale = 32768.0;

std::mutex g_mutex;
std::once_flag g_audioOutInit;
int g_port = 0;
std::size_t g_bufferBytes = 0;
std::int32_t g_portLevel = DefaultLevel;
std::int32_t g_allMusicLevel = DefaultLevel;
bool g_muted = false;

int MapAudioOutStatus(int status) {
    switch (status) {
    case 0: return 0;
    case AudioOutInvalidSize: return ErrorInvalidArgument;
    case AudioOutOutOfMemory: return ErrorOutOfMemory;
    case AudioOutBusy: return ErrorBusy;
    default: return ErrorAudioOut;
    }
}

int ApplyLevel(int level) {
    std::array<int, 8> levels;
    levels.fill(level);
    return MapAudioOutStatus(sceAudioOutSetVolume(g_port, 0xff, levels.data()));
}

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
    const bool writable = GuestWritable_nid_no_patch(pointer, size);
    errno = savedErrno;
    return writable;
}

}

extern "C" {

int APS5_VABI sceCustomMusicAudioOutInitialize(int frames, int attribute, int format) {
    if ((format != StereoS16 && format != StereoFloat) || (attribute != 0 && attribute != 1)) {
        return ErrorInvalidArgument;
    }
    const std::lock_guard lock(g_mutex);
    std::call_once(g_audioOutInit, [] { sceAudioOutInit(); });
    if (frames < 0 || frames > MaxHostFrames) {
        return AudioOutInvalidSize;
    }
    const auto params = static_cast<std::uint32_t>(format) | (attribute == 1 ? ExtendedAttribute : 0);
    const int port = sceAudioOutOpen(SystemUser, BackgroundMusic, 0, static_cast<std::uint32_t>(frames), SampleRate, params);
    if (port < 0) {
        return port;
    }
    if (g_port != 0) {
        sceAudioOutClose(g_port);
    }
    g_port = port;
    const std::size_t sampleBytes = format == StereoS16 ? sizeof(std::int16_t) : sizeof(float);
    g_bufferBytes = static_cast<std::size_t>(frames) * 2 * sampleBytes;
    g_portLevel = g_allMusicLevel;
    g_muted = false;
    ApplyLevel(g_portLevel);
    return 0;
}

int APS5_VABI sceCustomMusicAudioOutFinalize(void) {
    const std::lock_guard lock(g_mutex);
    if (g_port == 0) {
        return ErrorNotInitialized;
    }
    const int status = MapAudioOutStatus(sceAudioOutClose(g_port));
    if (status != 0) {
        return status;
    }
    g_port = 0;
    g_bufferBytes = 0;
    g_portLevel = DefaultLevel;
    g_muted = false;
    return 0;
}

int APS5_VABI sceCustomMusicAudioOutGetVolume(double* out) {
    if (!IsValidGuestWrite(out, sizeof(*out))) {
        return ErrorInvalidArgument;
    }
    const std::lock_guard lock(g_mutex);
    if (g_port == 0) {
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
    const std::lock_guard lock(g_mutex);
    if (g_port == 0) {
        return ErrorNotInitialized;
    }
    g_portLevel = static_cast<std::int32_t>(std::round(v * LevelScale));
    return ApplyLevel(g_portLevel);
}

int APS5_VABI sceCustomMusicAudioOutGetMuted(bool* out) {
    if (!IsValidGuestWrite(out, sizeof(*out))) {
        return ErrorInvalidArgument;
    }
    const std::lock_guard lock(g_mutex);
    if (g_port == 0) {
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
    const std::lock_guard lock(g_mutex);
    if (g_port == 0) {
        return ErrorNotInitialized;
    }
    const int status = ApplyLevel(m ? 0 : g_portLevel);
    g_muted = m;
    return status;
}

int APS5_VABI sceCustomMusicAudioOutGetAllMusicVolume(double* out) {
    if (!IsValidGuestWrite(out, sizeof(*out))) {
        return ErrorInvalidArgument;
    }
    const std::lock_guard lock(g_mutex);
    if (g_port == 0) {
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
    const std::lock_guard lock(g_mutex);
    if (g_port == 0) {
        return ErrorNotInitialized;
    }
    g_allMusicLevel = static_cast<std::int32_t>(std::round(v * LevelScale));
    g_portLevel = g_allMusicLevel;
    return ApplyLevel(g_portLevel);
}

int APS5_VABI sceCustomMusicAudioOutGetLastOutputTime(std::uint64_t* out) {
    if (!IsValidGuestWrite(out, sizeof(*out))) {
        return ErrorInvalidArgument;
    }
    const std::lock_guard lock(g_mutex);
    if (g_port == 0) {
        return ErrorNotInitialized;
    }
    return MapAudioOutStatus(sceAudioOutGetLastOutputTime(g_port, out));
}

int APS5_VABI sceCustomMusicAudioOutOutput(const void* buf) try {
    const std::lock_guard lock(g_mutex);
    if (g_port == 0) {
        return ErrorNotInitialized;
    }
    std::vector<std::byte> samples;
    if (buf != nullptr) {
        samples.resize(g_bufferBytes);
        if (!GuestCopyIn_nid_no_patch(samples.data(), buf, samples.size())) {
            return ErrorAudioOut;
        }
    }
    const int written = sceAudioOutOutput(g_port, buf == nullptr ? nullptr : samples.data());
    return written < 0 ? MapAudioOutStatus(written) : 0;
} catch (const std::bad_alloc&) {
    return ErrorOutOfMemory;
}

}
