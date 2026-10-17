#include "SDL.h"
#include "prx/libSceAudioOut/include/AudioOutSdl.hpp"
#include "prx/libScePad/include/PadState.hpp"
#include "prx/libScePad/include/PadAudioHapticQueue.hpp"
#include "prx/libkernel/Time/include/Time.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <initializer_list>
#include <stdexcept>
#include <string_view>
#include <vector>

extern "C" {
int APS5_VABI sceAudioOutOpen(int, int, int, std::uint32_t, std::uint32_t, std::uint32_t);
int APS5_VABI sceAudioOutClose(int);
int APS5_VABI sceAudioOutOutput(int, const void*);
int APS5_VABI sceAudioOutOutputs(AudioOutOutputParam*, std::uint32_t);
int APS5_VABI sceAudioOutSetVolume(int, std::uint32_t, int*);
int APS5_VABI sceAudioOutGetLastOutputTime(int, std::uint64_t*);
}

namespace {
constexpr std::uint32_t frames = 256;
bool available = true;
bool failOpen = false;
bool failInitialize = false;
bool failEnumeration = false;
bool pulse = false;
std::uint32_t frequency = 48000;
int opened = 0;
int closed = 0;
int queued = 0;
std::vector<float> played;
struct Block {
    int source;
    std::uint64_t begin;
    std::uint64_t end;
    std::array<std::uint8_t, 2> rumble;
};
std::vector<Block> blocks;
std::vector<int> clears;
PadAudioHapticQueue timeline;

void Require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}

float Expected(std::int16_t sample, int volume, bool floating) {
    const float value = sample / 32768.0f;
    if (floating) return std::clamp(value * (volume / 32768.0f), -1.0f, 1.0f);
    return static_cast<float>(std::clamp<std::int64_t>(static_cast<std::int64_t>(sample) * volume / 32768, -32768, 32767)) / 32768.0f;
}

void Run(std::uint32_t format, bool batch, int volume) {
    const bool floating = format >= 3;
    const std::uint32_t channels = format == 0 || format == 3 ? 1u : 2u;
    const int handle = sceAudioOutOpen(0x10000000, 10, 0, frames, frequency, format);
    Require(handle == 1, "a closed or failed vibration port remained allocated");
    std::array<int, 2> levels{volume, volume / 2};
    Require(sceAudioOutSetVolume(handle, 3, levels.data()) == 0, "vibration gains failed");
    std::array<std::int16_t, frames * 2> integers{};
    std::array<float, frames * 2> floats{};
    for (std::uint32_t frame = 0; frame < frames; ++frame) {
        integers[frame * channels] = 8192;
        floats[frame * channels] = 0.25f;
        if (channels == 2) {
            integers[frame * channels + 1] = -16384;
            floats[frame * channels + 1] = -0.5f;
        }
    }
    const void* data = floating ? static_cast<const void*>(floats.data()) : static_cast<const void*>(integers.data());
    AudioOutOutputParam outputs[2]{};
    outputs[0].handle = handle;
    outputs[0].ptr = data;
    outputs[1].handle = 9999;
    outputs[1].ptr = data;
    const int before = queued;
    Require(sceAudioOutOutputs(outputs, 2) < 0 && queued == before, "an invalid batch queued a valid prefix");
    Require((batch ? sceAudioOutOutputs(outputs, 1) : sceAudioOutOutput(handle, data)) == static_cast<int>(frames), "vibration frame count changed");
    Require(queued == before + 1, "native vibration did not reach the named controller device");
    const std::uint32_t deviceChannels = pulse ? 6u : 4u;
    Require(played.size() == frames * deviceChannels, "native vibration frame layout changed");
    const auto left = Expected(8192, levels[0], floating);
    const auto right = channels == 1 ? left : Expected(-16384, levels[1], floating);
    for (std::uint32_t frame = 0; frame < frames; ++frame) {
        for (std::uint32_t channel = 0; channel < deviceChannels; ++channel) {
            const auto expected = channel == deviceChannels - 2 ? left : channel == deviceChannels - 1 ? right : 0.0f;
            Require(played[frame * deviceChannels + channel] == expected, "vibration PCM or gain leaked into a speaker channel");
        }
    }
    std::uint64_t time = 0;
    Require(sceAudioOutGetLastOutputTime(handle, &time) == 0 && time != 0, "vibration acceptance time was not recorded");
    Require(sceAudioOutOutput(handle, nullptr) == static_cast<int>(frames) && queued == before + 1, "draining vibration queued samples");
    std::uint64_t after = 0;
    Require(sceAudioOutGetLastOutputTime(handle, &after) == 0 && after == time, "a drain changed the last data time");
    Require(sceAudioOutClose(handle) == 0, "native vibration close failed");
}

int Submit(std::uint32_t count, std::uint32_t rate, bool split = false) {
    const int handle = sceAudioOutOpen(0x10000000, 10, 0, count, rate, 1);
    Require(handle > 0, "timed vibration port did not open");
    std::vector<std::int16_t> samples(static_cast<std::size_t>(count) * 2);
    for (std::uint32_t frame = 0; frame < count; ++frame) {
        samples[frame * 2] = split && frame >= 256 ? 16384 : 8192;
        samples[frame * 2 + 1] = split && frame >= 256 ? -8192 : -16384;
    }
    Require(sceAudioOutOutput(handle, samples.data()) == static_cast<int>(count), "timed vibration output failed");
    return handle;
}

void RunTiming() {
    available = false;
    const int shortPort = Submit(256, 48000);
    Require(blocks.size() == 1 && blocks[0].end - blocks[0].begin == 5333 && blocks[0].rumble == std::array<std::uint8_t, 2>{128, 255}, "short block timing changed");
    const int longPort = Submit(4800, 24000);
    Require(blocks.size() == 20 && blocks.back().end - blocks[1].begin == 200000, "long block duration did not follow frame count/rate");
    for (std::size_t index = 2; index < blocks.size(); ++index) Require(blocks[index].begin == blocks[index - 1].end, "successive grains overlap or leave a gap");
    auto model = timeline;
    Require(model.Read(blocks[1].begin + 81000) == std::array<std::uint8_t, 2>{128, 255}, "a long block inherited the AudioOut2 timeout");
    Require(sceAudioOutOutput(shortPort, nullptr) == 256 && clears == std::vector<int>{shortPort}, "a drain cleared another port");
    Require(sceKernelGetProcessTime() >= blocks[0].end, "a drain returned before the submitted block ended");
    Require(sceAudioOutClose(shortPort) == 0 && sceAudioOutClose(longPort) == 0, "timed port close failed");
    Require(clears == std::vector<int>{shortPort, shortPort, longPort}, "close did not clear its own source");
    Require(timeline.Read(sceKernelGetProcessTime()) == std::array<std::uint8_t, 2>{}, "close retained a queued source");

    const auto firstLong = blocks.size();
    const int drained = Submit(4800, 24000);
    const auto end = blocks.back().end;
    Require(blocks.size() == firstLong + 19, "long PCM was not divided into grains");
    Require(sceAudioOutOutput(drained, nullptr) == 4800 && sceKernelGetProcessTime() >= end, "long drain did not wait for its frame duration");
    Require(sceAudioOutClose(drained) == 0, "drained port close failed");

    const auto firstSplit = blocks.size();
    const int split = Submit(512, 48000, true);
    Require(blocks.size() == firstSplit + 2, "two-grain PCM was reduced to one value");
    Require(blocks[firstSplit].rumble == std::array<std::uint8_t, 2>{128, 255} && blocks.back().rumble == std::array<std::uint8_t, 2>{255, 128}, "grain-local energy was replaced by whole-block RMS");
    Require(sceAudioOutClose(split) == 0, "split port close failed");
    Require(opened == 0 && queued == 0, "virtual vibration opened a speaker device");
}
}

int InitializeSdlAudio_nid_no_patch() { return failInitialize ? -1 : 0; }
extern "C" void PadQueueLegacyAudioHaptics_nid_postfix(int source, std::uint64_t begin, std::uint64_t end, std::uint8_t large, std::uint8_t small) {
    blocks.push_back({source, begin, end, {large, small}});
    timeline.Queue(source, begin, end, large, small);
}
extern "C" void PadClearLegacyAudioHaptics_nid_postfix(int source) {
    clears.push_back(source);
    timeline.Clear(source);
}
extern "C" Uint32 SDLCALL SDL_WasInit(Uint32) { return SDL_INIT_AUDIO; }
extern "C" int SDLCALL SDL_GetNumAudioDevices(int) { return failEnumeration ? -1 : available ? 2 : 1; }
extern "C" const char* SDLCALL SDL_GetAudioDeviceName(int index, int) { return index == 0 ? "Main speakers" : "DualSense Wireless Controller"; }
extern "C" int SDLCALL SDL_GetAudioDeviceSpec(int index, int, SDL_AudioSpec* spec) { *spec = {}; spec->channels = index == 0 ? 2 : 4; return 0; }
extern "C" const char* SDLCALL SDL_GetCurrentAudioDriver() { return pulse ? "pulseaudio" : "dummy"; }
extern "C" const char* SDLCALL SDL_GetError() { return "test controller open failure"; }
extern "C" SDL_AudioDeviceID SDLCALL SDL_OpenAudioDevice(const char* name, int capture, const SDL_AudioSpec* desired, SDL_AudioSpec* obtained, int flags) {
    Require(name != nullptr && std::string_view(name) == "DualSense Wireless Controller", "vibration opened the default speakers");
    Require(capture == 0 && flags == 0 && desired->callback == nullptr && desired->freq == static_cast<int>(frequency) && desired->format == AUDIO_F32SYS && desired->channels == (pulse ? 6 : 4) && desired->samples == frames, "native controller request changed");
    if (failOpen) return 0;
    ++opened;
    *obtained = *desired;
    return 42;
}
extern "C" void SDLCALL SDL_PauseAudioDevice(SDL_AudioDeviceID device, int) { Require(device == 42, "wrong native device paused"); }
extern "C" Uint32 SDLCALL SDL_GetQueuedAudioSize(SDL_AudioDeviceID device) { Require(device == 42, "wrong native queue queried"); return 0; }
extern "C" int SDLCALL SDL_QueueAudio(SDL_AudioDeviceID device, const void* data, Uint32 bytes) {
    Require(device == 42 && bytes % sizeof(float) == 0, "wrong vibration queue");
    const auto* samples = static_cast<const float*>(data);
    const std::size_t sampleCount = bytes / sizeof(float);
    played.assign(samples, samples + sampleCount);
    ++queued;
    return 0;
}
extern "C" void SDLCALL SDL_ClearQueuedAudio(SDL_AudioDeviceID device) { Require(device == 42, "wrong native queue cleared"); }
extern "C" void SDLCALL SDL_CloseAudioDevice(SDL_AudioDeviceID device) { Require(device == 42, "wrong native device closed"); ++closed; }

int main(int argc, char** argv) {
    try {
        Require(argc == 2, "invalid test arguments");
        const std::string_view mode = argv[1];
        Require(mode == "quad" || mode == "pulse" || mode == "open_failure" || mode == "timing" || mode == "discovery_failure", "invalid device mode");
        pulse = mode == "pulse";
        Pad::Initialize();
        Pad::SetVibrationMode(1);
        std::uint32_t seen = 0;
        PadOutputState state{};
        static_cast<void>(PadFetchOutput_nid_postfix(&seen, &state));
        if (mode == "timing") {
            RunTiming();
            std::puts("legacy vibration device tests passed");
            return 0;
        }
        if (mode == "open_failure") {
            failOpen = true;
            bool threw = false;
            try { static_cast<void>(sceAudioOutOpen(0x10000000, 10, 0, frames, frequency, 1)); }
            catch (const std::runtime_error&) { threw = true; }
            Require(threw && opened == 0 && queued == 0, "a failed native open silently changed routes");
            failOpen = false;
        }
        if (mode == "discovery_failure") {
            for (bool* failure : {&failInitialize, &failEnumeration}) {
                *failure = true;
                bool threw = false;
                try { static_cast<void>(sceAudioOutOpen(0x10000000, 10, 0, frames, frequency, 1)); }
                catch (const std::runtime_error&) { threw = true; }
                Require(threw && opened == 0 && queued == 0, "a discovery failure silently changed routes");
                *failure = false;
            }
        }
        for (auto rate : {24000u, 48000u}) {
            frequency = rate;
            for (auto format : {0u, 1u, 3u, 4u})
                for (bool batch : {false, true})
                    for (int volume : {0, 16384, 32768, 131072}) Run(format, batch, volume);
        }
        for (auto format : {2u, 5u, 6u, 7u}) {
            bool threw = false;
            try { static_cast<void>(sceAudioOutOpen(0x10000000, 10, 0, frames, frequency, format)); }
            catch (const std::runtime_error&) { threw = true; }
            Require(threw && opened == 64, "an unknown multichannel vibration layout was accepted");
        }
        Require(opened == closed && opened == 64, "native vibration device ownership changed");
        Require(!PadFetchOutput_nid_postfix(&seen, &state), "native waveform output also sent motor approximation");
        std::puts("legacy vibration device tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
