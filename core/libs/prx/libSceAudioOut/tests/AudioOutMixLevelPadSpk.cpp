#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <random>
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

static void Require(bool value) { if (!value) std::abort(); }

static void SetEnvironment(const char* name, const std::string& value) {
#ifdef _WIN32
    _putenv_s(name, value.c_str());
#else
    setenv(name, value.c_str(), 1);
#endif
}

template<typename TFunction>
static bool ThrowsRuntimeError(TFunction function) {
    try {
        function();
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

namespace {

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

template<typename TSample, typename TSetup>
std::vector<TSample> Play(int type, std::uint32_t format, const std::vector<TSample>& block, TSetup setup) {
    const auto path = std::filesystem::temp_directory_path() / ("anyps5_audio_out_mix_level_pad_spk-" + std::to_string(std::random_device{}()) + ".raw");
    std::filesystem::remove(path);
    SetEnvironment("SDL_DISKAUDIOFILE", path.string());

    const int handle = sceAudioOutOpen(user, type, 0, frames, frequency, format);
    Require(handle > 0);
    setup(handle);
    Require(sceAudioOutOutput(handle, block.data()) == static_cast<int>(frames));
    Require(sceAudioOutOutput(handle, nullptr) == static_cast<int>(frames));
    Require(sceAudioOutClose(handle) == 0);

    std::ifstream file(path, std::ios::binary);
    const std::vector<char> bytes{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    file.close();
    std::filesystem::remove(path);
    Require(bytes.size() >= block.size() * sizeof(TSample) && bytes.size() % sizeof(TSample) == 0);

    const auto* samples = reinterpret_cast<const TSample*>(bytes.data());
    std::size_t first = 0;
    std::size_t last = bytes.size() / sizeof(TSample);
    while (first < last && samples[first] == TSample{}) first++;
    while (last > first && samples[last - 1] == TSample{}) last--;
    return {samples + first, samples + last};
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
        Require(sample * level % unity == 0);
        scaled.push_back(static_cast<std::int16_t>(sample * level / unity));
    }
    return scaled;
}

void TestDefaultLevel() {
    std::vector<std::int16_t> block(frames);
    for (std::uint32_t frame = 0; frame < frames; frame++) block[frame] = frame % 2 == 0 ? 16384 : -16384;
    const auto played = Play(portTypePadSpeaker, formatS16Mono, block, [](int) {});
    Require(played == Scaled(block, defaultMixLevel));
    Require(played[0] == 5813 && played[1] == -5813);
}

void TestLevels() {
    const auto block = Ramp();
    Require(Play(portTypePadSpeaker, formatS16Mono, block, [](int handle) {
        Require(sceAudioOutSetMixLevelPadSpk(handle, unity) == 0);
    }) == block);
    Require(Play(portTypePadSpeaker, formatS16Mono, block, [](int handle) {
        Require(sceAudioOutSetMixLevelPadSpk(handle, unity / 2) == 0);
    }) == Scaled(block, unity / 2));
    Require(Play(portTypePadSpeaker, formatS16Mono, block, [](int handle) {
        Require(sceAudioOutSetMixLevelPadSpk(handle, unity) == 0);
        Require(sceAudioOutSetMixLevelPadSpk(handle, unity / 4) == 0);
    }) == Scaled(block, unity / 4));
    Require(Play(portTypePadSpeaker, formatS16Mono, block, [](int handle) {
        Require(sceAudioOutSetMixLevelPadSpk(handle, 0) == 0);
    }).empty());
}

void TestLevelWithVolume() {
    const auto block = Ramp();
    Require(Play(portTypePadSpeaker, formatS16Mono, block, [](int handle) {
        int volume = unity / 2;
        Require(sceAudioOutSetVolume(handle, 1, &volume) == 0);
        Require(sceAudioOutSetMixLevelPadSpk(handle, unity / 2) == 0);
    }) == Scaled(block, unity / 4));

    std::vector<float> stereo(frames * 2);
    for (std::uint32_t frame = 0; frame < frames; frame++) {
        stereo[frame * 2] = (frame % 2 == 0 ? 0.5f : -0.25f);
        stereo[frame * 2 + 1] = (frame % 2 == 0 ? -1.0f : 0.125f);
    }
    const auto played = Play(portTypePadSpeaker, formatFloatStereo, stereo, [](int handle) {
        int volume[2] = {unity, unity / 2};
        Require(sceAudioOutSetVolume(handle, 2, volume) == 0);
        Require(sceAudioOutSetMixLevelPadSpk(handle, unity / 4) == 0);
    });
    Require(played.size() == stereo.size());
    for (std::uint32_t frame = 0; frame < frames; frame++) {
        Require(played[frame * 2] == stereo[frame * 2] * 0.25f);
        Require(played[frame * 2 + 1] == stereo[frame * 2 + 1] * 0.125f);
    }
}

void TestOtherPortType() {
    const auto block = Ramp();
    Require(Play(portTypeMain, formatS16Mono, block, [](int) {}) == block);
    Require(Play(portTypeMain, formatS16Mono, block, [](int handle) {
        Require(sceAudioOutSetMixLevelPadSpk(handle, unity / 2) == invalidPortType);
        Require(sceAudioOutSetMixLevelPadSpk(handle, unity + 1) == invalidPortType);
    }) == block);
}

void TestRejectedLevels() {
    const auto block = Ramp();
    int closed = 0;
    Require(Play(portTypePadSpeaker, formatS16Mono, block, [&closed](int handle) {
        closed = handle;
        Require(sceAudioOutSetMixLevelPadSpk(handle, unity / 2) == 0);
        Require(sceAudioOutSetMixLevelPadSpk(handle, unity + 1) == invalidMixLevel);
        Require(ThrowsRuntimeError([handle] { sceAudioOutSetMixLevelPadSpk(handle, -1); }));
        Require(sceAudioOutSetMixLevelPadSpk(0, unity) == invalidPort);
        Require(sceAudioOutSetMixLevelPadSpk(handle + 1, unity) == invalidPort);
    }) == Scaled(block, unity / 2));
    Require(sceAudioOutSetMixLevelPadSpk(closed, unity) == invalidPort);
}

}

int main() {
    SetEnvironment("SDL_AUDIODRIVER", "disk");
    TestDefaultLevel();
    TestLevels();
    TestLevelWithVolume();
    TestOtherPortType();
    TestRejectedLevels();
    return 0;
}
