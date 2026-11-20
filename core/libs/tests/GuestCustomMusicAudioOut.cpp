#include "prx/libSceCustomMusicAudioOut/include/Api.hpp"
#include <cmath>
#include <cstdlib>

namespace {

void Require(bool value) {
    if (!value) {
        std::abort();
    }
}

constexpr int kErrInvalidArg = static_cast<int>(0x81380001);
constexpr int kErrNotInit = static_cast<int>(0x81380102);

}

int main() {
    double vol = 0.0;
    bool muted = false;

    Require(sceCustomMusicAudioOutGetVolume(nullptr) == kErrInvalidArg);
    Require(sceCustomMusicAudioOutGetVolume(&vol) == kErrNotInit);
    Require(sceCustomMusicAudioOutGetMuted(nullptr) == kErrInvalidArg);
    Require(sceCustomMusicAudioOutGetMuted(&muted) == kErrNotInit);
    Require(sceCustomMusicAudioOutGetAllMusicVolume(nullptr) == kErrInvalidArg);
    Require(sceCustomMusicAudioOutGetAllMusicVolume(&vol) == kErrNotInit);
    Require(sceCustomMusicAudioOutSetVolume(0.5) == kErrNotInit);
    Require(sceCustomMusicAudioOutSetMuted(true) == kErrNotInit);
    Require(sceCustomMusicAudioOutSetAllMusicVolume(0.5) == kErrNotInit);
    Require(sceCustomMusicAudioOutFinalize() == kErrNotInit);

    Require(sceCustomMusicAudioOutInitialize(0, 2, 1) == kErrInvalidArg);
    Require(sceCustomMusicAudioOutInitialize(0, 0, 2) == kErrInvalidArg);
    Require(sceCustomMusicAudioOutInitialize(0, 0, 1) == 0);

    Require(sceCustomMusicAudioOutGetVolume(&vol) == 0);
    Require(std::abs(vol - 0.5) < 0.001);
    Require(sceCustomMusicAudioOutGetMuted(&muted) == 0);
    Require(!muted);
    Require(sceCustomMusicAudioOutGetAllMusicVolume(&vol) == 0);
    Require(std::abs(vol - 0.5) < 0.001);

    Require(sceCustomMusicAudioOutSetVolume(0.75) == 0);
    Require(sceCustomMusicAudioOutGetVolume(&vol) == 0);
    Require(std::abs(vol - 0.75) < 0.001);

    Require(sceCustomMusicAudioOutSetMuted(true) == 0);
    Require(sceCustomMusicAudioOutGetMuted(&muted) == 0);
    Require(muted);

    Require(sceCustomMusicAudioOutSetAllMusicVolume(0.25) == 0);
    Require(sceCustomMusicAudioOutGetAllMusicVolume(&vol) == 0);
    Require(std::abs(vol - 0.25) < 0.001);
    Require(sceCustomMusicAudioOutGetVolume(&vol) == 0);
    Require(std::abs(vol - 0.25) < 0.001);

    Require(sceCustomMusicAudioOutSetVolume(1.5) == 0);
    Require(sceCustomMusicAudioOutGetVolume(&vol) == 0);
    Require(std::abs(vol - 1.0) < 0.001);

    Require(sceCustomMusicAudioOutSetVolume(-0.5) == 0);
    Require(sceCustomMusicAudioOutGetVolume(&vol) == 0);
    Require(std::abs(vol - 0.0) < 0.001);

    Require(sceCustomMusicAudioOutFinalize() == 0);
    Require(sceCustomMusicAudioOutGetVolume(&vol) == kErrNotInit);
    Require(sceCustomMusicAudioOutFinalize() == kErrNotInit);

    return 0;
}
