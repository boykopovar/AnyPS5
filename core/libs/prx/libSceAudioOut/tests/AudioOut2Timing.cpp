#include "prx/libSceAudioOut/include/AudioOut2.hpp"
#include "prx/libSceAudioOut/src/AudioOut2Internal.hpp"
#include "SceTypes.hpp"

#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
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

namespace {

constexpr std::uint32_t grain = 256;
constexpr std::uint32_t queueDepth = 2;
constexpr std::uint32_t cushionGrains = 8;
constexpr std::uint32_t frequency = 48000;
constexpr std::uint32_t formatMonoFloat = 1u << 8;
constexpr std::uint32_t formatStereoFloat = 2u << 8;
constexpr std::uint32_t attributeData = 0;
constexpr int queueFull = static_cast<int>(0x80260507);
constexpr int errorInvalidArgument = static_cast<int>(0x80260502);
constexpr int errorInvalidHandle = static_cast<int>(0x80260503);
constexpr float masterGain = 0.5f;
constexpr float tolerance = 1e-5f;

struct Session {
    std::filesystem::path path;
    AudioOut2ContextHandle context = 0;
    AudioOut2PortHandle port = 0;
    std::vector<float> buffer = std::vector<float>(static_cast<std::size_t>(grain) * 2, 0.0f);

    bool low;

    explicit Session(const char* name, bool lowLatency = false) : path(std::filesystem::temp_directory_path() / (std::to_string(std::random_device{}()) + "-" + name)), low(lowLatency) {
        std::filesystem::remove(path);
        SetEnvironment("SDL_DISKAUDIOFILE", path.string());
        AudioOut2ContextParam params{};
        Require(sceAudioOut2ContextResetParam(&params) == 0);
        params.num_grains = grain;
        params.queue_depth = queueDepth;
        Require((low ? sceAudioOut2LoContextCreate : sceAudioOut2ContextCreate)(&params, nullptr, 0, &context) == 0);
        AudioOut2PortParam port{};
        port.port_type = 0;
        port.data_format = formatStereoFloat;
        port.sampling_freq = frequency;
        Require((low ? sceAudioOut2LoPortCreate : sceAudioOut2PortCreate)(context, &port, &this->port) == 0);
    }

    void Point(const void* data) {
        const AudioOut2Attribute attribute{attributeData, 0, &data, sizeof(data)};
        Require((low ? sceAudioOut2LoPortSetAttributes : sceAudioOut2PortSetAttributes)(port, &attribute, 1) == 0);
    }

    void Fill(float value) {
        for (float& sample : buffer) sample = value;
    }

    std::vector<float> Close() {
        Require((low ? sceAudioOut2LoPortDestroy : sceAudioOut2PortDestroy)(port) == 0);
        Require((low ? sceAudioOut2LoContextDestroy : sceAudioOut2ContextDestroy)(context) == 0);
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

void TestDeviceLevelDoesNotWaitForTheContext(bool low) {
    Session session("anyps5_audio_out2_level_poll.raw", low);
    auto& context = *reinterpret_cast<AudioOut2Context*>(session.context);
    Require(context.device != 0);
    std::promise<void> started;
    auto ready = started.get_future();
    std::unique_lock lock(context.lock);
    auto poll = std::async(std::launch::async, [&] {
        started.set_value();
        std::uint32_t level = 99;
        std::uint32_t available = 99;
        Require((low ? sceAudioOut2LoContextGetQueueLevel : sceAudioOut2ContextGetQueueLevel)(session.context, &level, &available) == 0);
        return std::pair{level, available};
    });
    ready.wait();
    const bool completed = poll.wait_for(std::chrono::seconds(1)) == std::future_status::ready;
    lock.unlock();
    const auto result = poll.get();
    session.Close();
    Require(completed);
    Require(result.first == 0 && result.second == queueDepth);
}

void TestLevelWithoutADeviceUsesThePlaybackClock() {
    AudioOut2Context context;
    context.queueDepth = queueDepth;
    context.queued = queueDepth;
    context.playHead = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    const auto handle = reinterpret_cast<AudioOut2ContextHandle>(&context);
    std::uint32_t level = 99;
    std::uint32_t available = 99;
    Require(sceAudioOut2ContextGetQueueLevel(handle, &level, &available) == 0);
    Require(level == queueDepth && available == 0);
    context.playHead = std::chrono::steady_clock::now() - std::chrono::seconds(5);
    Require(sceAudioOut2LoContextGetQueueLevel(handle, &level, &available) == 0);
    Require(level == 0 && available == queueDepth);
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

void TestLowLatencyCallsRejectBadArguments() {
    AudioOut2ContextParam params{};
    Require(sceAudioOut2ContextResetParam(&params) == 0);
    params.num_grains = grain;
    params.queue_depth = queueDepth;
    const std::size_t sizeSentinel = 0x1234;
    std::size_t size = sizeSentinel;
    Require(sceAudioOut2LoContextQueryMemory(nullptr, &size) == errorInvalidArgument);
    Require(sceAudioOut2LoContextQueryMemory(&params, nullptr) == errorInvalidArgument);
    Require(size == sizeSentinel);
    Require(sceAudioOut2LoContextQueryMemory(&params, &size) == 0);
    std::size_t standardSize = sizeSentinel;
    Require(sceAudioOut2ContextQueryMemory(&params, &standardSize) == 0);
    Require(size == standardSize && size != sizeSentinel);

    const AudioOut2ContextHandle contextSentinel = 0x5A5A;
    AudioOut2ContextHandle context = contextSentinel;
    Require(sceAudioOut2LoContextCreate(nullptr, nullptr, 0, &context) == errorInvalidArgument);
    Require(context == contextSentinel);
    Require(sceAudioOut2LoContextCreate(&params, nullptr, 0, nullptr) == errorInvalidArgument);

    Require(sceAudioOut2LoContextDestroy(0) == errorInvalidHandle);
    Require(sceAudioOut2LoContextAdvance(0) == errorInvalidHandle);
    Require(sceAudioOut2LoContextPush(0, 0) == errorInvalidHandle);
    Require(sceAudioOut2LoContextPush(0, 1) == errorInvalidHandle);
    std::uint32_t level = 77;
    std::uint32_t available = 78;
    Require(sceAudioOut2LoContextGetQueueLevel(0, &level, &available) == errorInvalidHandle);
    Require(level == 77 && available == 78);
    const AudioOut2Attribute attribute{0, 0, nullptr, 0};
    Require(sceAudioOut2LoContextSetAttributes(0, &attribute, 1) == errorInvalidHandle);

    AudioOut2PortParam portParams{};
    portParams.port_type = 0;
    portParams.data_format = formatStereoFloat;
    portParams.sampling_freq = frequency;
    const AudioOut2PortHandle portSentinel = 0x6B6B;
    AudioOut2PortHandle port = portSentinel;
    Require(sceAudioOut2LoPortCreate(0, &portParams, &port) == errorInvalidHandle);
    Require(port == portSentinel);
    Require(sceAudioOut2LoPortDestroy(0) == errorInvalidHandle);
    Require(sceAudioOut2LoPortSetAttributes(0, &attribute, 1) == errorInvalidHandle);
    AudioOut2PortState state;
    Require(sceAudioOut2LoPortGetState(0, &state) == errorInvalidHandle);

    Session session("anyps5_audio_out2_low_arguments.raw", true);
    Require(sceAudioOut2LoPortCreate(session.context, nullptr, &port) == errorInvalidArgument);
    Require(port == portSentinel);
    Require(sceAudioOut2LoPortCreate(session.context, &portParams, nullptr) == errorInvalidArgument);
    Require(sceAudioOut2LoContextSetAttributes(session.context, nullptr, 1) == errorInvalidArgument);
    Require(sceAudioOut2LoContextSetAttributes(session.context, nullptr, 0) == 0);
    Require(sceAudioOut2LoContextSetAttributes(session.context, &attribute, 1) == 0);
    Require(sceAudioOut2LoPortSetAttributes(session.port, nullptr, 1) == errorInvalidArgument);
    Require(sceAudioOut2LoPortSetAttributes(session.port, nullptr, 0) == 0);
    Require(sceAudioOut2LoPortGetState(session.port, nullptr) == errorInvalidArgument);
    Require(sceAudioOut2LoContextGetQueueLevel(session.context, nullptr, nullptr) == 0);
    std::memset(&state, 0xAB, sizeof(state));
    Require(sceAudioOut2LoPortGetState(session.port, &state) == 0);
    Require(state.output == 1 && state.num_channels == 2 && state.volume == 127 && state.reroute_counter == 0 && state.flags == 0);
    const auto destroyed = session.port;
    Require(sceAudioOut2LoPortDestroy(destroyed) == 0);
    Require(sceAudioOut2LoPortDestroy(destroyed) == errorInvalidHandle);
    Require(sceAudioOut2LoPortGetState(destroyed, &state) == errorInvalidHandle);
    Require(sceAudioOut2LoPortSetAttributes(destroyed, &attribute, 1) == errorInvalidHandle);
    Require(sceAudioOut2LoContextDestroy(session.context) == 0);
    std::filesystem::remove(session.path);
}

void TestLowLatencyReadsAtNextPush() {
    Session session("anyps5_audio_out2_low_next_push.raw", true);
    session.Fill(0.25f);
    session.Point(session.buffer.data());
    Require(sceAudioOut2LoContextPush(session.context, 1) == 0);
    session.Fill(0.5f);
    Require(sceAudioOut2LoContextPush(session.context, 1) == 0);
    session.Fill(0.75f);
    Require(sceAudioOut2LoContextPush(session.context, 1) == 0);
    session.Fill(0.0f);
    session.Point(nullptr);
    for (std::uint32_t push = 0; push < cushionGrains * 4; push++) Require(sceAudioOut2LoContextPush(session.context, 1) == 0);
    const auto played = session.Close();
    const auto first = FirstNonZero(played);
    const std::size_t samples = static_cast<std::size_t>(grain) * 2;
    Require(Constant(played, first, samples, 0.5f * masterGain));
    Require(Constant(played, first + samples, samples, 0.75f * masterGain));
    Require(FirstNonZero(played, first + 2 * samples) == played.size());
}

void TestLowLatencyAdvanceKeepsTheAdvancedGrain() {
    Session session("anyps5_audio_out2_low_advance.raw", true);
    session.Fill(0.25f);
    session.Point(session.buffer.data());
    Require(sceAudioOut2LoContextAdvance(session.context) == 0);
    session.Fill(0.75f);
    session.Point(nullptr);
    for (std::uint32_t push = 0; push < cushionGrains * 4; push++) {
        Require(sceAudioOut2LoContextPush(session.context, 1) == 0);
        Require(sceAudioOut2LoContextAdvance(session.context) == 0);
    }
    const auto played = session.Close();
    const auto first = FirstNonZero(played);
    const std::size_t samples = static_cast<std::size_t>(grain) * 2;
    Require(Constant(played, first, samples, 0.25f * masterGain));
    Require(FirstNonZero(played, first + samples) == played.size());
}

void TestLowLatencyQueueLevelFillsToTheDepth() {
    Session session("anyps5_audio_out2_low_level.raw", true);
    session.Point(nullptr);
    std::uint32_t level = 99;
    std::uint32_t available = 99;
    Require(sceAudioOut2LoContextGetQueueLevel(session.context, &level, &available) == 0);
    Require(level == 0 && available == queueDepth);
    std::uint32_t accepted = 0;
    int result = 0;
    while ((result = sceAudioOut2LoContextPush(session.context, 0)) == 0) {
        accepted++;
        Require(accepted < 1000);
    }
    Require(result == queueFull);
    Require(accepted >= cushionGrains + queueDepth);
    Require(sceAudioOut2LoContextGetQueueLevel(session.context, &level, &available) == 0);
    Require(level <= queueDepth && level + available == queueDepth);
    session.Close();
}

}

int main() {
    SetEnvironment("SDL_AUDIODRIVER", "disk");
    TestReadAtNextPush();
    TestRecreatedPort();
    TestLevelExcludesCushion();
    TestDeviceLevelDoesNotWaitForTheContext(false);
    TestDeviceLevelDoesNotWaitForTheContext(true);
    TestLevelWithoutADeviceUsesThePlaybackClock();
    TestPrimingAfterRunningDry();
    TestLowLatencyCallsRejectBadArguments();
    TestLowLatencyReadsAtNextPush();
    TestLowLatencyAdvanceKeepsTheAdvancedGrain();
    TestLowLatencyQueueLevelFillsToTheDepth();
    return 0;
}
