#include "SceTypes.hpp"

#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <thread>
#include <vector>

extern "C" {
int APS5_VABI sceAudioOut2ContextResetParam(AudioOut2ContextParam*);
int APS5_VABI sceAudioOut2ContextCreate(const AudioOut2ContextParam*, void*, std::size_t, AudioOut2ContextHandle*);
int APS5_VABI sceAudioOut2ContextDestroy(AudioOut2ContextHandle);
int APS5_VABI sceAudioOut2ContextPush(AudioOut2ContextHandle, std::uint32_t);
int APS5_VABI sceAudioOut2ContextGetQueueLevel(AudioOut2ContextHandle, std::uint32_t*, std::uint32_t*);
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
constexpr std::uint32_t queueDepth = 2;
constexpr std::uint32_t cushionGrains = 8;
constexpr std::uint32_t frequency = 48000;
constexpr std::uint32_t formatMonoFloat = 1u << 8;
constexpr std::uint32_t formatStereoFloat = 2u << 8;
constexpr std::uint32_t attributeData = 0;
constexpr int queueFull = static_cast<int>(0x80260507);
constexpr float masterGain = 0.5f;
constexpr float tolerance = 1e-5f;

struct Session {
    std::filesystem::path path;
    AudioOut2ContextHandle context = 0;
    AudioOut2PortHandle port = 0;
    std::vector<float> buffer = std::vector<float>(static_cast<std::size_t>(grain) * 2, 0.0f);

    explicit Session(const char* name) : path(std::filesystem::temp_directory_path() / name) {
        std::filesystem::remove(path);
        SetEnvironment("SDL_DISKAUDIOFILE", path.string());
        AudioOut2ContextParam params{};
        Require(sceAudioOut2ContextResetParam(&params) == 0);
        params.num_grains = grain;
        params.queue_depth = queueDepth;
        Require(sceAudioOut2ContextCreate(&params, nullptr, 0, &context) == 0);
        AudioOut2PortParam port{};
        port.port_type = 0;
        port.data_format = formatStereoFloat;
        port.sampling_freq = frequency;
        Require(sceAudioOut2PortCreate(context, &port, &this->port) == 0);
    }

    void Point(const void* data) {
        const AudioOut2Attribute attribute{attributeData, 0, &data, sizeof(data)};
        Require(sceAudioOut2PortSetAttributes(port, &attribute, 1) == 0);
    }

    void Fill(float value) {
        for (float& sample : buffer) sample = value;
    }

    std::vector<float> Close() {
        Require(sceAudioOut2PortDestroy(port) == 0);
        Require(sceAudioOut2ContextDestroy(context) == 0);
        std::ifstream file(path, std::ios::binary);
        const std::vector<char> bytes{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
        file.close();
        std::filesystem::remove(path);
        Require(bytes.size() % sizeof(float) == 0);
        const auto* samples = reinterpret_cast<const float*>(bytes.data());
        return {samples, samples + bytes.size() / sizeof(float)};
    }
};

std::size_t FirstNonZero(const std::vector<float>& played, std::size_t from = 0) {
    while (from < played.size() && played[from] == 0.0f) from++;
    return from;
}

bool Constant(const std::vector<float>& played, std::size_t first, std::size_t count, float value) {
    if (first + count > played.size()) return false;
    for (std::size_t index = first; index < first + count; index++) {
        if (std::fabs(played[index] - value) > tolerance) return false;
    }
    return true;
}

void TestReadAtNextPush() {
    Session session("anyps5_audio_out2_next_push.raw");
    session.Fill(0.25f);
    session.Point(session.buffer.data());
    Require(sceAudioOut2ContextPush(session.context, 1) == 0);
    session.Fill(0.5f);
    Require(sceAudioOut2ContextPush(session.context, 1) == 0);
    session.Fill(0.75f);
    Require(sceAudioOut2ContextPush(session.context, 1) == 0);
    session.Fill(0.0f);
    session.Point(nullptr);
    for (std::uint32_t push = 0; push < cushionGrains * 4; push++) Require(sceAudioOut2ContextPush(session.context, 1) == 0);
    const auto played = session.Close();
    const auto first = FirstNonZero(played);
    const std::size_t samples = static_cast<std::size_t>(grain) * 2;
    Require(Constant(played, first, samples, 0.5f * masterGain));
    Require(Constant(played, first + samples, samples, 0.75f * masterGain));
    Require(FirstNonZero(played, first + 2 * samples) == played.size());
}

void TestRecreatedPort() {
    Session session("anyps5_audio_out2_recreated_port.raw");
    session.Fill(0.5f);
    session.Point(session.buffer.data());
    Require(sceAudioOut2ContextPush(session.context, 1) == 0);
    const auto first = session.port;
    Require(sceAudioOut2PortDestroy(session.port) == 0);
    AudioOut2PortParam mono{};
    mono.port_type = 0;
    mono.data_format = formatMonoFloat;
    mono.sampling_freq = frequency;
    Require(sceAudioOut2PortCreate(session.context, &mono, &session.port) == 0);
    Require(session.port == first);
    const std::vector<float> monoBuffer(grain, 0.75f);
    session.Point(monoBuffer.data());
    Require(sceAudioOut2ContextPush(session.context, 1) == 0);
    session.Point(nullptr);
    for (std::uint32_t push = 0; push < cushionGrains * 4; push++) Require(sceAudioOut2ContextPush(session.context, 1) == 0);
    const auto played = session.Close();
    const auto start = FirstNonZero(played);
    const std::size_t samples = static_cast<std::size_t>(grain) * 2;
    Require(Constant(played, start, samples, 0.75f * masterGain));
    Require(FirstNonZero(played, start + samples) == played.size());
}

void TestLevelExcludesCushion() {
    Session session("anyps5_audio_out2_level.raw");
    session.Point(nullptr);
    std::uint32_t level = 99;
    std::uint32_t available = 99;
    Require(sceAudioOut2ContextGetQueueLevel(session.context, &level, &available) == 0);
    Require(level == 0 && available == queueDepth);
    std::uint32_t accepted = 0;
    int result = 0;
    while ((result = sceAudioOut2ContextPush(session.context, 0)) == 0) {
        accepted++;
        Require(accepted < 1000);
    }
    Require(result == queueFull);
    Require(accepted >= cushionGrains + queueDepth);
    session.Close();
}

void TestPrimingAfterRunningDry() {
    Session session("anyps5_audio_out2_priming.raw");
    session.Fill(0.5f);
    session.Point(session.buffer.data());
    for (std::uint32_t push = 0; push < 12; push++) Require(sceAudioOut2ContextPush(session.context, 1) == 0);
    session.Point(nullptr);
    Require(sceAudioOut2ContextPush(session.context, 1) == 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    session.Fill(-0.5f);
    session.Point(session.buffer.data());
    for (std::uint32_t push = 0; push < 12; push++) Require(sceAudioOut2ContextPush(session.context, 1) == 0);
    session.Point(nullptr);
    for (std::uint32_t push = 0; push < cushionGrains * 4; push++) Require(sceAudioOut2ContextPush(session.context, 1) == 0);
    const auto played = session.Close();
    const std::size_t burst = static_cast<std::size_t>(grain) * 2 * 12;
    const auto first = FirstNonZero(played);
    Require(Constant(played, first, burst, 0.5f * masterGain));
    const auto second = FirstNonZero(played, first + burst);
    Require(second < played.size() && second - (first + burst) >= static_cast<std::size_t>(grain) * 2);
    Require(Constant(played, second, burst, -0.5f * masterGain));
}

}

int main() {
    SetEnvironment("SDL_AUDIODRIVER", "disk");
    TestReadAtNextPush();
    TestRecreatedPort();
    TestLevelExcludesCushion();
    TestPrimingAfterRunningDry();
    return 0;
}
