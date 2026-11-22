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
constexpr int kErrAudioOut = static_cast<int>(0x81380003);
constexpr int kErrNotInit = static_cast<int>(0x81380102);
constexpr int kErrAudioInvalidSize = static_cast<int>(0x80260006);
constexpr int kFrames = 256;

}

int main() {
    double vol = 0.0;
    bool muted = false;
    std::uint64_t lastOutput = 99;

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
    Require(sceCustomMusicAudioOutGetLastOutputTime(reinterpret_cast<std::uint64_t*>(0x10)) == kErrInvalidArg);
    Require(errno == 71);
    Require(sceCustomMusicAudioOutGetLastOutputTime(&lastOutput) == kErrNotInit);
    Require(lastOutput == 99);
    Require(sceCustomMusicAudioOutOutput(nullptr) == kErrNotInit);

    Require(sceCustomMusicAudioOutSetVolume(0.5) == kErrNotInit);
    Require(sceCustomMusicAudioOutSetMuted(true) == kErrNotInit);
    Require(sceCustomMusicAudioOutSetAllMusicVolume(0.5) == kErrNotInit);
    Require(sceCustomMusicAudioOutFinalize() == kErrNotInit);

    Require(sceCustomMusicAudioOutInitialize(0, 2, 1) == kErrInvalidArg);
    Require(sceCustomMusicAudioOutInitialize(0, 0, 2) == kErrInvalidArg);
    Require(sceCustomMusicAudioOutInitialize(kFrames, 0, 1) == 0);

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
    Require(sceCustomMusicAudioOutGetLastOutputTime(reinterpret_cast<std::uint64_t*>(0x10)) == kErrInvalidArg);
    Require(errno == 71);

    Require(sceCustomMusicAudioOutGetLastOutputTime(&lastOutput) == 0);
    Require(lastOutput == 0);
    Require(sceCustomMusicAudioOutOutput(reinterpret_cast<const void*>(0x10)) == kErrAudioOut);
    Require(sceCustomMusicAudioOutGetLastOutputTime(&lastOutput) == 0);
    Require(lastOutput == 0);
    std::array<std::int16_t, kFrames * 2> stereo{};
    stereo.front() = 1024;
    Require(sceCustomMusicAudioOutOutput(stereo.data()) == 0);
    Require(sceCustomMusicAudioOutGetLastOutputTime(&lastOutput) == 0);
    Require(lastOutput > 0);
    const auto submittedAt = lastOutput;
    Require(sceCustomMusicAudioOutOutput(nullptr) == 0);
    Require(sceCustomMusicAudioOutGetLastOutputTime(&lastOutput) == 0);
    Require(lastOutput == submittedAt);
    Require(sceCustomMusicAudioOutInitialize(0, 0, 1) == kErrAudioInvalidSize);
    Require(sceCustomMusicAudioOutInitialize(32769, 0, 1) == kErrAudioInvalidSize);
    Require(sceCustomMusicAudioOutGetLastOutputTime(&lastOutput) == 0);
    Require(lastOutput == submittedAt);

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

    Require(sceCustomMusicAudioOutInitialize(kFrames, 1, 4) == 0);
    Require(sceCustomMusicAudioOutGetLastOutputTime(&lastOutput) == 0);
    Require(lastOutput == 0);
    Require(sceCustomMusicAudioOutGetMuted(&muted) == 0);
    Require(!muted);
    Require(sceCustomMusicAudioOutGetVolume(&vol) == 0);
    Require(vol == 0.25);
    std::array<float, kFrames * 2> floatStereo{};
    floatStereo.front() = 0.25f;
    Require(sceCustomMusicAudioOutOutput(floatStereo.data()) == 0);
    Require(sceCustomMusicAudioOutGetLastOutputTime(&lastOutput) == 0);
    Require(lastOutput > 0);

    Require(sceCustomMusicAudioOutFinalize() == 0);
    Require(sceCustomMusicAudioOutGetVolume(&vol) == kErrNotInit);
    Require(sceCustomMusicAudioOutGetLastOutputTime(&lastOutput) == kErrNotInit);
    Require(sceCustomMusicAudioOutOutput(stereo.data()) == kErrNotInit);
    Require(sceCustomMusicAudioOutFinalize() == kErrNotInit);

    return 0;
}
