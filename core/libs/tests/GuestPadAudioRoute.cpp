#include "SceTypes.hpp"
#include "prx/libScePad/include/Pad.hpp"
#include "prx/libScePad/include/PadHapticBuffer.hpp"
#include "prx/libScePad/include/PadState.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

static void RequireAt(bool value, int line) { if (!value) { std::fprintf(stderr, "check failed at line %d\n", line); std::abort(); } }
#define Require(value) RequireAt((value), __LINE__)

static PadOutputState FetchAt(int port, std::uint32_t& seen, int line) {
    PadOutputState out{};
    RequireAt(PadFetchOutputForPort_nid_postfix(port, &seen, &out), line);
    return out;
}

static void TestPadHapticBuffer() {
    {
        PadHapticBuffer buf;
        std::array<float, 16> left{};
        std::array<float, 16> right{};
        left.fill(0.5f);
        right.fill(-0.25f);
        buf.Push(left.data(), right.data(), 1, 16, 1500);
        Require(buf.Frames() == 32);
        std::array<std::int8_t, PadHapticBuffer::BlockFrames * 2> block{};
        Require(buf.Take(block));
        Require(buf.Frames() == 0);
        for (std::size_t i = 0; i < PadHapticBuffer::BlockFrames; ++i) {
            Require(block[i * 2] == 64);
            Require(block[i * 2 + 1] == -32);
        }
    }

    {
        PadHapticBuffer buf;
        const std::array<float, 2> left{0.5f, -0.5f};
        const std::array<float, 2> right{-0.25f, 0.25f};
        buf.Push(left.data(), right.data(), 1, 2, 2000);
        Require(buf.Frames() == 3);
        const std::array<float, 20> padLeft{};
        const std::array<float, 20> padRight{};
        buf.Push(padLeft.data(), padRight.data(), 1, 20, 2000);
        Require(buf.Frames() == 33);
        std::array<std::int8_t, PadHapticBuffer::BlockFrames * 2> block{};
        Require(buf.Take(block));
        Require(block[0] == 64 && block[1] == -32);
        Require(block[2] == -64 && block[3] == 32);
        Require(block[4] == -64 && block[5] == 32);
    }

    {
        PadHapticBuffer buf;
        std::array<float, 15> left48{};
        std::array<float, 15> right48{};
        left48.fill(1.0f);
        right48.fill(-1.0f);
        buf.Push(left48.data(), right48.data(), 1, 15, 48000);
        Require(buf.Frames() == 0);

        const float zero = 0.0f;
        buf.Push(&zero, &zero, 0, 1, 3000);
        Require(buf.Frames() == 1);
        buf.Push(&zero, &zero, 0, 31, 3000);
        Require(buf.Frames() == 32);

        std::array<std::int8_t, PadHapticBuffer::BlockFrames * 2> block{};
        Require(buf.Take(block));
        for (std::size_t i = 0; i < PadHapticBuffer::BlockFrames * 2; ++i) {
            Require(block[i] == 0);
        }
    }

    {
        PadHapticBuffer buf;
        const float one = 1.0f;
        const float negOne = -1.0f;
        buf.Push(&one, &negOne, 0, 1431655u, 0xFFFFF800u);
        Require(buf.Frames() == 0);
        buf.Push(&one, &negOne, 0, 1u, 0xFFFFF800u);
        Require(buf.Frames() == 1);
    }

    {
        PadHapticBuffer buf;
        const std::array<float, 4> samples{0.5f, 0.5f, 0.5f, 0.5f};
        buf.Push(nullptr, samples.data(), 1, 4, 3000);
        Require(buf.Frames() == 0);
        buf.Push(samples.data(), nullptr, 1, 4, 3000);
        Require(buf.Frames() == 0);
        buf.Push(samples.data(), samples.data(), 1, 0, 3000);
        Require(buf.Frames() == 0);
        buf.Push(samples.data(), samples.data(), 1, 4, 0);
        Require(buf.Frames() == 0);
    }

    {
        PadHapticBuffer buf;
        std::array<float, 15> left48{};
        std::array<float, 15> right48{};
        left48.fill(0.5f);
        right48.fill(-0.25f);
        buf.Push(left48.data(), right48.data(), 1, 15, 48000);
        Require(buf.Frames() == 0);
        const float zero = 0.0f;
        buf.Push(&zero, &zero, 1, 0, 3000);
        buf.Push(left48.data(), right48.data(), 1, 1, 48000);
        Require(buf.Frames() == 1);
    }

    {
        PadHapticBuffer buf;
        std::array<float, 11> left{};
        std::array<float, 11> right{};
        left.fill(0.5f);
        right.fill(-0.25f);
        buf.Push(left.data(), right.data(), 1, 10, 1000);
        Require(buf.Frames() == 30);
        buf.Push(left.data(), right.data(), 1, 1, 1000);
        Require(buf.Frames() == 33);
        std::array<std::int8_t, PadHapticBuffer::BlockFrames * 2> block{};
        Require(buf.Take(block));
        Require(buf.Frames() == 1);
        for (std::size_t i = 0; i < PadHapticBuffer::BlockFrames; ++i) {
            Require(block[i * 2] == 64);
            Require(block[i * 2 + 1] == -32);
        }
    }

    {
        PadHapticBuffer buf;
        std::array<float, 512> left{};
        std::array<float, 512> right{};
        for (std::size_t i = 0; i < left.size(); ++i) {
            left[i] = (i % 16) < 8 ? 0.5f : 0.0f;
            right[i] = -0.5f;
        }
        buf.Push(left.data(), right.data(), 1, 512, 48000, 50);
        Require(buf.Frames() == 32);
        std::array<std::int8_t, PadHapticBuffer::BlockFrames * 2> block{};
        Require(buf.Take(block));
        Require(buf.Frames() == 0);
        for (std::size_t i = 0; i < PadHapticBuffer::BlockFrames; ++i) {
            Require(block[i * 2] == 16);
            Require(block[i * 2 + 1] == -32);
        }
    }

    {
        PadHapticBuffer buf;
        std::array<float, 16> leftA{};
        std::array<float, 16> rightA{};
        leftA.fill(0.5f);
        rightA.fill(-0.25f);
        buf.Push(leftA.data(), rightA.data(), 1, 8, 1500);
        Require(buf.Frames() == 16);

        std::array<float, 16> leftB{};
        std::array<float, 16> rightB{};
        leftB.fill(-0.5f);
        rightB.fill(0.25f);
        buf.Push(leftB.data(), rightB.data(), 1, 16, 3000);
        Require(buf.Frames() == 32);

        std::array<std::int8_t, PadHapticBuffer::BlockFrames * 2> block{};
        Require(buf.Take(block));
        Require(buf.Frames() == 0);
        for (std::size_t i = 0; i < 16; ++i) {
            Require(block[i * 2] == 64 && block[i * 2 + 1] == -32);
        }
        for (std::size_t i = 16; i < 32; ++i) {
            Require(block[i * 2] == -64 && block[i * 2 + 1] == 32);
        }
    }
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

    TestPadHapticBuffer();
}
