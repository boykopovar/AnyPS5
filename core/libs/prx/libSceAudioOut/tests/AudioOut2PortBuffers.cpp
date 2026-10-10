#include "SceTypes.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <random>
#include <string>
#include <vector>

extern "C" {
int APS5_VABI sceAudioOut2ContextResetParam(AudioOut2ContextParam*);
int APS5_VABI sceAudioOut2ContextCreate(const AudioOut2ContextParam*, void*, std::size_t, AudioOut2ContextHandle*);
int APS5_VABI sceAudioOut2ContextDestroy(AudioOut2ContextHandle);
int APS5_VABI sceAudioOut2ContextPush(AudioOut2ContextHandle, std::uint32_t);
int APS5_VABI sceAudioOut2PortCreate(AudioOut2ContextHandle, const AudioOut2PortParam*, AudioOut2PortHandle*);
int APS5_VABI sceAudioOut2PortDestroy(AudioOut2PortHandle);
int APS5_VABI sceAudioOut2PortSetAttributes(AudioOut2PortHandle, const AudioOut2Attribute*, std::uint32_t);
}

static void Require(bool value) { if (!value) std::abort(); }

static void SetEnvironment(const char* name, const std::string& value) {
#ifdef _WIN32
    _putenv_s(name, value.c_str());
#else
    setenv(name, value.c_str(), 1);
#endif
}

namespace {

constexpr std::uint32_t grain = 256;
constexpr std::uint32_t channels = 2;
constexpr std::uint32_t frequency = 48000;
constexpr std::uint16_t portTypeMain = 0;
constexpr std::uint32_t attributeData = 0;
constexpr std::uint32_t formatStereoFloat = 2u << 8;
constexpr std::uint32_t silentGrains = 32;
constexpr float masterGain = 0.5f;
constexpr float tolerance = 1e-6f;

void SetData(AudioOut2PortHandle port, const void* data) {
    const AudioOut2Attribute attribute{attributeData, 0, &data, sizeof(data)};
    Require(sceAudioOut2PortSetAttributes(port, &attribute, 1) == 0);
}

std::vector<float> Play(const std::function<void(AudioOut2ContextHandle, AudioOut2PortHandle)>& feed) {
    const auto path = std::filesystem::temp_directory_path() / ("anyps5_audio_out2_port_buffers-" + std::to_string(std::random_device{}()) + ".raw");
    std::filesystem::remove(path);
    SetEnvironment("SDL_DISKAUDIOFILE", path.string());

    AudioOut2ContextParam params{};
    Require(sceAudioOut2ContextResetParam(&params) == 0);
    params.num_grains = grain;
    params.queue_depth = 1;
    AudioOut2ContextHandle context = 0;
    Require(sceAudioOut2ContextCreate(&params, nullptr, 0, &context) == 0);
    AudioOut2PortParam portParams{};
    portParams.port_type = portTypeMain;
    portParams.data_format = formatStereoFloat;
    portParams.sampling_freq = frequency;
    AudioOut2PortHandle port = 0;
    Require(sceAudioOut2PortCreate(context, &portParams, &port) == 0);
    feed(context, port);
    for (std::uint32_t push = 0; push < silentGrains; push++) Require(sceAudioOut2ContextPush(context, 1) == 0);
    Require(sceAudioOut2PortDestroy(port) == 0);
    Require(sceAudioOut2ContextDestroy(context) == 0);

    std::ifstream file(path, std::ios::binary);
    const std::vector<char> bytes{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    file.close();
    std::filesystem::remove(path);
    Require(bytes.size() % sizeof(float) == 0);
    const auto* samples = reinterpret_cast<const float*>(bytes.data());
    std::size_t first = 0;
    std::size_t last = bytes.size() / sizeof(float);
    while (first < last && samples[first] == 0.0f) first++;
    while (last > first && samples[last - 1] == 0.0f) last--;
    return {samples + first, samples + last};
}

std::vector<float> Grain(float level) {
    std::vector<float> data(static_cast<std::size_t>(grain) * channels);
    for (std::uint32_t frame = 0; frame < grain; frame++) {
        const float sign = frame % 2 == 0 ? 1.0f : -1.0f;
        data[frame * channels] = sign * level * static_cast<float>(1 + frame % 5);
        data[frame * channels + 1] = -sign * level * static_cast<float>(1 + frame % 3);
    }
    return data;
}

void RequirePlayed(const std::vector<float>& played, const std::vector<const std::vector<float>*>& grains) {
    Require(played.size() == grains.size() * grain * channels);
    for (std::size_t index = 0; index < grains.size(); index++) {
        const auto& data = *grains[index];
        for (std::size_t sample = 0; sample < data.size(); sample++) {
            Require(std::fabs(played[index * data.size() + sample] - data[sample] * masterGain) <= tolerance);
        }
    }
}

void TestBuffersSetBeforeOnePushPlayInOrder() {
    const auto first = Grain(0.01f);
    const auto second = Grain(0.02f);
    const auto third = Grain(0.03f);
    const auto played = Play([&](AudioOut2ContextHandle context, AudioOut2PortHandle port) {
        SetData(port, first.data());
        SetData(port, second.data());
        SetData(port, third.data());
        Require(sceAudioOut2ContextPush(context, 1) == 0);
        Require(sceAudioOut2ContextPush(context, 1) == 0);
        Require(sceAudioOut2ContextPush(context, 1) == 0);
    });
    RequirePlayed(played, {&first, &second, &third});
}

void TestBurstThenGapKeepsEveryBuffer() {
    const auto first = Grain(0.01f);
    const auto second = Grain(0.02f);
    const auto third = Grain(0.03f);
    const auto played = Play([&](AudioOut2ContextHandle context, AudioOut2PortHandle port) {
        SetData(port, first.data());
        Require(sceAudioOut2ContextPush(context, 1) == 0);
        SetData(port, second.data());
        SetData(port, third.data());
        Require(sceAudioOut2ContextPush(context, 1) == 0);
        Require(sceAudioOut2ContextPush(context, 1) == 0);
    });
    RequirePlayed(played, {&first, &second, &third});
}

void TestAdvancingPortWithoutNewBufferPlaysSilence() {
    const auto first = Grain(0.01f);
    const auto second = Grain(0.02f);
    const auto played = Play([&](AudioOut2ContextHandle context, AudioOut2PortHandle port) {
        SetData(port, first.data());
        Require(sceAudioOut2ContextPush(context, 1) == 0);
        SetData(port, second.data());
        Require(sceAudioOut2ContextPush(context, 1) == 0);
    });
    RequirePlayed(played, {&first, &second});
}

void TestRewrittenBufferRepeats() {
    auto buffer = Grain(0.01f);
    const auto first = buffer;
    const auto second = Grain(0.02f);
    const auto played = Play([&](AudioOut2ContextHandle context, AudioOut2PortHandle port) {
        SetData(port, buffer.data());
        Require(sceAudioOut2ContextPush(context, 1) == 0);
        Require(sceAudioOut2ContextPush(context, 1) == 0);
        buffer = second;
        SetData(port, nullptr);
    });
    RequirePlayed(played, {&first, &second});
}

}

int main() {
    SetEnvironment("SDL_AUDIODRIVER", "disk");
    TestBuffersSetBeforeOnePushPlayInOrder();
    TestBurstThenGapKeepsEveryBuffer();
    TestAdvancingPortWithoutNewBufferPlaysSilence();
    TestRewrittenBufferRepeats();
    return 0;
}
