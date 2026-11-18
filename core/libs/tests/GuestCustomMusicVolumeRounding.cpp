#include "prx/libSceCustomMusicAudioOut/include/Api.hpp"
#include <cstdio>

namespace {

int failures = 0;

bool CheckStatus(int status, const char* operation) {
    if (status == 0) {
        return true;
    }
    std::fprintf(stderr, "%s: expected status 0, got %#x\n", operation, static_cast<unsigned>(status));
    ++failures;
    return false;
}

void CheckVolume(double actual, double expected, double input, const char* operation) {
    if (actual != expected) {
        std::fprintf(stderr, "%s input %a: expected %a, got %a\n", operation, input, expected, actual);
        ++failures;
    }
}

}

int main() {
    if (!CheckStatus(sceCustomMusicAudioOutInitialize(0, 0, 1), "initialize")) {
        return 1;
    }

    double savedAllVolume = 0.0;
    double savedPortVolume = 0.0;
    bool savedMuted = false;
    const bool savedAll = CheckStatus(sceCustomMusicAudioOutGetAllMusicVolume(&savedAllVolume), "save all-music volume");
    const bool savedPort = CheckStatus(sceCustomMusicAudioOutGetVolume(&savedPortVolume), "save port volume");
    const bool savedMute = CheckStatus(sceCustomMusicAudioOutGetMuted(&savedMuted), "save mute");
    if (!savedAll || !savedPort || !savedMute) {
        CheckStatus(sceCustomMusicAudioOutFinalize(), "finalize after state read failure");
        return 1;
    }

    const struct {
        double input;
        double expected;
    } cases[] = {
        {1.25 / 32768.0, 1.0 / 32768.0},
        {1.5 / 32768.0, 2.0 / 32768.0},
        {1.75 / 32768.0, 2.0 / 32768.0},
        {2.5 / 32768.0, 3.0 / 32768.0},
        {32767.25 / 32768.0, 32767.0 / 32768.0},
        {32767.5 / 32768.0, 1.0},
        {32767.75 / 32768.0, 1.0},
    };

    for (const auto& test : cases) {
        CheckStatus(sceCustomMusicAudioOutSetAllMusicVolume(0.75), "set all-music sentinel");
        CheckStatus(sceCustomMusicAudioOutSetVolume(test.input), "set port volume");
        double portVolume = 0.0;
        if (CheckStatus(sceCustomMusicAudioOutGetVolume(&portVolume), "read port volume")) {
            CheckVolume(portVolume, test.expected, test.input, "set port volume");
        }

        CheckStatus(sceCustomMusicAudioOutSetVolume(0.25), "set port sentinel");
        CheckStatus(sceCustomMusicAudioOutSetAllMusicVolume(test.input), "set all-music volume");
        double allVolume = 0.0;
        if (CheckStatus(sceCustomMusicAudioOutGetAllMusicVolume(&allVolume), "read all-music volume")) {
            CheckVolume(allVolume, test.expected, test.input, "set all-music volume: all-music readback");
        }
        if (CheckStatus(sceCustomMusicAudioOutGetVolume(&portVolume), "read port volume after all-music setter")) {
            CheckVolume(portVolume, test.expected, test.input, "set all-music volume: port readback");
        }
    }

    CheckStatus(sceCustomMusicAudioOutSetAllMusicVolume(savedAllVolume), "restore all-music volume");
    CheckStatus(sceCustomMusicAudioOutSetVolume(savedPortVolume), "restore port volume");
    CheckStatus(sceCustomMusicAudioOutSetMuted(savedMuted), "restore mute");
    CheckStatus(sceCustomMusicAudioOutFinalize(), "finalize");
    return failures == 0 ? 0 : 1;
}
