#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/PreciseWait.hpp"
#include "SDL.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <mutex>
#include <stdexcept>

namespace {

constexpr int AUDIO_IN_ERROR_INVALID_HANDLE = static_cast<int>(0x80260101);
constexpr int AUDIO_IN_ERROR_INVALID_SIZE = static_cast<int>(0x80260102);
constexpr int AUDIO_IN_ERROR_INVALID_FREQ = static_cast<int>(0x80260103);
constexpr int AUDIO_IN_ERROR_INVALID_TYPE = static_cast<int>(0x80260104);
constexpr int AUDIO_IN_ERROR_INVALID_POINTER = static_cast<int>(0x80260105);
constexpr int AUDIO_IN_ERROR_INVALID_PARAM = static_cast<int>(0x80260106);
constexpr int AUDIO_IN_ERROR_PORT_FULL = static_cast<int>(0x80260107);
constexpr int AUDIO_IN_ERROR_BUSY = static_cast<int>(0x8026010A);
constexpr int AUDIO_IN_SILENT_STATE_DEVICE_NONE = 1;
constexpr std::uint32_t MAX_QUEUED_BLOCKS = 4;

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
};

std::mutex g_mutex;
std::array<Port, 8> g_ports{};
bool g_sdlAudio = false;

bool formatOf(std::uint32_t param, Format& format) {
    switch (param) {
        case 1: format = {AUDIO_S16SYS, 1}; return true;
        case 2: format = {AUDIO_S16SYS, 2}; return true;
        case 0x10:
        case 0x11: format = {AUDIO_F32SYS, 1}; return true;
        case 0x12: format = {AUDIO_F32SYS, 2}; return true;
        default: return false;
    }
}

SDL_AudioDeviceID openDevice(std::uint32_t freq, std::uint32_t samples, const Format& format) {
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
    const SDL_AudioDeviceID device = SDL_OpenAudioDevice(nullptr, 1, &desired, &obtained, 0);
    if (device != 0) SDL_PauseAudioDevice(device, 0);
    return device;
}

Port* find(int handle) {
    if (handle <= 0 || static_cast<std::size_t>(handle) > g_ports.size()) return nullptr;
    Port& port = g_ports[static_cast<std::size_t>(handle - 1)];
    return port.used ? &port : nullptr;
}

int checkArguments(std::uint32_t type, std::uint32_t index, std::uint32_t len, std::uint32_t freq) {
    if (type != 0 && type != 1) return AUDIO_IN_ERROR_INVALID_TYPE;
    if (index != 0) return AUDIO_IN_ERROR_INVALID_PARAM;
    if (len != 128 && len != 256) return AUDIO_IN_ERROR_INVALID_SIZE;
    if (freq != 48000 && freq != 16000) return AUDIO_IN_ERROR_INVALID_FREQ;
    return 0;
}

int openPort(std::uint32_t len, std::uint32_t freq, std::uint32_t param) {
    Format format{};
    if (!formatOf(param, format)) return AUDIO_IN_ERROR_INVALID_PARAM;
    std::lock_guard lock(g_mutex);
    for (std::size_t i = 0; i < g_ports.size(); ++i) {
        if (g_ports[i].used) continue;
        const auto frameBytes = static_cast<std::uint32_t>(SDL_AUDIO_BITSIZE(format.format) / 8 * format.channels);
        const auto grain = std::chrono::duration_cast<Clock::duration>(std::chrono::microseconds(1000000ull * len / freq));
        g_ports[i] = {true, false, len, frameBytes, openDevice(freq, len, format), grain, {}};
        return static_cast<int>(i + 1);
    }
    return AUDIO_IN_ERROR_PORT_FULL;
}

std::size_t capture(SDL_AudioDeviceID device, std::uint8_t* dest, std::size_t bytes, Clock::time_point deadline) {
    while (SDL_GetQueuedAudioSize(device) < bytes && Clock::now() < deadline) PreciseSleepUs(1000);
    const std::size_t queued = SDL_GetQueuedAudioSize(device);
    std::array<std::uint8_t, 4096> discard;
    for (std::size_t excess = queued > bytes * MAX_QUEUED_BLOCKS ? queued - bytes * MAX_QUEUED_BLOCKS : 0; excess > 0;) {
        const auto chunk = static_cast<Uint32>(std::min(excess, discard.size()));
        excess -= SDL_DequeueAudio(device, discard.data(), chunk);
    }
    return SDL_DequeueAudio(device, dest, static_cast<Uint32>(bytes));
}

}

extern "C" {

int APS5_VABI sceAudioInInit() {
    return 0;
}

int APS5_VABI sceAudioInGetSilentState(int handle) {
    std::lock_guard lock(g_mutex);
    const Port* port = find(handle);
    if (!port) return AUDIO_IN_ERROR_INVALID_HANDLE;
    return port->device != 0 ? 0 : AUDIO_IN_SILENT_STATE_DEVICE_NONE;
}

int APS5_VABI sceAudioInInput(int handle, void* dest) {
    std::unique_lock lock(g_mutex);
    Port* port = find(handle);
    if (!port) return AUDIO_IN_ERROR_INVALID_HANDLE;
    if (port->busy) return AUDIO_IN_ERROR_BUSY;
    if (!dest) {
        if (port->device != 0) SDL_ClearQueuedAudio(port->device);
        port->next = {};
        return 0;
    }
    const std::size_t bytes = static_cast<std::size_t>(port->samples) * port->frameBytes;
    const SDL_AudioDeviceID device = port->device;
    const auto now = Clock::now();
    const auto wake = port->next > now ? port->next : now;
    port->next = wake + port->grain;
    const auto deadline = now + port->grain * MAX_QUEUED_BLOCKS;
    const int samples = static_cast<int>(port->samples);
    port->busy = true;
    lock.unlock();

    auto* out = static_cast<std::uint8_t*>(dest);
    std::size_t captured = 0;
    if (device != 0) {
        captured = capture(device, out, bytes, deadline);
    } else {
        PreciseSleepUntil(wake);
    }
    std::memset(out + captured, 0, bytes - captured);
    lock.lock();
    port->busy = false;
    return samples;
}

int APS5_VABI sceAudioInOpen(int user_id, uint32_t type, uint32_t index, uint32_t len, uint32_t freq, uint32_t param) {
    (void)user_id;
    if (const int error = checkArguments(type, index, len, freq); error != 0) return error;
    return openPort(len, freq, param);
}

int32_t APS5_VABI sceAudioInClose(int32_t handle) {
    std::lock_guard lock(g_mutex);
    Port* port = find(handle);
    if (!port) return AUDIO_IN_ERROR_INVALID_HANDLE;
    if (port->busy) return AUDIO_IN_ERROR_BUSY;
    if (port->device != 0) SDL_CloseAudioDevice(port->device);
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
    if (!find(handle)) return AUDIO_IN_ERROR_INVALID_HANDLE;
    return 0;
}

int32_t APS5_VABI sceAudioInGetGain(int32_t handle, float* gain) {
    if (!gain) return AUDIO_IN_ERROR_INVALID_POINTER;
    *gain = 1.0f;
    std::lock_guard lock(g_mutex);
    return find(handle) ? 0 : AUDIO_IN_ERROR_INVALID_HANDLE;
}

int32_t APS5_VABI sceAudioInGetRerouteCount(int32_t handle) {
    std::lock_guard lock(g_mutex);
    return find(handle) ? 0 : AUDIO_IN_ERROR_INVALID_HANDLE;
}

int APS5_VABI sceAudioInDeviceIdOpen(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAudioInDeviceIdAsyncOpen(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAudioInSetUsbGain(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAudioInIsSharedDevice(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAudioInDeviceIdHqOpen(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAudioInDeviceHqOpen(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
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

int APS5_VABI sceAudioInGetHandleStatusInfo(void) {
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

int APS5_VABI sceAudioInDeviceOpenEx(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
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

int APS5_VABI sceAudioInSetMicLevel(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAudioInSetDeviceMuteStatus(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAudioInSetDevConnection(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAudioInCountPorts(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAudioInSetUserMute(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAudioInVmicCreate(void) {
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

int APS5_VABI sceAudioInSetMode(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAudioInSetMode2(void) {
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
