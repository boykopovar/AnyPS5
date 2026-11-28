#include "prx/libSceCustomMusicAudioOut/include/Api.hpp"
#include <array>
#include <cerrno>
#include <cmath>
#include <cstdint>
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

    errno = 71;
    Require(sceCustomMusicAudioOutGetVolume(nullptr) == kErrInvalidArg);
    Require(errno == 71);
    errno = 71;
    Require(sceCustomMusicAudioOutGetVolume(reinterpret_cast<double*>(0x10)) == kErrInvalidArg);
    Require(errno == 71);
    errno = 71;
    Require(sceCustomMusicAudioOutGetVolume(&vol) == kErrNotInit);
    Require(errno == 71);

    errno = 71;
    Require(sceCustomMusicAudioOutGetMuted(nullptr) == kErrInvalidArg);
    Require(errno == 71);
    errno = 71;
    Require(sceCustomMusicAudioOutGetMuted(reinterpret_cast<bool*>(0x10)) == kErrInvalidArg);
    Require(errno == 71);
    errno = 71;
    Require(sceCustomMusicAudioOutGetMuted(&muted) == kErrNotInit);
    Require(errno == 71);

    errno = 71;
    Require(sceCustomMusicAudioOutGetAllMusicVolume(nullptr) == kErrInvalidArg);
    Require(errno == 71);
    errno = 71;
    Require(sceCustomMusicAudioOutGetAllMusicVolume(reinterpret_cast<double*>(0x10)) == kErrInvalidArg);
    Require(errno == 71);
    errno = 71;
    Require(sceCustomMusicAudioOutGetAllMusicVolume(&vol) == kErrNotInit);
    Require(errno == 71);

    errno = 71;
    Require(sceCustomMusicAudioOutGetLastOutputTime(nullptr) == kErrInvalidArg);
    Require(errno == 71);
    errno = 71;
    Require(sceCustomMusicAudioOutGetLastOutputTime(reinterpret_cast<void*>(0x10)) == kErrInvalidArg);
    Require(errno == 71);

    Require(sceCustomMusicAudioOutOutput(nullptr) == kErrNotInit);
    std::uint64_t lastTime = 0;
    Require(sceCustomMusicAudioOutGetLastOutputTime(&lastTime) == kErrNotInit);

    Require(sceCustomMusicAudioOutSetVolume(0.5) == kErrNotInit);
    Require(sceCustomMusicAudioOutSetMuted(true) == kErrNotInit);
    Require(sceCustomMusicAudioOutSetAllMusicVolume(0.5) == kErrNotInit);
    Require(sceCustomMusicAudioOutFinalize() == kErrNotInit);

    Require(sceCustomMusicAudioOutInitialize(0, 2, 1) == kErrInvalidArg);
    Require(sceCustomMusicAudioOutInitialize(0, 0, 2) == kErrInvalidArg);
    Require(sceCustomMusicAudioOutInitialize(0, 0, 1) == kErrInvalidArg);
    Require(sceCustomMusicAudioOutInitialize(256, 0, 1) == 0);

    errno = 71;
    Require(sceCustomMusicAudioOutGetVolume(nullptr) == kErrInvalidArg);
    Require(errno == 71);
    errno = 71;
    Require(sceCustomMusicAudioOutGetVolume(reinterpret_cast<double*>(0x10)) == kErrInvalidArg);
    Require(errno == 71);
    errno = 71;
    Require(sceCustomMusicAudioOutGetMuted(nullptr) == kErrInvalidArg);
    Require(errno == 71);
    errno = 71;
    Require(sceCustomMusicAudioOutGetMuted(reinterpret_cast<bool*>(0x10)) == kErrInvalidArg);
    Require(errno == 71);
    errno = 71;
    Require(sceCustomMusicAudioOutGetAllMusicVolume(nullptr) == kErrInvalidArg);
    Require(errno == 71);
    errno = 71;
    Require(sceCustomMusicAudioOutGetAllMusicVolume(reinterpret_cast<double*>(0x10)) == kErrInvalidArg);
    Require(errno == 71);
    errno = 71;
    Require(sceCustomMusicAudioOutGetLastOutputTime(nullptr) == kErrInvalidArg);
    Require(errno == 71);
    errno = 71;
    Require(sceCustomMusicAudioOutGetLastOutputTime(reinterpret_cast<void*>(0x10)) == kErrInvalidArg);
    Require(errno == 71);

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

    std::uint64_t beforeTime = 0;
    Require(sceCustomMusicAudioOutGetLastOutputTime(&beforeTime) == 0);
    std::array<std::int16_t, 256 * 2> pcm{};
    Require(sceCustomMusicAudioOutOutput(pcm.data()) == 256);
    std::uint64_t afterTime = 0;
    Require(sceCustomMusicAudioOutGetLastOutputTime(&afterTime) == 0);
    Require(afterTime >= beforeTime && afterTime > 0);

    Require(sceCustomMusicAudioOutFinalize() == 0);
    Require(sceCustomMusicAudioOutGetVolume(&vol) == kErrNotInit);
    Require(sceCustomMusicAudioOutOutput(pcm.data()) == kErrNotInit);
    Require(sceCustomMusicAudioOutGetLastOutputTime(&afterTime) == kErrNotInit);
    Require(sceCustomMusicAudioOutFinalize() == kErrNotInit);

    Require(sceCustomMusicAudioOutInitialize(512, 1, 4) == 0);
    std::array<float, 512 * 2> pcmFloat{};
    Require(sceCustomMusicAudioOutOutput(pcmFloat.data()) == 512);
    Require(sceCustomMusicAudioOutFinalize() == 0);

    return 0;
}
