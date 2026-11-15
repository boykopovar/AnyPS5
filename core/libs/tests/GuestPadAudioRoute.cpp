#include "SceTypes.hpp"
#include "prx/libScePad/include/Pad.hpp"
#include "prx/libScePad/include/PadState.hpp"
#include <cstdio>
#include <cstdlib>

static void RequireAt(bool value, int line) { if (!value) { std::fprintf(stderr, "check failed at line %d\n", line); std::abort(); } }
#define Require(value) RequireAt((value), __LINE__)

static PadOutputState FetchAt(int port, std::uint32_t& seen, int line) {
    PadOutputState out{};
    RequireAt(PadFetchOutputForPort_nid_postfix(port, &seen, &out), line);
    return out;
}

int main() {
    const PadVolumeGain gain{40, 50, 0, 60};
    Require(scePadIsSupportedAudioFunction(1) == PAD_ERROR_NOT_INITIALIZED);
    Require(scePadSetAudioOutPath(1, 3) == PAD_ERROR_NOT_INITIALIZED);
    Require(scePadSetAudioOutPath(1, 9) == PAD_ERROR_NOT_INITIALIZED);
    Require(scePadSetVolumeGain(1, &gain) == PAD_ERROR_NOT_INITIALIZED);
    Require(scePadSetVolumeGain(1, nullptr) == PAD_ERROR_NOT_INITIALIZED);

    Require(scePadInit_nid_postfix() == PAD_OK);
    for (int handle : {-1, 0, 1, 12345}) {
        Require(scePadIsSupportedAudioFunction(handle) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetAudioOutPath(handle, 3) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetAudioOutPath(handle, 5) == PAD_ERROR_INVALID_ARG);
        Require(scePadSetAudioOutPath(handle, -1) == PAD_ERROR_INVALID_ARG);
        Require(scePadSetVolumeGain(handle, &gain) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetVolumeGain(handle, nullptr) == PAD_ERROR_INVALID_ARG);
    }

    const int handle = scePadOpen_nid_postfix(0x10000000, PAD_PORT_TYPE_STANDARD, 0, nullptr);
    Require(handle > 0);
    std::uint32_t seen = 0xffffffffu;
    PadOutputState out = FetchAt(0, seen, __LINE__);
    Require(out.audioOutPath == 0 && !out.volumeGainValid);

    Require(scePadIsSupportedAudioFunction(handle) == 1);
    for (int path : {0, 1, 2, 3, 4}) {
        Require(scePadSetAudioOutPath(handle, path) == PAD_OK);
        if (path != 0) {
            out = FetchAt(0, seen, __LINE__);
            Require(out.audioOutPath == path);
        }
    }
    Require(scePadSetAudioOutPath(handle, 5) == PAD_ERROR_INVALID_ARG);
    Require(scePadSetAudioOutPath(handle, 0x7fffffff) == PAD_ERROR_INVALID_ARG);
    Require(!PadFetchOutputForPort_nid_postfix(0, &seen, &out));
    Require(scePadSetAudioOutPath(handle, 4) == PAD_OK);
    Require(!PadFetchOutputForPort_nid_postfix(0, &seen, &out));

    Require(scePadSetVolumeGain(handle, nullptr) == PAD_ERROR_INVALID_ARG);
    Require(scePadSetVolumeGain(handle, &gain) == PAD_OK);
    out = FetchAt(0, seen, __LINE__);
    Require(out.volumeGainValid && out.speakerVolume == 40 && out.headsetVolume == 50 && out.micGain == 60);
    Require(scePadSetVolumeGain(handle, &gain) == PAD_OK);
    Require(!PadFetchOutputForPort_nid_postfix(0, &seen, &out));
    const PadVolumeGain other{0, 255, 7, 1};
    Require(scePadSetVolumeGain(handle, &other) == PAD_OK);
    out = FetchAt(0, seen, __LINE__);
    Require(out.speakerVolume == 0 && out.headsetVolume == 255 && out.micGain == 1);

    PadSetPortConnected_nid_postfix(0, false);
    Require(scePadIsSupportedAudioFunction(handle) == static_cast<int>(0x80920007));
    Require(scePadSetAudioOutPath(handle, 1) == PAD_OK);
    PadSetPortConnected_nid_postfix(0, true);
    Require(scePadClose_nid_postfix(handle) == PAD_OK);
    Require(scePadSetAudioOutPath(handle, 2) == PAD_OK);

    const int special = scePadOpen_nid_postfix(0x10000000, PAD_PORT_TYPE_SPECIAL, 0, nullptr);
    Require(special > 0);
    Require(scePadIsSupportedAudioFunction(special) == static_cast<int>(0x80920007));
    Require(scePadSetAudioOutPath(special, 1) == PAD_OK);
    Require(scePadSetAudioOutPath(special, 5) == PAD_ERROR_INVALID_ARG);
    Require(scePadSetVolumeGain(special, &gain) == PAD_OK);
    Require(scePadSetVolumeGain(special, nullptr) == PAD_ERROR_INVALID_ARG);
    out = FetchAt(0, seen, __LINE__);
    Require(out.audioOutPath == 2);
    Require(scePadClose_nid_postfix(special) == PAD_OK);
}
