#include "prx/libSceAudioIn/include/AudioIn.hpp"
#include "prx/libSceAudioIn/AudioInCapture.hpp"
#include "prx/libSceAudioIn/AudioInDeviceSelection.hpp"
#include <cstdint>
#include <cstddef>
#include <cerrno>
#include <cstdlib>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/PreciseWait.hpp"
#include "SDL.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <cerrno>
#include <mutex>
#include <string>
#include <string_view>
#include <stdexcept>
#include "prx/libScePad/include/HostAudioInControl.hpp"

namespace {

constexpr int AUDIO_IN_ERROR_INVALID_HANDLE = static_cast<int>(0x80260101);
constexpr int AUDIO_IN_ERROR_INVALID_SIZE = static_cast<int>(0x80260102);
constexpr int AUDIO_IN_ERROR_INVALID_FREQ = static_cast<int>(0x80260103);
constexpr int AUDIO_IN_ERROR_INVALID_TYPE = static_cast<int>(0x80260104);
constexpr int AUDIO_IN_ERROR_INVALID_POINTER = static_cast<int>(0x80260105);
constexpr int AUDIO_IN_ERROR_INVALID_PARAM = static_cast<int>(0x80260106);
constexpr int AUDIO_IN_ERROR_PORT_FULL = static_cast<int>(0x80260107);
constexpr int AUDIO_IN_ERROR_INVALID_STATE = static_cast<int>(0x80260109);
constexpr int AUDIO_IN_ERROR_BUSY = static_cast<int>(0x8026010A);
constexpr int AUDIO_IN_ERROR_INVALID_DEVICE = static_cast<int>(0x8026010F);
constexpr int AUDIO_IN_SILENT_STATE_DEVICE_NONE = 1;
constexpr std::uint32_t HANDLE_BASE = 0x30000000;
constexpr std::uint32_t MAX_QUEUED_BLOCKS = 4;
constexpr std::size_t MAX_CAPTURE_BYTES = 0x100u * 2u * sizeof(float);

using Clock = std::chrono::steady_clock;

struct Format {
    SDL_AudioFormat format;
    std::uint8_t channels;
};

struct Port {
    bool used = false;
    bool busy = false;
    std::uint32_t samples = 0;
    std::uint32_t frameBytes = 0;
    SDL_AudioDeviceID device = 0;
    Clock::duration grain{};
    Clock::time_point next{};
    std::int32_t mode = 0;
};

std::mutex g_mutex;
std::array<Port, 8> g_ports{};
bool g_sdlAudio = false;
std::int32_t g_mode = 0;

audioin::detail::CaptureMuteGenerations g_captureMuteGenerations;

bool formatOf(std::uint32_t param, Format& format) {
    static constexpr std::uint8_t channels[] = {1, 1, 2};
    if ((param & 0xf) > 2) return false;
    const std::uint32_t encoding = param & 0xf0;
    if (encoding != 0 && encoding != 0x10) return false;
    format = {static_cast<SDL_AudioFormat>(encoding == 0x10 ? AUDIO_F32SYS : AUDIO_S16SYS), channels[param & 0xf]};
    return true;
}

SDL_AudioDeviceID openDevice(std::uint32_t freq, std::uint32_t samples, const Format& format, std::string_view requestedName) {
    if (!g_sdlAudio) {
        if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) return 0;
        g_sdlAudio = true;
    }
    SDL_AudioSpec desired{};
    desired.freq = static_cast<int>(freq);
    desired.format = format.format;
    desired.channels = format.channels;
    desired.samples = static_cast<Uint16>(samples);
    SDL_AudioSpec obtained{};
    auto selection = audioin::detail::selectCaptureDevice(requestedName, 0,
        [](int index) { return SDL_GetAudioDeviceName(index, 1); });
    if (selection.kind != audioin::detail::CaptureDeviceMatchKind::Default) {
        selection = audioin::detail::selectCaptureDevice(requestedName, SDL_GetNumAudioDevices(1),
            [](int index) { return SDL_GetAudioDeviceName(index, 1); });
    }
    if (selection.kind == audioin::detail::CaptureDeviceMatchKind::Missing ||
        selection.kind == audioin::detail::CaptureDeviceMatchKind::Ambiguous) return 0;
    std::string selectedName;
    const char* deviceName = nullptr;
    if (selection.kind == audioin::detail::CaptureDeviceMatchKind::Found) {
        const char* name = SDL_GetAudioDeviceName(selection.index, 1);
        if (!name) return 0;
        selectedName = name;
        deviceName = selectedName.c_str();
    }
    const SDL_AudioDeviceID device = SDL_OpenAudioDevice(deviceName, 1, &desired, &obtained, 0);
    if (device != 0) SDL_PauseAudioDevice(device, 0);
    return device;
}

int findPort(int handle, Port** port) {
    const std::uint32_t raw = static_cast<std::uint32_t>(handle);
    if ((raw & 0x7f000000u) != HANDLE_BASE || (raw & 0xffu) >= g_ports.size()) return AUDIO_IN_ERROR_INVALID_HANDLE;
    Port& candidate = g_ports[raw & 0xffu];
    if (!candidate.used) return AUDIO_IN_ERROR_INVALID_STATE;
    *port = &candidate;
    return 0;
}

int checkArguments(std::uint32_t type, std::uint32_t index, std::uint32_t len, std::uint32_t freq) {
    if (len != 0x100 && len != 0x80) return AUDIO_IN_ERROR_INVALID_SIZE;
    if (freq != 48000 && freq != 16000) return AUDIO_IN_ERROR_INVALID_FREQ;
    if (type > 5) return AUDIO_IN_ERROR_INVALID_TYPE;
    if (index != 0) return AUDIO_IN_ERROR_INVALID_PARAM;
    return 0;
}

int checkDeviceArguments(std::uint32_t type, std::uint32_t len, std::uint32_t freq) {
    if (type > 5) return AUDIO_IN_ERROR_INVALID_TYPE;
    if (freq != 48000 && freq != 16000) return AUDIO_IN_ERROR_INVALID_FREQ;
    if (len != 0x100 && len != 0x80) return AUDIO_IN_ERROR_INVALID_SIZE;
    return 0;
}

int checkMicLevel(int type, int level) {
    if (static_cast<std::uint32_t>(level) > 100) return AUDIO_IN_ERROR_INVALID_PARAM;
    if (type >= 0 && static_cast<std::uint32_t>(type - 1) > 3) return AUDIO_IN_ERROR_INVALID_DEVICE;
    return 0;
}

int checkMuteType(int type) {
    const std::uint32_t index = static_cast<std::uint32_t>(type) & 0x7fffffffu;
    if (index - 1 > 6 || !(0x6fu >> (index - 1) & 1u)) return AUDIO_IN_ERROR_INVALID_DEVICE;
    return 0;
}

void applyMode(int mode, int value) {
    if (mode == 0) {
        std::lock_guard lock(g_mutex);
        g_mode = value;
    }
}

int openPort(std::uint32_t len, std::uint32_t freq, std::uint32_t param) {
    Format format{};
    if (!formatOf(param, format)) return AUDIO_IN_ERROR_INVALID_PARAM;
    const char* const configuredName = std::getenv("ANYPS5_AUDIO_IN_DEVICE");
    const std::string_view requestedName = configuredName ? std::string_view(configuredName) : std::string_view{};
    const bool explicitDevice = !requestedName.empty() && requestedName != "default";
    std::lock_guard lock(g_mutex);
    for (std::size_t i = g_ports.size(); i-- > 0;) {
        if (g_ports[i].used) continue;
        const auto frameBytes = static_cast<std::uint32_t>(SDL_AUDIO_BITSIZE(format.format) / 8 * format.channels);
        const auto grain = std::chrono::duration_cast<Clock::duration>(std::chrono::microseconds(1000000ull * len / freq));
        const SDL_AudioDeviceID device = openDevice(freq, len, format, requestedName);
        if (explicitDevice && device == 0) return AUDIO_IN_ERROR_INVALID_DEVICE;
        g_ports[i] = {true, false, len, frameBytes, device, grain, {}, g_mode};
        return static_cast<int>(HANDLE_BASE + i);
    }
    return AUDIO_IN_ERROR_PORT_FULL;
}

std::size_t capture(SDL_AudioDeviceID device, std::uint8_t* dest, std::size_t bytes, Clock::time_point deadline) {
    const std::uint64_t muteState = HostAudioInMuteState_nid_no_patch();
    const std::uint64_t generation = muteState >> 1;
    if (g_captureMuteGenerations.Observe(device, generation)) SDL_ClearQueuedAudio(device);
    while (SDL_GetQueuedAudioSize(device) < bytes && Clock::now() < deadline) PreciseSleepUs(1000);
    const std::size_t queued = SDL_GetQueuedAudioSize(device);
    std::array<std::uint8_t, 4096> discard;
    const std::size_t excess = queued > bytes * MAX_QUEUED_BLOCKS ? queued - bytes * MAX_QUEUED_BLOCKS : 0;
    audioin::detail::discardQueuedAudio(excess, discard.data(), discard.size(),
        [device](std::uint8_t* dest, std::uint32_t chunk) { return SDL_DequeueAudio(device, dest, chunk); });
    const std::size_t captured = SDL_DequeueAudio(device, dest, static_cast<Uint32>(bytes));
    const std::uint64_t finalMuteState = HostAudioInMuteState_nid_no_patch();
    const bool muted = (muteState & 1u) != 0 || (finalMuteState & 1u) != 0 || muteState != finalMuteState;
    const std::size_t delivered = audioin::detail::filterCapturedSamples(dest, bytes, captured, muted);
    if (muted) SDL_ClearQueuedAudio(device);
    return delivered;
}

}

extern "C" {

int APS5_VABI sceAudioInInit() {
    return 0;
}

int APS5_VABI sceAudioInGetSilentState(int handle) {
    std::lock_guard lock(g_mutex);
    Port* port;
    if (const int error = findPort(handle, &port)) return error;
    return port->device != 0 ? 0 : AUDIO_IN_SILENT_STATE_DEVICE_NONE;
}

int APS5_VABI sceAudioInInput(int handle, void* dest) {
    const int savedErrno = errno;
    std::unique_lock lock(g_mutex);
    Port* port;
    if (const int error = findPort(handle, &port)) return error;
    if (port->busy) return AUDIO_IN_ERROR_BUSY;
    if (!dest) {
        if (port->device != 0) SDL_ClearQueuedAudio(port->device);
        port->next = {};
        return 0;
    }
    const std::size_t bytes = static_cast<std::size_t>(port->samples) * port->frameBytes;
    if (bytes > MAX_CAPTURE_BYTES) throw std::logic_error("AudioIn capture block exceeds supported format");
    const SDL_AudioDeviceID device = port->device;
    const auto now = Clock::now();
    const auto wake = port->next > now ? port->next : now;
    port->next = wake + port->grain;
    const auto deadline = now + port->grain * MAX_QUEUED_BLOCKS;
    const int samples = static_cast<int>(port->samples);
    port->busy = true;
    lock.unlock();

    std::array<std::uint8_t, MAX_CAPTURE_BYTES> block;
    std::size_t captured = 0;
    if (device != 0) {
        captured = capture(device, block.data(), bytes, deadline);
    } else {
        PreciseSleepUntil(wake);
    }
    std::memset(block.data() + captured, 0, bytes - captured);
    const bool copied = GuestCopyOut_nid_no_patch(dest, block.data(), bytes);
    lock.lock();
    port->busy = false;
    if (!copied) {
        errno = savedErrno;
        return AUDIO_IN_ERROR_INVALID_POINTER;
    }
    return samples;
}

int APS5_VABI sceAudioInOpen(int user_id, uint32_t type, uint32_t index, uint32_t len, uint32_t freq, uint32_t param) {
    (void)user_id;
    if (const int error = checkArguments(type, index, len, freq); error != 0) return error;
    return openPort(len, freq, param);
}

int32_t APS5_VABI sceAudioInClose(int32_t handle) {
    std::lock_guard lock(g_mutex);
    Port* port;
    if (const int error = findPort(handle, &port)) return error;
    if (port->busy) return AUDIO_IN_ERROR_BUSY;
    if (port->device != 0) {
        g_captureMuteGenerations.Forget(port->device);
        SDL_CloseAudioDevice(port->device);
    }
    *port = {};
    return 0;
}

int32_t APS5_VABI sceAudioInHqOpen(int32_t userId, uint32_t type, uint32_t index, uint32_t len, uint32_t freq, uint32_t param) {
    if (freq != 48000) return AUDIO_IN_ERROR_INVALID_FREQ;
    if (len != 128) return AUDIO_IN_ERROR_INVALID_SIZE;
    if (param != 2) return AUDIO_IN_ERROR_INVALID_PARAM;
    if (const int error = checkArguments(type, index, len, freq); error != 0) return error;
    return openPort(len, freq, param);
}

int32_t APS5_VABI sceAudioInAsyncOpen(int32_t userId, uint32_t type, uint32_t index, uint32_t len, uint32_t freq, uint32_t param) {
    if (freq != 48000) return AUDIO_IN_ERROR_INVALID_FREQ;
    if (len != 128) return AUDIO_IN_ERROR_INVALID_SIZE;
    if (const int error = checkArguments(type, index, len, freq); error != 0) return error;
    return openPort(len, freq, param);
}

int32_t APS5_VABI sceAudioInIsPadEmbeddedMic(int32_t handle) {
    std::lock_guard lock(g_mutex);
    Port* port;
    if (const int error = findPort(handle, &port)) return error;
    return 0;
}

int32_t APS5_VABI sceAudioInGetGain(int32_t handle, float* gain) {
    if (!gain) return AUDIO_IN_ERROR_INVALID_POINTER;
    constexpr float defaultGain = 1.0f;
    const int savedErrno = errno;
    if (!GuestCopyOut_nid_no_patch(gain, &defaultGain, sizeof(defaultGain))) {
        errno = savedErrno;
        return AUDIO_IN_ERROR_INVALID_POINTER;
    }
    std::lock_guard lock(g_mutex);
    Port* port;
    if (const int error = findPort(handle, &port)) return error;
    return 0;
}

int32_t APS5_VABI sceAudioInGetRerouteCount(int32_t handle) {
    std::lock_guard lock(g_mutex);
    Port* port;
    if (const int error = findPort(handle, &port)) return error;
    return 0;
}

int APS5_VABI sceAudioInDeviceIdOpen(int userId, std::uint32_t type, std::uint32_t deviceId, std::uint32_t len, std::uint32_t freq, std::uint32_t param) {
    (void)userId;
    (void)deviceId;
    if (const int error = checkDeviceArguments(type, len, freq)) return error;
    return openPort(len, freq, param);
}

int APS5_VABI sceAudioInDeviceIdAsyncOpen(int userId, std::uint32_t type, std::uint32_t deviceId, std::uint32_t len, std::uint32_t freq, std::uint32_t param) {
    return sceAudioInDeviceIdOpen(userId, type, deviceId, len, freq, param);
}

int APS5_VABI sceAudioInSetUsbGain(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAudioInIsSharedDevice(int handle) {
    const std::uint32_t index = static_cast<std::uint32_t>(handle) & 0xffu;
    if (index >= g_ports.size()) return AUDIO_IN_ERROR_INVALID_HANDLE;
    {
        std::lock_guard lock(g_mutex);
        if (!g_ports[index].used) return AUDIO_IN_ERROR_INVALID_STATE;
    }
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAudioInDeviceIdHqOpen(int userId, std::uint32_t type, std::uint32_t deviceId, std::uint32_t len, std::uint32_t freq, std::uint32_t param) {
    return sceAudioInDeviceIdOpen(userId, type, deviceId, len, freq, param);
}

int APS5_VABI sceAudioInDeviceHqOpen(int userId, std::uint32_t deviceId, std::uint32_t len, std::uint32_t freq, std::uint32_t param) {
    (void)userId;
    (void)deviceId;
    if (const int error = checkDeviceArguments(4, len, freq)) return error;
    return openPort(len, freq, param);
}

int APS5_VABI sceAudioInSetConnections(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAudioInSetPortConnections(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAudioInSetPortStatuses(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAudioInGetHandleStatusInfo(int handle, void* info) {
    (void)info;
    Port* port;
    if (const int error = findPort(handle, &port)) return error;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAudioInOpenEx(int user_id, uint32_t type, uint32_t index, uint32_t extra, uint32_t len, uint32_t freq, uint32_t param) {
    (void)extra;
    return sceAudioInOpen(user_id, type, index, len, freq, param);
}

int APS5_VABI sceAudioInHqOpenEx(int user_id, uint32_t type, uint32_t index, uint32_t extra, uint32_t len, uint32_t freq, uint32_t param) {
    (void)extra;
    return sceAudioInOpen(user_id, type, index, len, freq, param);
}

int APS5_VABI sceAudioInDeviceOpenEx(int userId, std::uint32_t type, std::uint32_t deviceId, std::uint32_t extra, std::uint32_t len, std::uint32_t freq, std::uint32_t param) {
    (void)extra;
    return sceAudioInDeviceIdOpen(userId, type, deviceId, len, freq, param);
}

int APS5_VABI sceAudioInChangeAppModuleState(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAudioInSetSparkParam(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAudioInSetSparkSideTone(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAudioInSetCompressorPreGain(void) {
    return 0;
}

int APS5_VABI sceAudioInSetNoiseGateThreshold(void) {
    return 0;
}

int APS5_VABI sceAudioInSetMicLevel(int type, std::uint64_t device, int level, int arg4, int arg5) {
    (void)device;
    (void)arg4;
    (void)arg5;
    if (const int error = checkMicLevel(type, level)) return error;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAudioInSetMicrophoneLevel(int type, std::uint64_t device, int level, int arg) {
    (void)device;
    (void)arg;
    if (const int error = checkMicLevel(type, level)) return error;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAudioInSetDeviceMuteStatus(int type, std::uint64_t device, int mute) {
    (void)device;
    (void)mute;
    if (const int error = checkMuteType(type)) return error;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAudioInSetDevConnection(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAudioInCountPorts(std::int16_t type, std::int16_t index, std::int16_t arg3, std::int32_t arg4) {
    (void)type;
    (void)index;
    (void)arg3;
    (void)arg4;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAudioInSetUserMute(int type, std::uint64_t device, int mute) {
    (void)device;
    (void)mute;
    if (const int error = checkMuteType(type)) return error;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAudioInVmicCreate(int userId, std::uint32_t len, std::uint32_t freq, std::uint32_t param) {
    (void)userId;
    (void)param;
    if (len != 0x100 && len != 0x80) return AUDIO_IN_ERROR_INVALID_SIZE;
    if (freq != 48000 && freq != 16000) return AUDIO_IN_ERROR_INVALID_FREQ;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAudioInVmicDestroy(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAudioInVmicWrite(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAudioInVmicGetStatus(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAudioInVmicSetMute(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAudioInSetMode2(int mode, int value, void* arg) {
    (void)arg;
    applyMode(mode, value);
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAudioInSetMode(int mode, int value) {
    applyMode(mode, value);
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAudioInExtCtrl(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAudioInInputs(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
