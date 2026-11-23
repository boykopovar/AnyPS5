#include "prx/libSceCustomMusicAudioOut/include/Api.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr int Frames = 256;
constexpr int Samples = Frames * 2;

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "%s\n", message);
        std::exit(1);
    }
}

void SubmitBlock(std::int16_t amplitude) {
    std::array<std::int16_t, Samples> pcm;
    pcm.fill(amplitude);
    Require(sceCustomMusicAudioOutOutput(pcm.data()) == 0, "PCM submission failed");
    Require(sceCustomMusicAudioOutOutput(nullptr) == 0, "PCM drain failed");
}

}

int main(int argc, char** argv) {
    Require(argc == 2, "Pass volume or all-music-volume");
    const std::string_view mode = argv[1];
    Require(mode == "volume" || mode == "all-music-volume", "Unknown volume setter");
    const auto setVolume = mode == "volume" ? sceCustomMusicAudioOutSetVolume : sceCustomMusicAudioOutSetAllMusicVolume;

    const auto path = std::filesystem::temp_directory_path() /
        ("anyps5-custom-music-mute-" + std::to_string(std::random_device{}()) + ".raw");
#ifdef _WIN32
    Require(_putenv_s("SDL_DISKAUDIOFILE", path.string().c_str()) == 0, "Could not select SDL disk output");
#else
    Require(setenv("SDL_DISKAUDIOFILE", path.string().c_str(), 1) == 0, "Could not select SDL disk output");
#endif

    Require(sceCustomMusicAudioOutInitialize(Frames, 0, 1) == 0, "Custom music initialization failed");
    SubmitBlock(16384);
    Require(sceCustomMusicAudioOutSetMuted(true) == 0, "Muting failed");
    SubmitBlock(20480);
    Require(setVolume(0.25) == 0, "Changing the volume while muted failed");
    bool muted = false;
    Require(sceCustomMusicAudioOutGetMuted(&muted) == 0 && muted, "Volume change cleared the mute flag");
    SubmitBlock(24576);
    Require(sceCustomMusicAudioOutSetMuted(false) == 0, "Unmuting failed");
    SubmitBlock(12288);
    Require(sceCustomMusicAudioOutFinalize() == 0, "Custom music finalization failed");

    std::ifstream capture(path, std::ios::binary);
    Require(capture.is_open(), "SDL did not write the PCM capture");
    std::vector<std::int16_t> nonzero;
    std::int16_t sample = 0;
    while (capture.read(reinterpret_cast<char*>(&sample), sizeof(sample))) {
        if (sample != 0) nonzero.push_back(sample);
    }
    Require(capture.eof() && capture.gcount() == 0, "The PCM capture could not be read");
    capture.close();
    std::filesystem::remove(path);

    std::vector<std::int16_t> expected(Samples, 8192);
    expected.insert(expected.end(), Samples, 3072);
    Require(nonzero == expected, "Only the initial and unmuted blocks should contain nonzero PCM");
    return 0;
}
