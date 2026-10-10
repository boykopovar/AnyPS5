#include "SceTypes.hpp"
#include "AudioOutTestSupport.hpp"

#include <Testing/Test.hpp>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
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

namespace {

using Testing::Case;
using Testing::Require;
using Testing::RequireEqual;

constexpr std::uint32_t grain = 256;
constexpr std::uint32_t frequency = 48000;
constexpr std::uint16_t portTypeMain = 0;
constexpr std::uint32_t attributeData = 0;
constexpr std::uint32_t attributeVolume = 1;
constexpr std::uint32_t formatFloat = 0;
constexpr std::uint32_t formatS16 = 1;
constexpr std::uint32_t silentGrains = 32;
constexpr float masterGain = 0.5f;
constexpr float fold = 0.70710678f;
constexpr float tolerance = 1e-5f;

struct Layout {
    const char* name;
    std::uint32_t channels;
    std::vector<float> left;
    std::vector<float> right;
};

const Layout mono{"mono", 1, {1.0f}, {1.0f}};
const Layout stereo{"stereo", 2, {1.0f, 0.0f}, {0.0f, 1.0f}};
const Layout surround71{"7.1", 8, {1.0f, 0.0f, fold, 0.0f, fold, 0.0f, fold, 0.0f}, {0.0f, 1.0f, fold, 0.0f, 0.0f, fold, 0.0f, fold}};
const Layout surround714{"7.1.4", 12, {1.0f, 0.0f, fold, 0.0f, fold, 0.0f, fold, 0.0f, fold, 0.0f, fold * fold, 0.0f},
    {0.0f, 1.0f, fold, 0.0f, 0.0f, fold, 0.0f, fold, 0.0f, fold, 0.0f, fold * fold}};

std::uint32_t Format(std::uint32_t channels, std::uint32_t type) {
    return channels << 8 | type;
}

class Context {
public:
    Context() {
        AudioOut2ContextParam params{};
        RequireEqual(sceAudioOut2ContextResetParam(&params), 0, "reset the context parameters");
        params.num_grains = grain;
        params.queue_depth = 1;
        RequireEqual(sceAudioOut2ContextCreate(&params, nullptr, 0, &handle), 0, "create the context");
        Require(handle != 0, "context handle is set");
    }

    ~Context() {
        if (handle != 0) sceAudioOut2ContextDestroy(handle);
    }

    Context(const Context&) = delete;
    Context& operator=(const Context&) = delete;

    void Destroy() {
        const auto context = handle;
        handle = 0;
        RequireEqual(sceAudioOut2ContextDestroy(context), 0, "destroy the context");
    }

    AudioOut2ContextHandle handle = 0;
};

int CreatePort(AudioOut2ContextHandle context, std::uint32_t format, AudioOut2PortHandle* port) {
    AudioOut2PortParam params{};
    params.port_type = portTypeMain;
    params.data_format = format;
    params.sampling_freq = frequency;
    return sceAudioOut2PortCreate(context, &params, port);
}

void SetData(AudioOut2PortHandle port, const void* data) {
    const AudioOut2Attribute attribute{attributeData, 0, &data, sizeof(data)};
    RequireEqual(sceAudioOut2PortSetAttributes(port, &attribute, 1), 0, "set the data attribute");
}

void SetVolume(AudioOut2PortHandle port, const std::vector<float>& volume) {
    const AudioOut2Attribute attribute{attributeVolume, 0, volume.data(), volume.size() * sizeof(float)};
    RequireEqual(sceAudioOut2PortSetAttributes(port, &attribute, 1), 0, "set the volume attribute");
}

std::vector<float> Play(std::uint32_t format, const void* data, const std::vector<float>& volume) {
    const DiskAudioCapture capture;
    {
        Context context;
        AudioOut2PortHandle port = 0;
        RequireEqual(CreatePort(context.handle, format, &port), 0, "create the port");
        SetVolume(port, volume);
        SetData(port, data);
        RequireEqual(sceAudioOut2ContextPush(context.handle, 1), 0, "push the grain");
        SetData(port, nullptr);
        for (std::uint32_t push = 0; push < silentGrains; push++) RequireEqual(sceAudioOut2ContextPush(context.handle, 1), 0, "push silence");
        RequireEqual(sceAudioOut2PortDestroy(port), 0, "destroy the port");
        context.Destroy();
    }
    return capture.TrimmedSamples<float>();
}

float Sample(std::uint32_t channel, std::uint32_t frame) {
    const float sign = frame % 2 == 0 ? 1.0f : -1.0f;
    return sign * 0.01f * static_cast<float>(channel + 1) * static_cast<float>(1 + frame % 5);
}

std::vector<float> FloatGrain(std::uint32_t channels) {
    std::vector<float> data(static_cast<std::size_t>(grain) * channels);
    for (std::uint32_t frame = 0; frame < grain; frame++) {
        for (std::uint32_t channel = 0; channel < channels; channel++) data[frame * channels + channel] = Sample(channel, frame);
    }
    return data;
}

std::vector<std::int16_t> S16Grain(std::uint32_t channels) {
    std::vector<std::int16_t> data(static_cast<std::size_t>(grain) * channels);
    for (std::uint32_t frame = 0; frame < grain; frame++) {
        for (std::uint32_t channel = 0; channel < channels; channel++) {
            data[frame * channels + channel] = static_cast<std::int16_t>(std::lround(Sample(channel, frame) * 32768.0f));
        }
    }
    return data;
}

std::vector<float> Volume(std::uint32_t channels) {
    std::vector<float> volume(channels);
    for (std::uint32_t channel = 0; channel < channels; channel++) volume[channel] = 1.0f - 0.05f * static_cast<float>(channel);
    return volume;
}

void RequireClose(float actual, float expected, const std::string& message) {
    Require(std::fabs(actual - expected) <= tolerance, message + ": expected " + std::to_string(expected) + ", got " + std::to_string(actual));
}

template<typename TSample>
void RequireFold(const std::vector<float>& played, const Layout& layout, const std::vector<TSample>& data, const std::vector<float>& volume,
                 float scale, const std::string& name) {
    RequireEqual(played.size(), static_cast<std::size_t>(grain) * 2, name + " sample count");
    for (std::uint32_t frame = 0; frame < grain; frame++) {
        float left = 0.0f;
        float right = 0.0f;
        for (std::uint32_t channel = 0; channel < layout.channels; channel++) {
            const float sample = static_cast<float>(data[frame * layout.channels + channel]) * scale * volume[channel];
            left += sample * layout.left[channel];
            right += sample * layout.right[channel];
        }
        RequireClose(played[frame * 2], left * masterGain, name + " left frame " + std::to_string(frame));
        RequireClose(played[frame * 2 + 1], right * masterGain, name + " right frame " + std::to_string(frame));
    }
}

const Case floatLayouts{"PortFloat_EveryLayout_FoldsToStereo", [] {
    for (const Layout* layout : {&mono, &stereo, &surround71, &surround714}) {
        const auto data = FloatGrain(layout->channels);
        const auto volume = Volume(layout->channels);
        RequireFold(Play(Format(layout->channels, formatFloat), data.data(), volume), *layout, data, volume, 1.0f,
                    std::string("float ") + layout->name);
    }
}};

const Case s16Layouts{"PortS16_EveryLayout_FoldsToStereo", [] {
    for (const Layout* layout : {&mono, &stereo, &surround71, &surround714}) {
        const auto data = S16Grain(layout->channels);
        const auto volume = Volume(layout->channels);
        RequireFold(Play(Format(layout->channels, formatS16), data.data(), volume), *layout, data, volume, 1.0f / 32768.0f,
                    std::string("s16 ") + layout->name);
    }
}};

const Case standardBit{"PortFloat_Surround71WithStandardBit_FoldsLike71", [] {
    const auto data = FloatGrain(8);
    const auto volume = Volume(8);
    RequireFold(Play(Format(8, formatFloat) | 0x80, data.data(), volume), surround71, data, volume, 1.0f, "7.1 with bit 0x80");
}};

const Case highBits{"PortFloat_StereoWithHighFormatBits_FoldsLikeStereo", [] {
    const auto data = FloatGrain(2);
    const auto volume = Volume(2);
    RequireFold(Play(0xFFFFF200u, data.data(), volume), stereo, data, volume, 1.0f, "stereo 0xFFFFF200");
}};

const Case heightChannels{"PortFloat_HeightChannelsOnly_FoldWithDoubleAttenuation", [] {
    std::vector<float> data(static_cast<std::size_t>(grain) * surround714.channels, 0.0f);
    for (std::uint32_t frame = 0; frame < grain; frame++) {
        data[frame * 12 + 8] = 0.5f;
        data[frame * 12 + 9] = -0.25f;
        data[frame * 12 + 10] = 0.5f;
        data[frame * 12 + 11] = 1.0f;
    }
    const std::vector<float> volume{1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.5f};
    const auto played = Play(Format(12, formatFloat), data.data(), volume);
    RequireEqual(played.size(), static_cast<std::size_t>(grain) * 2, "sample count");
    for (std::uint32_t frame = 0; frame < grain; frame++) {
        RequireClose(played[frame * 2], (0.5f * fold + 0.5f * fold * fold) * masterGain, "left frame " + std::to_string(frame));
        RequireClose(played[frame * 2 + 1], (-0.25f * fold + 0.5f * fold * fold) * masterGain, "right frame " + std::to_string(frame));
    }
}};

const Case droppedLfe{"PortFloat_InfiniteLfe_IsDropped", [] {
    auto data = FloatGrain(surround714.channels);
    auto expected = data;
    for (std::uint32_t frame = 0; frame < grain; frame++) {
        data[frame * surround714.channels + 3] = std::numeric_limits<float>::infinity();
        expected[frame * surround714.channels + 3] = 0.0f;
    }
    const auto volume = Volume(surround714.channels);
    RequireFold(Play(Format(surround714.channels, formatFloat), data.data(), volume), surround714, expected, volume, 1.0f, "7.1.4 with LFE");
}};

const Case measuredFormats{"PortCreate_MeasuredMainPortFormats_MatchHardwareResults", [] {
    struct FormatResult {
        std::uint32_t format;
        std::uint32_t result;
    };
    constexpr FormatResult cases[] = {
        {0x000, 0x8026800E}, {0x001, 0x8026800E}, {0x080, 0x80268001},
        {0x100, 0}, {0x101, 0}, {0x102, 0x8026800E}, {0x17F, 0x8026800E},
        {0x180, 0x80268001}, {0x181, 0x80268001}, {0x1FF, 0x80268001},
        {0x200, 0}, {0x201, 0}, {0x202, 0x8026800E}, {0x27F, 0x8026800E},
        {0x280, 0x80268001}, {0x281, 0x80268001}, {0x2FF, 0x80268001},
        {0x300, 0x8026800E}, {0x301, 0x8026800E}, {0x380, 0x80268001},
        {0x400, 0x8026800E}, {0x401, 0x8026800E}, {0x500, 0x8026800E},
        {0x600, 0x8026800E}, {0x601, 0x8026800E}, {0x680, 0x80268001},
        {0x700, 0x8026800E}, {0x701, 0x8026800E}, {0x780, 0x80268001},
        {0x800, 0}, {0x801, 0}, {0x802, 0x8026800E}, {0x87F, 0x8026800E},
        {0x880, 0}, {0x881, 0}, {0x882, 0x8026800E}, {0x8FF, 0x8026800E},
        {0x900, 0x8026800E}, {0x901, 0x8026800E}, {0x980, 0x80268001},
        {0xA00, 0x8026800E}, {0xB00, 0x8026800E}, {0xC00, 0}, {0xC01, 0},
        {0xC02, 0x8026800E}, {0xC7F, 0x8026800E}, {0xC80, 0x80268001},
        {0xC81, 0x80268001}, {0xD00, 0x8026800E}, {0xE00, 0x8026800E},
        {0xF00, 0x8026800E}, {0xFFF, 0x80268001},
        {0xFFFFF100, 0}, {0xFFFFF201, 0}, {0xFFFFF800, 0}, {0xFFFFFC01, 0},
        {0xFFFFF600, 0x8026800E}, {0xFFFFF202, 0x8026800E}, {0xFFFFF280, 0x80268001},
    };
    const DiskAudioCapture capture;
    Context context;
    for (const auto& test : cases) {
        const auto name = "format " + std::to_string(test.format);
        AudioOut2PortHandle port = 0xAAAAAAAAAAAAAAAAull;
        RequireEqual(static_cast<std::uint32_t>(CreatePort(context.handle, test.format, &port)), test.result, name);
        if (test.result == 0) {
            Require(port != 0 && port != static_cast<AudioOut2PortHandle>(-1), name + " returns a port");
            RequireEqual(sceAudioOut2PortDestroy(port), 0, name + " port destroys");
        } else {
            RequireEqual(port, static_cast<AudioOut2PortHandle>(-1), name + " returns the invalid port");
        }
    }
}};

const Case ignoredBits{"PortCreate_ValidFormatWithAnyHighBit_Succeeds", [] {
    const DiskAudioCapture capture;
    Context context;
    for (const std::uint32_t format : {0x100u, 0x201u, 0x800u, 0xC01u}) {
        for (std::uint32_t bit = 12; bit < 32; bit++) {
            const auto name = "format " + std::to_string(format) + " with bit " + std::to_string(bit);
            AudioOut2PortHandle port = 0;
            RequireEqual(CreatePort(context.handle, format | (1u << bit), &port), 0, name);
            RequireEqual(sceAudioOut2PortDestroy(port), 0, name + " port destroys");
        }
    }
    context.Destroy();
}};

} // namespace
