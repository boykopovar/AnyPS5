#include "SceTypes.hpp"
#include "AudioOutTestSupport.hpp"

#include <Testing/Test.hpp>

#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
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

namespace {

using Testing::Case;
using Testing::Require;
using Testing::RequireEqual;

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
constexpr std::size_t stereoGrainSamples = static_cast<std::size_t>(grain) * 2;

class Session {
public:
    Session() {
        AudioOut2ContextParam params{};
        RequireEqual(sceAudioOut2ContextResetParam(&params), 0, "reset the context parameters");
        params.num_grains = grain;
        params.queue_depth = queueDepth;
        RequireEqual(sceAudioOut2ContextCreate(&params, nullptr, 0, &context), 0, "create the context");
        port = CreatePort(formatStereoFloat);
    }

    ~Session() {
        if (port != 0) sceAudioOut2PortDestroy(port);
        if (context != 0) sceAudioOut2ContextDestroy(context);
    }

    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;

    AudioOut2PortHandle CreatePort(std::uint32_t format) {
        AudioOut2PortParam param{};
        param.port_type = 0;
        param.data_format = format;
        param.sampling_freq = frequency;
        AudioOut2PortHandle handle = 0;
        RequireEqual(sceAudioOut2PortCreate(context, &param, &handle), 0, "create the port");
        return handle;
    }

    void Point(const void* data) {
        const AudioOut2Attribute attribute{attributeData, 0, &data, sizeof(data)};
        RequireEqual(sceAudioOut2PortSetAttributes(port, &attribute, 1), 0, "point the port at its data");
    }

    void Fill(float value) {
        for (float& sample : buffer) sample = value;
    }

    void Push(std::uint32_t count, const char* message) {
        for (std::uint32_t push = 0; push < count; push++) RequireEqual(sceAudioOut2ContextPush(context, 1), 0, message);
    }

    std::vector<float> Close() {
        const auto closingPort = port;
        const auto closingContext = context;
        port = 0;
        context = 0;
        RequireEqual(sceAudioOut2PortDestroy(closingPort), 0, "destroy the port");
        RequireEqual(sceAudioOut2ContextDestroy(closingContext), 0, "destroy the context");
        return capture.Samples<float>();
    }

    const DiskAudioCapture capture;
    AudioOut2ContextHandle context = 0;
    AudioOut2PortHandle port = 0;
    std::vector<float> buffer = std::vector<float>(stereoGrainSamples, 0.0f);
};

std::size_t FirstNonZero(const std::vector<float>& played, std::size_t from = 0) {
    while (from < played.size() && played[from] == 0.0f) from++;
    return from;
}

void RequireConstant(const std::vector<float>& played, std::size_t first, std::size_t count, float value, const std::string& message) {
    Require(first + count <= played.size(), message + ": capture holds " + std::to_string(count) + " samples from " + std::to_string(first));
    for (std::size_t index = first; index < first + count; index++) {
        Require(std::fabs(played[index] - value) <= tolerance,
                message + " sample " + std::to_string(index) + ": expected " + std::to_string(value) + ", got " + std::to_string(played[index]));
    }
}

const Case nextPush{"ContextPush_PortData_IsReadAtTheNextPush", [] {
    Session session;
    session.Fill(0.25f);
    session.Point(session.buffer.data());
    session.Push(1, "push 0.25");
    session.Fill(0.5f);
    session.Push(1, "push 0.5");
    session.Fill(0.75f);
    session.Push(1, "push 0.75");
    session.Fill(0.0f);
    session.Point(nullptr);
    session.Push(cushionGrains * 4, "push silence");
    const auto played = session.Close();
    const auto first = FirstNonZero(played);
    RequireConstant(played, first, stereoGrainSamples, 0.5f * masterGain, "first audible grain");
    RequireConstant(played, first + stereoGrainSamples, stereoGrainSamples, 0.75f * masterGain, "second audible grain");
    RequireEqual(FirstNonZero(played, first + 2 * stereoGrainSamples), played.size(), "silence after the two grains");
}};

const Case recreatedPort{"PortCreate_AfterDestroyInSameContext_ReusesHandleWithNewFormat", [] {
    Session session;
    session.Fill(0.5f);
    session.Point(session.buffer.data());
    session.Push(1, "push stereo");
    const auto first = session.port;
    const auto destroyed = session.port;
    session.port = 0;
    RequireEqual(sceAudioOut2PortDestroy(destroyed), 0, "destroy the stereo port");
    session.port = session.CreatePort(formatMonoFloat);
    RequireEqual(session.port, first, "recreated handle");
    const std::vector<float> monoBuffer(grain, 0.75f);
    session.Point(monoBuffer.data());
    session.Push(1, "push mono");
    session.Point(nullptr);
    session.Push(cushionGrains * 4, "push silence");
    const auto played = session.Close();
    const auto start = FirstNonZero(played);
    RequireConstant(played, start, stereoGrainSamples, 0.75f * masterGain, "mono grain");
    RequireEqual(FirstNonZero(played, start + stereoGrainSamples), played.size(), "silence after the mono grain");
}};

const Case queueLevel{"ContextGetQueueLevel_EmptyQueue_ExcludesCushion", [] {
    Session session;
    session.Point(nullptr);
    std::uint32_t level = 99;
    std::uint32_t available = 99;
    RequireEqual(sceAudioOut2ContextGetQueueLevel(session.context, &level, &available), 0, "queue level");
    RequireEqual(level, 0u, "level");
    RequireEqual(available, queueDepth, "available");
    std::uint32_t accepted = 0;
    int result = 0;
    while ((result = sceAudioOut2ContextPush(session.context, 0)) == 0) {
        accepted++;
        Require(accepted < 1000, "the queue fills up");
    }
    RequireEqual(result, queueFull, "push into a full queue");
    Require(accepted >= cushionGrains + queueDepth, "accepted " + std::to_string(accepted) + " non-blocking pushes");
    session.Close();
}};

const Case priming{"ContextPush_AfterRunningDry_PrimesBeforeNextBurst", [] {
    Session session;
    session.Fill(0.5f);
    session.Point(session.buffer.data());
    session.Push(12, "push the first burst");
    session.Point(nullptr);
    session.Push(1, "push one silent grain");
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    session.Fill(-0.5f);
    session.Point(session.buffer.data());
    session.Push(12, "push the second burst");
    session.Point(nullptr);
    session.Push(cushionGrains * 4, "push silence");
    const auto played = session.Close();
    const std::size_t burst = stereoGrainSamples * 12;
    const auto first = FirstNonZero(played);
    RequireConstant(played, first, burst, 0.5f * masterGain, "first burst");
    const auto second = FirstNonZero(played, first + burst);
    Require(second < played.size(), "second burst is audible");
    Require(second - (first + burst) >= stereoGrainSamples, "a gap of at least one grain separates the bursts");
    RequireConstant(played, second, burst, -0.5f * masterGain, "second burst");
}};

} // namespace
