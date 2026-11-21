extern "C" {
#include "prx/libSceAudioOut/include/AudioOut.hpp"
#include "prx/libSceAudioOut/include/OutputDevice.hpp"
#include <algorithm>
#include <cstdlib>
#include <array>
#include <atomic>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <string>
#include <vector>
#include <prx/libc/include/General.hpp>

#include "SDL.h"
#include "SceTypes.hpp"
#include "prx/libc/include/PreciseWait.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include "prx/libkernel/Time/include/Time.hpp"
#include "prx/libSceAudioOut/include/AudioOutSdl.hpp"
#include "prx/libSceAudioOut/include/HostGain.hpp"
#include "prx/libScePad/include/HostVolumeControl.hpp"
#include "AudioOutShutdown.hpp"
#include "AudioOut2PadMix.hpp"
#include "prx/libScePad/include/PadState.hpp"
#include "prx/libScePad/include/PadRumbleIntensity.hpp"

static constexpr int PORT_TYPE_MAIN = 0;
static constexpr int PORT_TYPE_BGM = 1;
static constexpr int PORT_TYPE_VOICE = 2;
static constexpr int PORT_TYPE_PERSONAL = 3;
static constexpr int PORT_TYPE_PADSPK = 4;
static constexpr int PORT_TYPE_EX = 6;
static constexpr int PORT_TYPE_SYS = 7;
static constexpr int PORT_TYPE_VIBRATION = 10;
static constexpr int PORT_TYPE_AUDIO3D = 126;
static constexpr int PORT_TYPE_AUX = 127;

static constexpr int PORTS_MAX = 32;
static constexpr int DEFAULT_VOLUME = 32768;
static constexpr int DEFAULT_PADSPK_MIX_LEVEL = 11626;
static constexpr std::uint32_t FORMAT_MASK = 0xFFu;
static constexpr std::uint64_t TARGET_LATENCY_US = 40000;
static constexpr std::uint64_t DRAIN_TIMEOUT_US = 200000;
static constexpr std::uint64_t DRAIN_SLEEP_US = 1000;
static constexpr std::uint64_t DEVICE_RETRY_US = 2000000;
static constexpr int MONITOR_TYPE_HDMI = 1;
static constexpr std::uint32_t HDMI_MONITOR_INFO_SIZE = 0x180;
static constexpr std::uint32_t EX_MONITOR_INFO_SIZE = 0x400;
static constexpr std::uint32_t EX_OPEN_MODE_MAX = 10;
static constexpr std::uint32_t EX_OPEN_MODES = 0x61F;
static constexpr std::uint32_t EX_CONFIGURE_MODE_DEFAULT = 0xFF;
static constexpr std::uint32_t EX_CONFIGURE_TARGET_DEFAULT = 0xFF;
static constexpr int ERROR_EX_INVALID_MODE = -2144993259;
static constexpr int ERROR_EX_INVALID_CONTROL = -2144960503;
static constexpr std::uint32_t SYS_OPEN_MODE_TRUEHD = 5;
static constexpr std::uint32_t SYS_CONFIGURE_TYPE_HDMI = 1;
static constexpr std::uint32_t SYS_CONFIGURE_TARGET_HDMI = 1;
static constexpr std::uint32_t SYS_PORT_SAMPLES = 1024;
static constexpr std::uint32_t SYS_PORT_FRAME_RATE = 192000;

enum class Format {
    Unknown,
    S16Mono,
    S16Stereo,
    S16_8Ch,
    F32Mono,
    F32Stereo,
    F32_8Ch,
    S16_8ChStd,
    F32_8ChStd,
};

int APS5_VABI sceAudioOutClose(int handle) {
    if (handle <= 0 || handle > PORTS_MAX) return -2144993277;
    std::lock_guard<std::mutex> outputLock(g_outputMutexes[handle - 1]);
    std::lock_guard<std::mutex> lock(g_mutex);
    Port* port = getPort(handle);
    if (port == nullptr) {
        return -2144993277;
    }
    closeDevice(*port);
    if (port->type == PORT_TYPE_VIBRATION) PadClearLegacyAudioHaptics_nid_postfix(handle);
    *port = Port{};
    return 0;
}

int APS5_VABI sceAudioOutOutput(int handle, const void* ptr) try {
    const AudioOutCall call;
    if (handle <= 0 || handle > PORTS_MAX) return -2144993277;
    std::lock_guard<std::mutex> outputLock(g_outputMutexes[handle - 1]);
    std::unique_lock<std::mutex> lock(g_mutex);
    Port* port = getPort(handle);
    if (port == nullptr) {
        return -2144993277;
    }

    reopenStoppedDevice(*port);
    if (port->device == 0) {
        outputVirtualPort(*port, ptr, lock);
    } else {
        queueAudio(*port, ptr, lock);
    }
    if (ptr != nullptr) port->lastDataOutputTime = sceKernelGetProcessTime();
    return static_cast<int>(port->samplesNum);
} catch (const ProcessShutdown&) {
    LibcAwaitExit_nid_postfix();
}

int APS5_VABI sceAudioOutOutputs(AudioOutOutputParam* param, std::uint32_t num) try {
    const AudioOutCall call;
    if (param == nullptr || num == 0) {
        return -2144993276;
    }

    std::array<bool, PORTS_MAX> selected{};
    for (std::uint32_t i = 0; i < num; ++i) {
        if (param[i].handle <= 0 || param[i].handle > PORTS_MAX) return -2144993277;
        selected[param[i].handle - 1] = true;
    }
    std::array<std::unique_lock<std::mutex>, PORTS_MAX> outputLocks;
    for (int index = 0; index < PORTS_MAX; ++index) {
        if (selected[index]) outputLocks[index] = std::unique_lock<std::mutex>(g_outputMutexes[index]);
    }
    std::unique_lock<std::mutex> lock(g_mutex);

    for (std::uint32_t i = 0; i < num; i++) {
        if (getPort(param[i].handle) == nullptr) {
            return -2144993277;
        }
    }

    Port& first = *getPort(param[0].handle);
    for (std::uint32_t i = 0; i < num; i++) {
        Port& port = *getPort(param[i].handle);
        reopenStoppedDevice(port);
        if (port.device == 0) {
            outputVirtualPort(port, param[i].ptr, lock);
        } else {
            queueAudio(port, param[i].ptr, lock);
        }
    }

    const std::uint64_t done = sceKernelGetProcessTime();
    for (std::uint32_t i = 0; i < num; i++) {
        if (param[i].ptr != nullptr) getPort(param[i].handle)->lastDataOutputTime = done;
    }

    return static_cast<int>(first.samplesNum);
} catch (const ProcessShutdown&) {
    LibcAwaitExit_nid_postfix();
}

int APS5_VABI sceAudioOutSetVolume(int handle, std::uint32_t flag, int* vol) {
    if (vol == nullptr) {
        return -2144993276;
    }
    std::lock_guard<std::mutex> lock(g_mutex);
    Port* port = getPort(handle);
    if (port == nullptr) {
        return -2144993277;
    }
    const bool isStd = formatIsStd(port->format);
    for (int i = 0; i < port->channels; i++, flag >>= 1u) {
        if ((flag & 1u) == 0) {
            continue;
        }
        int srcIdx = i;
        if (isStd) {
            if (i == 4) srcIdx = 6;
            else if (i == 5) srcIdx = 7;
            else if (i == 6) srcIdx = 4;
            else if (i == 7) srcIdx = 5;
        }
        port->volume[i] = vol[srcIdx];
    }
    return 0;
}

int APS5_VABI sceAudioOutGetLastOutputTime(int handle, std::uint64_t* outputTime) {
    if (outputTime == nullptr) {
        return -2144993276;
    }
    std::lock_guard<std::mutex> lock(g_mutex);
    Port* port = getPort(handle);
    if (port == nullptr) {
        return -2144993277;
    }
    const std::uint64_t time = port->lastDataOutputTime;
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(outputTime, &time, sizeof(time));
    errno = savedErrno;
    if (!copied) {
        return -2144993276;
    }
    return 0;
}

int APS5_VABI sceAudioOutGetPortState(int handle, AudioOutPortState* state) {
    if (state == nullptr) {
        return -2144993276;
    }
    std::lock_guard<std::mutex> lock(g_mutex);
    Port* port = getPort(handle);
    if (port == nullptr) {
        return -2144993277;
    }
    AudioOutPortState localState{};
    localState.rerouteCounter = 0;
    localState.volume = 127;
    localState.flag = 0;
    localState.activeState = 0;
    localState.reserved[0] = 0;
    switch (port->type) {
        case PORT_TYPE_MAIN:
        case PORT_TYPE_BGM:
        case PORT_TYPE_AUDIO3D:
            localState.output = 1;
            localState.channel = static_cast<std::uint8_t>(port->channels > 2 ? 2 : port->channels);
            break;
        case PORT_TYPE_VOICE:
        case PORT_TYPE_PERSONAL:
            localState.output = 0x40;
            localState.channel = 1;
            break;
        case PORT_TYPE_PADSPK:
        case PORT_TYPE_VIBRATION:
            localState.output = 4;
            localState.channel = 1;
            break;
        case PORT_TYPE_AUX:
            localState.output = 0x80;
            localState.channel = 0;
            break;
        default:
            throw std::runtime_error("sceAudioOutGetPortState: unknown port type");
    }
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(state, &localState, sizeof(localState));
    errno = savedErrno;
    if (!copied) {
        return -2144993276;
    }
    return 0;
}

int APS5_VABI sceAudioOutDeviceIdOpen(std::uint64_t deviceId, std::int32_t type, std::int32_t index, std::uint32_t length, std::int32_t frequency, std::uint32_t parameter) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAudioOutSysClose(int handle) {
    if (handle <= 0 || handle > PORTS_MAX) {
        throw std::runtime_error("sceAudioOutSysClose: handle " + std::to_string(handle) + " is not an open Sys port");
    }
    std::lock_guard<std::mutex> outputLock(g_outputMutexes[handle - 1]);
    std::lock_guard<std::mutex> lock(g_mutex);
    Port* port = getPort(handle);
    if (port == nullptr || port->type != PORT_TYPE_SYS) {
        throw std::runtime_error("sceAudioOutSysClose: handle " + std::to_string(handle) + " is not an open Sys port");
    }
    *port = Port{};
    return 0;
}

int APS5_VABI sceAudioOutSysConfigureOutput(int type, std::uint64_t flags, std::uint32_t mode, std::uint32_t target, std::uint64_t option) {
    if (type != SYS_CONFIGURE_TYPE_HDMI) {
        throw std::runtime_error("sceAudioOutSysConfigureOutput: output type " + std::to_string(type) + " not supported");
    }
    if (flags != 0 || option != 0) {
        throw std::runtime_error("sceAudioOutSysConfigureOutput: flags " + std::to_string(flags) + " and option " + std::to_string(option) + " not supported");
    }
    if (mode != SYS_OPEN_MODE_TRUEHD) {
        throw std::runtime_error("sceAudioOutSysConfigureOutput: mode " + std::to_string(mode) + " not supported");
    }
    if (target != SYS_CONFIGURE_TARGET_HDMI) {
        throw std::runtime_error("sceAudioOutSysConfigureOutput: target " + std::to_string(target) + " not supported");
    }
    return 0;
}

int APS5_VABI sceAudioOutSysGetHdmiMonitorInfo(int type, void* info, std::uint32_t size) {
    if (info == nullptr) return static_cast<int>(0x80260004);
    if (size != HDMI_MONITOR_INFO_SIZE) return static_cast<int>(0x80260006);
    if (type != MONITOR_TYPE_HDMI && type != 0x10) return static_cast<int>(0x8026000C);
    if (type == 0x10) throw std::runtime_error("sceAudioOutSysGetHdmiMonitorInfo: monitor type 16 not supported");
    std::memset(info, 0, HDMI_MONITOR_INFO_SIZE);
    return 0;
}

int APS5_VABI sceAudioOutSysOpen(int userId, std::uint32_t mode) {
    (void)userId;
    if (mode != SYS_OPEN_MODE_TRUEHD) {
        throw std::runtime_error("sceAudioOutSysOpen: mode " + std::to_string(mode) + " not supported");
    }
    return allocatePort(PORT_TYPE_SYS, SYS_PORT_SAMPLES, SYS_PORT_FRAME_RATE, Format::S16_8Ch);
}

int APS5_VABI sceAudioOutSetMainOutput() {
    return 0;
}

int APS5_VABI sceAudioOutSetMixLevelPadSpk(int handle, std::uint32_t mixLevel) {
    std::lock_guard<std::mutex> lock(g_mutex);
    Port* port = getPort(handle);
    if (port == nullptr) {
        return -2144993277;
    }
    if (mixLevel > DEFAULT_VOLUME) {
        return -2144993260;
    }
    if (port->type != PORT_TYPE_PADSPK) {
        return -2144993270;
    }
    port->mixLevel = mixLevel;
    return 0;
}

int APS5_VABI sceAudioOutExClose(int handle) {
    if (handle <= 0 || handle > PORTS_MAX) {
        throw std::runtime_error("sceAudioOutExClose: handle " + std::to_string(handle) + " is not an open Ex port");
    }
    std::lock_guard<std::mutex> outputLock(g_outputMutexes[handle - 1]);
    std::lock_guard<std::mutex> lock(g_mutex);
    Port* port = getPort(handle);
    if (port == nullptr || port->type != PORT_TYPE_EX) {
        throw std::runtime_error("sceAudioOutExClose: handle " + std::to_string(handle) + " is not an open Ex port");
    }
    *port = Port{};
    return 0;
}

int APS5_VABI sceAudioOutExConfigureOutput(int control, std::uint64_t flags, std::uint32_t mode, std::uint32_t target, std::uint64_t option) {
    if (control != 0) {
        return ERROR_EX_INVALID_CONTROL;
    }
    if (flags != 0 || option != 0) {
        throw std::runtime_error("sceAudioOutExConfigureOutput: flags " + std::to_string(flags) + " and option " + std::to_string(option) + " not supported");
    }
    if (mode > EX_OPEN_MODE_MAX && mode != EX_CONFIGURE_MODE_DEFAULT) {
        throw std::runtime_error("sceAudioOutExConfigureOutput: mode " + std::to_string(mode) + " not supported");
    }
    if ((target < 1 || target > 3) && target != EX_CONFIGURE_TARGET_DEFAULT) {
        throw std::runtime_error("sceAudioOutExConfigureOutput: target " + std::to_string(target) + " not supported");
    }
    return 0;
}

int APS5_VABI sceAudioOutExGetMonitorInfo(int type, int index, void* info, std::uint32_t size) {
    if (type != 0) return static_cast<int>(0x80260003);
    if (info == nullptr) return static_cast<int>(0x80260004);
    if (size != EX_MONITOR_INFO_SIZE) return static_cast<int>(0x80260006);
    const std::array<std::uint8_t, EX_MONITOR_INFO_SIZE> empty{};
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(info, empty.data(), empty.size());
    errno = savedErrno;
    if (!copied) return static_cast<int>(0x80260004);
    return 0;
}

int APS5_VABI sceAudioOutExOpen(int userId, std::uint32_t mode) {
    (void)userId;
    if (mode > EX_OPEN_MODE_MAX || ((EX_OPEN_MODES >> mode) & 1) == 0) {
        return ERROR_EX_INVALID_MODE;
    }
    const bool highRate = mode == 3 || mode == 4 || mode == 10;
    return allocatePort(PORT_TYPE_EX, highRate ? 1024 : 256, highRate ? 192000 : 48000, Format::S16Stereo);
}

int APS5_VABI sceAudioOutGetSystemState(AudioOutSystemState* state) {
    if (state == nullptr) {
        return -2144993276;
    }
    AudioOutSystemState localState{};
    localState.loudness = 0.0f;
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(state, &localState, sizeof(localState));
    errno = savedErrno;
    if (!copied) {
        return -2144993276;
    }
    return 0;
}

}
