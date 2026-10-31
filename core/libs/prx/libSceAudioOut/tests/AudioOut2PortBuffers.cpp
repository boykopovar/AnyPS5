#include "prx/libSceAudioOut/include/AudioOut2.hpp"
#include "prx/libSceAudioOut/src/AudioOut2PadMix.hpp"
#include "SceTypes.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <future>
#include <iterator>
#include <random>
#include <string>
#include <thread>
#include <vector>

static void Require(bool value) { if (!value) std::abort(); }

static void SetEnvironment(const char* name, const std::string& value) {
#ifdef _WIN32
    _putenv_s(name, value.c_str());
#else
    setenv(name, value.c_str(), 1);
#endif
}

static void UnsetEnvironment(const char* name) {
#ifdef _WIN32
    _putenv_s(name, "");
#else
    unsetenv(name);
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

std::vector<float> Play(const std::function<void(AudioOut2ContextHandle, AudioOut2PortHandle)>& feed, bool advanced = false, std::uint32_t queueDepth = 1) {
    const auto path = std::filesystem::temp_directory_path() / ("anyps5_audio_out2_port_buffers-" + std::to_string(std::random_device{}()) + ".raw");
    std::filesystem::remove(path);
    SetEnvironment("SDL_DISKAUDIOFILE", path.string());

    AudioOut2ContextParam params{};
    Require(sceAudioOut2ContextResetParam(&params) == 0);
    params.num_grains = grain;
    params.queue_depth = queueDepth;
    AudioOut2ContextHandle context = 0;
    Require(sceAudioOut2ContextCreate(&params, nullptr, 0, &context) == 0);
    AudioOut2PortParam portParams{};
    portParams.port_type = portTypeMain;
    portParams.data_format = formatStereoFloat;
    portParams.sampling_freq = frequency;
    AudioOut2PortHandle port = 0;
    Require(sceAudioOut2PortCreate(context, &portParams, &port) == 0);
    feed(context, port);
    for (std::uint32_t push = 0; push < silentGrains; push++) {
        if (advanced) {
            SetData(port, nullptr);
            Require(sceAudioOut2ContextAdvance(context) == 0);
        }
        Require(sceAudioOut2ContextPush(context, 1) == 0);
    }
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

void TestAdvanceRetainsRewrittenBuffer() {
    auto buffer = Grain(0.01f);
    const auto first = buffer;
    const auto second = Grain(0.02f);
    const auto third = Grain(0.03f);
    const auto played = Play([&](AudioOut2ContextHandle context, AudioOut2PortHandle port) {
        SetData(port, buffer.data());
        Require(sceAudioOut2ContextAdvance(context) == 0);
        buffer = second;
        SetData(port, buffer.data());
        Require(sceAudioOut2ContextAdvance(context) == 0);
        buffer = third;
        Require(sceAudioOut2ContextPush(context, 1) == 0);
        Require(sceAudioOut2ContextPush(context, 1) == 0);
    }, true);
    RequirePlayed(played, {&first, &second});
}

void TestAdvanceKeepsTheGrainSetBeforeTheBufferIsCleared() {
    auto buffer = Grain(0.01f);
    const auto first = buffer;
    const auto second = Grain(0.02f);
    const auto played = Play([&](AudioOut2ContextHandle context, AudioOut2PortHandle port) {
        SetData(port, buffer.data());
        std::fill(buffer.begin(), buffer.end(), 0.0f);
        Require(sceAudioOut2ContextAdvance(context) == 0);
        Require(sceAudioOut2ContextPush(context, 1) == 0);
        std::copy(second.begin(), second.end(), buffer.begin());
        SetData(port, buffer.data());
        std::fill(buffer.begin(), buffer.end(), 0.0f);
        Require(sceAudioOut2ContextAdvance(context) == 0);
        Require(sceAudioOut2ContextPush(context, 1) == 0);
    }, true);
    RequirePlayed(played, {&first, &second});
}

void TestAdvanceRetainsVolume() {
    const auto buffer = Grain(0.02f);
    auto reduced = buffer;
    for (auto& sample : reduced) sample *= 0.5f;
    const auto played = Play([&](AudioOut2ContextHandle context, AudioOut2PortHandle port) {
        SetData(port, buffer.data());
        Require(sceAudioOut2ContextAdvance(context) == 0);
        const std::array<float, 2> volume{0.5f, 0.5f};
        const AudioOut2Attribute attribute{1, 0, volume.data(), sizeof(volume)};
        Require(sceAudioOut2PortSetAttributes(port, &attribute, 1) == 0);
        SetData(port, buffer.data());
        Require(sceAudioOut2ContextAdvance(context) == 0);
        Require(sceAudioOut2ContextPush(context, 1) == 0);
        Require(sceAudioOut2ContextPush(context, 1) == 0);
    }, true);
    RequirePlayed(played, {&buffer, &reduced});
}

void TestAdvanceBurstRetainsEveryGrain() {
    std::vector<std::vector<float>> expected;
    for (std::uint32_t index = 0; index < 64; ++index) expected.push_back(Grain(0.001f * static_cast<float>(index + 1)));
    const auto played = Play([&](AudioOut2ContextHandle context, AudioOut2PortHandle port) {
        std::thread producer([&] {
            auto buffer = Grain(0.0f);
            for (const auto& data : expected) {
                buffer = data;
                SetData(port, buffer.data());
                Require(sceAudioOut2ContextAdvance(context) == 0);
            }
        });
        producer.join();
        for (std::size_t index = 0; index < expected.size(); ++index) Require(sceAudioOut2ContextPush(context, 1) == 0);
    }, true);
    std::vector<const std::vector<float>*> grains;
    for (const auto& data : expected) grains.push_back(&data);
    RequirePlayed(played, grains);
}

void TestPushingThreadWaitsForAdvance() {
    const auto first = Grain(0.01f);
    const auto second = Grain(0.02f);
    const auto played = Play([&](AudioOut2ContextHandle context, AudioOut2PortHandle port) {
        SetData(port, first.data());
        Require(sceAudioOut2ContextAdvance(context) == 0);
        Require(sceAudioOut2ContextPush(context, 1) == 0);
        std::promise<void> entered;
        std::promise<int> done;
        auto completed = done.get_future();
        std::thread pusher([&] {
            entered.set_value();
            done.set_value(sceAudioOut2ContextPush(context, 1));
        });
        entered.get_future().wait();
        const bool waited = completed.wait_for(std::chrono::milliseconds(10)) == std::future_status::timeout;
        SetData(port, second.data());
        Require(sceAudioOut2ContextAdvance(context) == 0);
        Require(completed.get() == 0);
        pusher.join();
        Require(waited);
    }, true);
    RequirePlayed(played, {&first, &second});
}

void TestRenderingSizedByLevelStaysWithinQueueDepth() {
    constexpr std::uint32_t queueDepth = 4;
    constexpr std::uint32_t grains = 48;
    std::vector<std::vector<float>> expected;
    for (std::uint32_t index = 0; index < grains; index++) expected.push_back(Grain(0.001f * static_cast<float>(index + 1)));
    std::uint32_t mostAwaiting = 0;
    const auto played = Play([&](AudioOut2ContextHandle context, AudioOut2PortHandle port) {
        std::uint32_t advanced = 0;
        std::uint32_t pushed = 0;
        while (pushed < grains) {
            std::uint32_t available = 0;
            Require(sceAudioOut2ContextGetQueueLevel(context, nullptr, &available) == 0);
            for (std::uint32_t index = 0; index < available && advanced < grains; index++) {
                SetData(port, expected[advanced].data());
                Require(sceAudioOut2ContextAdvance(context) == 0);
                advanced++;
            }
            mostAwaiting = std::max(mostAwaiting, advanced - pushed);
            if (advanced == pushed) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }
            Require(sceAudioOut2ContextPush(context, 1) == 0);
            pushed++;
        }
    }, true, queueDepth);
    Require(mostAwaiting <= queueDepth);
    std::vector<const std::vector<float>*> order;
    for (const auto& data : expected) order.push_back(&data);
    RequirePlayed(played, order);
}

void TestBufferReusedAcrossPortsKeepsEachPortsGrain(bool advanced) {
    const auto voice = Grain(0.01f);
    const std::vector<float> silence(voice.size(), 0.0f);
    auto shared = voice;
    const auto played = Play([&](AudioOut2ContextHandle context, AudioOut2PortHandle port) {
        AudioOut2PortParam portParams{};
        portParams.port_type = portTypeMain;
        portParams.data_format = formatStereoFloat;
        portParams.sampling_freq = frequency;
        AudioOut2PortHandle other = 0;
        Require(sceAudioOut2PortCreate(context, &portParams, &other) == 0);
        SetData(port, shared.data());
        shared = silence;
        SetData(other, shared.data());
        if (advanced) Require(sceAudioOut2ContextAdvance(context) == 0);
        Require(sceAudioOut2ContextPush(context, 1) == 0);
        SetData(port, nullptr);
        SetData(other, nullptr);
    }, advanced);
    RequirePlayed(played, {&voice});
}

}

int main() {
    UnsetEnvironment(AUDIO_PAD_SPEAKER_FALLBACK_ENV);
    SetEnvironment("SDL_AUDIODRIVER", "disk");
    TestAdvanceRetainsRewrittenBuffer();
    TestAdvanceRetainsVolume();
    TestAdvanceKeepsTheGrainSetBeforeTheBufferIsCleared();
    TestAdvanceBurstRetainsEveryGrain();
    TestPushingThreadWaitsForAdvance();
    TestBuffersSetBeforeOnePushPlayInOrder();
    TestBurstThenGapKeepsEveryBuffer();
    TestAdvancingPortWithoutNewBufferPlaysSilence();
    TestRewrittenBufferRepeats();
    TestRenderingSizedByLevelStaysWithinQueueDepth();
    TestBufferReusedAcrossPortsKeepsEachPortsGrain(false);
    TestBufferReusedAcrossPortsKeepsEachPortsGrain(true);
    return 0;
}
