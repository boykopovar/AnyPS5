#include "prx/libc/include/general/VabiMacros.hpp"
#include "AudioOutTestSupport.hpp"

#include <Testing/Test.hpp>

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <vector>

extern "C" {
int APS5_VABI sceAudioOutOpen(int, int, int, std::uint32_t, std::uint32_t, std::uint32_t);
int APS5_VABI sceAudioOutClose(int);
int APS5_VABI sceAudioOutOutput(int, const void*);
int APS5_VABI sceAudioOutSetVolume(int, std::uint32_t, int*);
int APS5_VABI sceAudioOutSetMixLevelPadSpk(int, int);
}

namespace {

using Testing::Case;
using Testing::Require;
using Testing::RequireEqual;

constexpr int user = 0x10000000;
constexpr int portTypeMain = 0;
constexpr int portTypePadSpeaker = 4;
constexpr std::uint32_t formatS16Mono = 0;
constexpr std::uint32_t formatFloatStereo = 4;
constexpr std::uint32_t frames = 256;
constexpr std::uint32_t frequency = 48000;
constexpr int unity = 32768;
constexpr int defaultMixLevel = 11626;
constexpr int invalidPort = static_cast<int>(0x80260003);
constexpr int invalidPortType = static_cast<int>(0x8026000A);
constexpr int invalidMixLevel = static_cast<int>(0x80260014);

class Port {
public:
    Port(int type, std::uint32_t format) : handle(sceAudioOutOpen(user, type, 0, frames, frequency, format)) {
        Require(handle > 0, "port must open: " + std::to_string(handle));
    }

    ~Port() {
        if (open) sceAudioOutClose(handle);
    }

    Port(const Port&) = delete;
    Port& operator=(const Port&) = delete;

    template<typename TSample>
    void Play(const std::vector<TSample>& block) {
        RequireEqual(sceAudioOutOutput(handle, block.data()), static_cast<int>(frames), "output the block");
        RequireEqual(sceAudioOutOutput(handle, nullptr), static_cast<int>(frames), "wait for the block");
        open = false;
        RequireEqual(sceAudioOutClose(handle), 0, "close the port");
    }

    const int handle;

private:
    bool open = true;
};

template<typename TSample>
std::vector<TSample> Captured(const DiskAudioCapture& capture, std::size_t blockSize) {
    const auto all = capture.Samples<TSample>();
    Require(all.size() >= blockSize, "the capture holds at least the block");
    return capture.TrimmedSamples<TSample>();
}

std::vector<std::int16_t> Ramp() {
    std::vector<std::int16_t> block(frames);
    for (std::uint32_t frame = 0; frame < frames; frame++) {
        block[frame] = static_cast<std::int16_t>((frame % 2 == 0 ? 1 : -1) * 256 * (static_cast<int>(frame % 100) + 1));
    }
    return block;
}

std::vector<std::int16_t> Scaled(const std::vector<std::int16_t>& block, int level) {
    std::vector<std::int16_t> scaled;
    for (const std::int16_t sample : block) {
        Require(sample * level % unity == 0, "test data scales exactly");
        scaled.push_back(static_cast<std::int16_t>(sample * level / unity));
    }
    return scaled;
}

void RequireSamples(const std::vector<std::int16_t>& played, const std::vector<std::int16_t>& expected, const std::string& message) {
    RequireEqual(played.size(), expected.size(), message + " sample count");
    for (std::size_t i = 0; i < played.size(); ++i) RequireEqual(played[i], expected[i], message + " sample " + std::to_string(i));
}

std::vector<std::int16_t> PlayPadSpeaker(const std::vector<std::int16_t>& block, std::initializer_list<int> levels) {
    const DiskAudioCapture capture;
    Port port(portTypePadSpeaker, formatS16Mono);
    for (const int level : levels) {
        RequireEqual(sceAudioOutSetMixLevelPadSpk(port.handle, level), 0, "set mix level " + std::to_string(level));
    }
    port.Play(block);
    return Captured<std::int16_t>(capture, block.size());
}

const Case defaultLevel{"MixLevelPadSpk_Default_AttenuatesPadSpeaker", [] {
    std::vector<std::int16_t> block(frames);
    for (std::uint32_t frame = 0; frame < frames; frame++) block[frame] = frame % 2 == 0 ? 16384 : -16384;
    const auto played = PlayPadSpeaker(block, {});
    RequireSamples(played, Scaled(block, defaultMixLevel), "default level");
    RequireEqual(played[0], 5813, "first sample");
    RequireEqual(played[1], -5813, "second sample");
}};

const Case unityLevel{"MixLevelPadSpk_Unity_PlaysBlockUnchanged", [] {
    const auto block = Ramp();
    RequireSamples(PlayPadSpeaker(block, {unity}), block, "unity level");
}};

const Case halfLevel{"MixLevelPadSpk_Half_HalvesSamples", [] {
    const auto block = Ramp();
    RequireSamples(PlayPadSpeaker(block, {unity / 2}), Scaled(block, unity / 2), "half level");
}};

const Case lastLevelWins{"MixLevelPadSpk_SetTwice_UsesLastLevel", [] {
    const auto block = Ramp();
    RequireSamples(PlayPadSpeaker(block, {unity, unity / 4}), Scaled(block, unity / 4), "quarter level after unity");
}};

const Case zeroLevel{"MixLevelPadSpk_Zero_PlaysSilence", [] {
    RequireEqual(PlayPadSpeaker(Ramp(), {0}).size(), std::size_t{0}, "audible samples at level 0");
}};

const Case monoVolume{"MixLevelPadSpk_WithPortVolume_MultipliesBoth", [] {
    const auto block = Ramp();
    const DiskAudioCapture capture;
    Port port(portTypePadSpeaker, formatS16Mono);
    int volume = unity / 2;
    RequireEqual(sceAudioOutSetVolume(port.handle, 1, &volume), 0, "set the volume");
    RequireEqual(sceAudioOutSetMixLevelPadSpk(port.handle, unity / 2), 0, "set the mix level");
    port.Play(block);
    RequireSamples(Captured<std::int16_t>(capture, block.size()), Scaled(block, unity / 4), "volume and level");
}};

const Case stereoVolume{"MixLevelPadSpk_FloatStereoWithChannelVolumes_ScalesEachChannel", [] {
    std::vector<float> stereo(frames * 2);
    for (std::uint32_t frame = 0; frame < frames; frame++) {
        stereo[frame * 2] = (frame % 2 == 0 ? 0.5f : -0.25f);
        stereo[frame * 2 + 1] = (frame % 2 == 0 ? -1.0f : 0.125f);
    }
    const DiskAudioCapture capture;
    Port port(portTypePadSpeaker, formatFloatStereo);
    int volume[2] = {unity, unity / 2};
    RequireEqual(sceAudioOutSetVolume(port.handle, 2, volume), 0, "set the volumes");
    RequireEqual(sceAudioOutSetMixLevelPadSpk(port.handle, unity / 4), 0, "set the mix level");
    port.Play(stereo);
    const auto played = Captured<float>(capture, stereo.size());
    RequireEqual(played.size(), stereo.size(), "sample count");
    for (std::uint32_t frame = 0; frame < frames; frame++) {
        RequireEqual(played[frame * 2], stereo[frame * 2] * 0.25f, "left frame " + std::to_string(frame));
        RequireEqual(played[frame * 2 + 1], stereo[frame * 2 + 1] * 0.125f, "right frame " + std::to_string(frame));
    }
}};

const Case mainPort{"MixLevelPadSpk_MainPort_IsRejectedAndDoesNotScale", [] {
    const auto block = Ramp();
    {
        const DiskAudioCapture capture;
        Port port(portTypeMain, formatS16Mono);
        port.Play(block);
        RequireSamples(Captured<std::int16_t>(capture, block.size()), block, "main port without a level");
    }
    const DiskAudioCapture capture;
    Port port(portTypeMain, formatS16Mono);
    RequireEqual(sceAudioOutSetMixLevelPadSpk(port.handle, unity / 2), invalidPortType, "half level on a main port");
    RequireEqual(sceAudioOutSetMixLevelPadSpk(port.handle, unity + 1), invalidPortType, "too high level on a main port");
    port.Play(block);
    RequireSamples(Captured<std::int16_t>(capture, block.size()), block, "main port after rejected levels");
}};

const Case rejectedLevels{"MixLevelPadSpk_InvalidLevelOrPort_IsRejectedAndKeepsLevel", [] {
    const auto block = Ramp();
    const DiskAudioCapture capture;
    Port port(portTypePadSpeaker, formatS16Mono);
    const int handle = port.handle;
    RequireEqual(sceAudioOutSetMixLevelPadSpk(handle, unity / 2), 0, "half level");
    RequireEqual(sceAudioOutSetMixLevelPadSpk(handle, unity + 1), invalidMixLevel, "level above unity");
    Testing::RequireThrows<std::runtime_error>([handle] { sceAudioOutSetMixLevelPadSpk(handle, -1); }, "negative level");
    RequireEqual(sceAudioOutSetMixLevelPadSpk(0, unity), invalidPort, "handle 0");
    RequireEqual(sceAudioOutSetMixLevelPadSpk(handle + 1, unity), invalidPort, "unopened handle");
    port.Play(block);
    RequireSamples(Captured<std::int16_t>(capture, block.size()), Scaled(block, unity / 2), "level kept after rejections");
    RequireEqual(sceAudioOutSetMixLevelPadSpk(handle, unity), invalidPort, "closed handle");
}};

} // namespace
