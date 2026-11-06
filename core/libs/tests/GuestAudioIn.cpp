#include "prx/libSceAudioIn/include/AudioIn.hpp"
#include "prx/libSceAudioIn/AudioInCapture.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

static void Require(bool value) { if (!value) std::abort(); }

template <typename TCall>
static bool ReportsNotImplemented(const char* name, TCall call) {
    try {
        call();
    } catch (const std::runtime_error& error) {
        return std::string(error.what()) == std::string(name) + " not implemented";
    }
    return false;
}

static void SetEnvironment(const char* name, const std::string& value) {
#ifdef _WIN32
    _putenv_s(name, value.c_str());
#else
    setenv(name, value.c_str(), 1);
#endif
}

namespace {

constexpr int user = 0x10000000;

void TestOpenEx() {
    constexpr int invalidSize = static_cast<int>(0x80260102);
    constexpr int invalidFreq = static_cast<int>(0x80260103);
    constexpr int invalidType = static_cast<int>(0x80260104);
    constexpr int invalidParam = static_cast<int>(0x80260106);
    Require(sceAudioInOpenEx(user, 2, 0, 7, 256, 48000, 2) == invalidType);
    Require(sceAudioInOpenEx(user, 0, 0, 7, 512, 48000, 2) == invalidSize);
    Require(sceAudioInOpenEx(user, 0, 0, 7, 256, 44100, 2) == invalidFreq);
    Require(sceAudioInOpenEx(user, 0, 0, 7, 256, 48000, 0) == invalidParam);
    Require(sceAudioInOpenEx(user, 0, 0, 7, 256, 48000, 3) == invalidParam);
    Require(sceAudioInHqOpenEx(user, 0, 0, 7, 128, 44100, 2) == invalidFreq);
    Require(sceAudioInHqOpenEx(user, 0, 0, 7, 128, 48000, 0) == invalidParam);
    Require(sceAudioInHqOpenEx(user, 0, 0, 7, 128, 48000, 3) == invalidParam);
    int port = sceAudioInOpenEx(user, 0, 0, 7, 256, 48000, 2);
    Require(port > 0 && sceAudioInClose(port) == 0);
    port = sceAudioInHqOpenEx(user, 0, 0, 7, 256, 16000, 2);
    Require(port > 0 && sceAudioInClose(port) == 0);
    Require(sceAudioInSetCompressorPreGain() == 0 && sceAudioInSetNoiseGateThreshold() == 0);
}

void TestMicrophoneMuteBounds() {
    constexpr int invalidHandle = static_cast<int>(0x80260101);
    for (const int badHandle : {-1000, -100, -2, -1, 0, 9, 10, 100, 1000, 0x7FFFFFFF}) {
        Require(sceAudioInGetSilentState(badHandle) == invalidHandle);
    }
    for (int handle = 1; handle <= 8; ++handle) {
        Require(sceAudioInGetSilentState(handle) == invalidHandle);
    }
    const int port = sceAudioInOpen(user, 0, 0, 256, 48000, 2);
    Require(port > 0);
    const int silentState = sceAudioInGetSilentState(port);
    Require(silentState == 0 || silentState == 1);
    for (int handle = 1; handle <= 8; ++handle) {
        if (handle != port) {
            Require(sceAudioInGetSilentState(handle) == invalidHandle);
        }
    }
    Require(sceAudioInClose(port) == 0);
    Require(sceAudioInGetSilentState(port) == invalidHandle);
    Require(ReportsNotImplemented("sceAudioInSetDeviceMuteStatus", [] { return sceAudioInSetDeviceMuteStatus(); }));
    Require(ReportsNotImplemented("sceAudioInSetUserMute", [] { return sceAudioInSetUserMute(); }));
    Require(ReportsNotImplemented("sceAudioInVmicSetMute", [] { return sceAudioInVmicSetMute(); }));
}

void TestStreamFormatRejection() {
    constexpr int invalidParam = static_cast<int>(0x80260106);
    constexpr std::array<std::uint32_t, 15> rejectedFormats = {
        0u, 3u, 4u, 5u, 6u, 7u, 8u, 0x0Fu, 0x13u, 0x14u, 0x1Fu, 0x20u, 0x100u, 0xFFFFu, 0xFFFFFFFFu
    };
    for (const std::uint32_t badFormat : rejectedFormats) {
        Require(sceAudioInOpen(user, 0, 0, 256, 48000, badFormat) == invalidParam);
        Require(sceAudioInOpen(user, 0, 0, 128, 48000, badFormat) == invalidParam);
        Require(sceAudioInOpen(user, 0, 0, 256, 16000, badFormat) == invalidParam);
        Require(sceAudioInOpen(user, 0, 0, 128, 16000, badFormat) == invalidParam);
        Require(sceAudioInOpenEx(user, 0, 0, 0, 256, 48000, badFormat) == invalidParam);
        Require(sceAudioInOpenEx(user, 0, 0, 7, 128, 48000, badFormat) == invalidParam);
        Require(sceAudioInOpenEx(user, 0, 0, 0, 256, 16000, badFormat) == invalidParam);
        Require(sceAudioInHqOpenEx(user, 0, 0, 0, 256, 48000, badFormat) == invalidParam);
        Require(sceAudioInHqOpenEx(user, 0, 0, 7, 128, 48000, badFormat) == invalidParam);
        Require(sceAudioInAsyncOpen(user, 0, 0, 128, 48000, badFormat) == invalidParam);
    }
    constexpr std::array<std::uint32_t, 11> rejectedHqFormats = {
        0u, 1u, 3u, 4u, 0x0Fu, 0x10u, 0x11u, 0x12u, 0x13u, 0x20u, 0xFFFFFFFFu
    };
    for (const std::uint32_t badFormat : rejectedHqFormats) {
        Require(sceAudioInHqOpen(user, 0, 0, 128, 48000, badFormat) == invalidParam);
    }
    for (const std::uint32_t validFormat : {1u, 2u, 0x10u, 0x11u, 0x12u}) {
        const int p1 = sceAudioInOpen(user, 0, 0, 256, 48000, validFormat);
        Require(p1 > 0);
        Require(sceAudioInClose(p1) == 0);
        const int p2 = sceAudioInAsyncOpen(user, 0, 0, 128, 48000, validFormat);
        Require(p2 > 0);
        Require(sceAudioInClose(p2) == 0);
    }
}

void TestSamplingRateValidation() {
    constexpr int invalidFreq = static_cast<int>(0x80260103);
    constexpr std::array<std::uint32_t, 12> rejectedFreqs = {
        0u, 8000u, 11025u, 12000u, 22050u, 24000u, 32000u, 44100u, 88200u, 96000u, 192000u, 0xFFFFFFFFu
    };
    for (const std::uint32_t badFreq : rejectedFreqs) {
        Require(sceAudioInOpen(user, 0, 0, 128, badFreq, 2) == invalidFreq);
        Require(sceAudioInOpen(user, 0, 0, 256, badFreq, 2) == invalidFreq);
        Require(sceAudioInOpen(user, 0, 0, 256, badFreq, 0x10) == invalidFreq);
        Require(sceAudioInOpenEx(user, 0, 0, 0, 128, badFreq, 2) == invalidFreq);
        Require(sceAudioInOpenEx(user, 0, 0, 7, 256, badFreq, 2) == invalidFreq);
        Require(sceAudioInHqOpenEx(user, 0, 0, 0, 128, badFreq, 2) == invalidFreq);
        Require(sceAudioInHqOpenEx(user, 0, 0, 7, 256, badFreq, 2) == invalidFreq);
        Require(sceAudioInHqOpen(user, 0, 0, 128, badFreq, 2) == invalidFreq);
        Require(sceAudioInAsyncOpen(user, 0, 0, 128, badFreq, 2) == invalidFreq);
    }
    Require(sceAudioInHqOpen(user, 0, 0, 128, 16000, 2) == invalidFreq);
    Require(sceAudioInAsyncOpen(user, 0, 0, 128, 16000, 2) == invalidFreq);
    for (const std::uint32_t validFreq : {16000u, 48000u}) {
        const int p1 = sceAudioInOpen(user, 0, 0, 256, validFreq, 2);
        Require(p1 > 0);
        Require(sceAudioInClose(p1) == 0);
        const int p2 = sceAudioInOpen(user, 0, 0, 128, validFreq, 1);
        Require(p2 > 0);
        Require(sceAudioInClose(p2) == 0);
        const int p3 = sceAudioInOpenEx(user, 0, 0, 0, 256, validFreq, 2);
        Require(p3 > 0);
        Require(sceAudioInClose(p3) == 0);
        const int p4 = sceAudioInHqOpenEx(user, 0, 0, 7, 128, validFreq, 2);
        Require(p4 > 0);
        Require(sceAudioInClose(p4) == 0);
    }
    const int hqPort = sceAudioInHqOpen(user, 0, 0, 128, 48000, 2);
    Require(hqPort > 0);
    Require(sceAudioInClose(hqPort) == 0);
    const int asyncPort = sceAudioInAsyncOpen(user, 0, 0, 128, 48000, 2);
    Require(asyncPort > 0);
    Require(sceAudioInClose(asyncPort) == 0);
}

void TestValidation() {
    constexpr int invalidHandle = static_cast<int>(0x80260101);
    constexpr int invalidSize = static_cast<int>(0x80260102);
    constexpr int invalidFreq = static_cast<int>(0x80260103);
    constexpr int invalidType = static_cast<int>(0x80260104);
    constexpr int invalidPointer = static_cast<int>(0x80260105);
    constexpr int invalidParam = static_cast<int>(0x80260106);
    Require(sceAudioInOpen(user, 2, 0, 256, 48000, 2) == invalidType);
    Require(sceAudioInOpen(user, 0, 1, 256, 48000, 2) == invalidParam);
    Require(sceAudioInOpen(user, 0, 0, 512, 48000, 2) == invalidSize);
    Require(sceAudioInOpen(user, 0, 0, 256, 44100, 2) == invalidFreq);
    Require(sceAudioInOpen(user, 0, 0, 256, 48000, 3) == invalidParam);
    Require(sceAudioInOpen(user, 0, 0, 256, 48000, 0) == invalidParam);
    Require(sceAudioInHqOpen(user, 2, 0, 128, 48000, 2) == invalidType);
    Require(sceAudioInHqOpen(user, 0, 1, 128, 48000, 2) == invalidParam);
    Require(sceAudioInHqOpen(user, 0, 0, 512, 48000, 2) == invalidSize);
    Require(sceAudioInHqOpen(user, 0, 0, 128, 44100, 2) == invalidFreq);
    Require(sceAudioInHqOpen(user, 0, 0, 128, 48000, 3) == invalidParam);
    Require(sceAudioInAsyncOpen(user, 2, 0, 128, 48000, 2) == invalidType);
    Require(sceAudioInAsyncOpen(user, 0, 1, 128, 48000, 2) == invalidParam);
    Require(sceAudioInAsyncOpen(user, 0, 0, 385, 48000, 2) == invalidSize);
    Require(sceAudioInAsyncOpen(user, 0, 0, 200, 48000, 2) == invalidSize);
    Require(sceAudioInAsyncOpen(user, 0, 0, 256, 44100, 2) == invalidFreq);
    Require(sceAudioInAsyncOpen(user, 0, 0, 256, 48000, 0) == invalidSize);
    Require(sceAudioInHqOpen(user, 0, 0, 256, 48000, 2) == invalidSize);
    Require(sceAudioInHqOpen(user, 0, 0, 128, 16000, 2) == invalidFreq);
    for (const int badHandle : {-1000, -100, -2, -1, 0, 1, 9, 10, 100, 1000, 0x7FFFFFFF}) {
        float gain = -2.5f;
        Require(sceAudioInGetGain(badHandle, &gain) == invalidHandle && gain == 1.0f);
        Require(sceAudioInGetRerouteCount(badHandle) == invalidHandle);
        Require(sceAudioInGetGain(badHandle, nullptr) == invalidPointer);
        Require(sceAudioInGetGain(badHandle, reinterpret_cast<float*>(static_cast<std::uintptr_t>(0x10))) == invalidPointer);
        Require(sceAudioInClose(badHandle) == invalidHandle);
        Require(sceAudioInInput(badHandle, nullptr) == invalidHandle);
        Require(sceAudioInGetSilentState(badHandle) == invalidHandle);
    }
    int port = sceAudioInAsyncOpen(user, 0, 0, 128, 48000, 2);
    float gain = 0.0f;
    Require(port > 0 && sceAudioInGetGain(port, &gain) == 0 && gain == 1.0f && sceAudioInGetRerouteCount(port) == 0);
    Require(sceAudioInGetGain(port, nullptr) == invalidPointer);
    Require(sceAudioInGetGain(port, reinterpret_cast<float*>(static_cast<std::uintptr_t>(0x10))) == invalidPointer);
    Require(sceAudioInClose(port) == 0);
    gain = 0.25f;
    Require(sceAudioInGetGain(port, &gain) == invalidHandle && gain == 1.0f && sceAudioInGetRerouteCount(port) == invalidHandle);
    port = sceAudioInOpen(user, 0, 0, 256, 48000, 0x10);
    gain = -1.0f;
    Require(port > 0 && sceAudioInGetGain(port, &gain) == 0 && gain == 1.0f && sceAudioInGetRerouteCount(port) == 0 && sceAudioInClose(port) == 0);
    for (const std::uint32_t format : {0u, 1u, 3u, 0x10u, 0x11u, 0x12u}) Require(sceAudioInHqOpen(user, 0, 0, 128, 48000, format) == invalidParam);
    for (const std::uint32_t size : {0u, 127u, 129u, 256u, 384u}) Require(sceAudioInAsyncOpen(user, 0, 0, size, 48000, 2) == invalidSize);
    Require(sceAudioInHqOpen(user, 2, 1, 256, 16000, 0) == invalidFreq);
    Require(sceAudioInAsyncOpen(user, 2, 1, 256, 16000, 0) == invalidFreq);
    Require(sceAudioInHqOpen(user, 2, 1, 256, 48000, 0) == invalidSize);
    Require(sceAudioInAsyncOpen(user, 2, 1, 256, 48000, 0) == invalidSize);
    port = sceAudioInAsyncOpen(user, 0, 0, 128, 48000, 0x10);
    gain = 0.5f;
    Require(port > 0 && sceAudioInGetGain(port, &gain) == 0 && gain == 1.0f && sceAudioInGetRerouteCount(port) == 0 && sceAudioInClose(port) == 0);
    Require(sceAudioInIsPadEmbeddedMic(1) == static_cast<int32_t>(0x80260101));
    const int mic = sceAudioInOpen(user, 0, 0, 256, 48000, 0x10);
    Require(mic > 0 && sceAudioInIsPadEmbeddedMic(mic) == 0 && sceAudioInClose(mic) == 0);
    Require(sceAudioInIsPadEmbeddedMic(mic) == static_cast<int32_t>(0x80260101));
}

void InputSound(int handle, std::vector<std::int16_t>& block, int samples = 256) {
    for (int attempt = 0; attempt < 200; ++attempt) {
        Require(sceAudioInInput(handle, block.data()) == samples);
        if (std::any_of(block.begin(), block.end() - 1, [](std::int16_t sample) { return sample != 0; })) return;
    }
    Require(false);
}

void TestQueuedAudioDiscard() {
    std::array<std::uint8_t, 4096> discard{};
    std::size_t calls = 0;
    audioin::detail::discardQueuedAudio(8192, discard.data(), discard.size(), [&calls](std::uint8_t*, std::uint32_t chunk) {
        ++calls;
        Require(chunk == 4096);
        return std::uint32_t{0};
    });
    Require(calls == 1);
    calls = 0;
    audioin::detail::discardQueuedAudio(8192, discard.data(), discard.size(), [&calls](std::uint8_t*, std::uint32_t) {
        ++calls;
        return std::uint32_t{1024};
    });
    Require(calls == 8);
    calls = 0;
    audioin::detail::discardQueuedAudio(1024, discard.data(), discard.size(), [&calls](std::uint8_t*, std::uint32_t) {
        ++calls;
        return std::uint32_t{2048};
    });
    Require(calls == 1);
}

void TestCapture() {
    TestQueuedAudioDiscard();
    const auto path = std::filesystem::temp_directory_path() / ("anyps5_audio_in_capture-" + std::to_string(std::random_device{}()) + ".raw");
    std::vector<std::int16_t> recorded(256 * 2 * 4);
    for (std::size_t i = 0; i < recorded.size(); ++i) recorded[i] = static_cast<std::int16_t>(i * 7 - 3000);
    std::ofstream(path, std::ios::binary).write(reinterpret_cast<const char*>(recorded.data()), static_cast<std::streamsize>(recorded.size() * 2));
    SetEnvironment("SDL_AUDIODRIVER", "disk");
    SetEnvironment("SDL_DISKAUDIOFILEIN", path.string());

    TestValidation();
    TestOpenEx();
    TestMicrophoneMuteBounds();
    TestStreamFormatRejection();
    TestSamplingRateValidation();
    const int handle = sceAudioInOpen(user, 0, 0, 256, 48000, 2);
    Require(handle > 0 && sceAudioInGetSilentState(handle) == 0);
    std::vector<std::int16_t> block(256 * 2 + 1, 0x5555);
    std::vector<std::int16_t> captured;
    for (int i = 0; i < 4; ++i) {
        InputSound(handle, block);
        captured.insert(captured.end(), block.begin(), block.end() - 1);
        Require(block.back() == 0x5555);
    }
    Require(captured == recorded);
    Require(sceAudioInInput(handle, block.data()) == 256);
    for (std::size_t i = 0; i < 256 * 2; ++i) Require(block[i] == 0);
    Require(sceAudioInClose(handle) == 0);
    const int async = sceAudioInAsyncOpen(user, 0, 0, 128, 48000, 2);
    Require(async == handle && sceAudioInGetSilentState(async) == 0);
    block.assign(128 * 2 + 1, 0x5555);
    InputSound(async, block, 128);
    Require(std::equal(block.begin(), block.end() - 1, recorded.begin()) && block.back() == 0x5555);
    Require(sceAudioInClose(async) == 0);
    std::filesystem::remove(path);
}

void TestNoDevice() {
    constexpr int invalidHandle = static_cast<int>(0x80260101);
    constexpr int portFull = static_cast<int>(0x80260107);
    SetEnvironment("SDL_AUDIODRIVER", "none");

    TestValidation();
    TestOpenEx();
    TestMicrophoneMuteBounds();
    TestStreamFormatRejection();
    TestSamplingRateValidation();
    const int handle = sceAudioInOpen(user, 0, 0, 256, 16000, 0x12);
    Require(handle > 0 && sceAudioInGetSilentState(handle) == 1);
    Require(sceAudioInGetSilentState(handle + 1) == invalidHandle);
    Require(sceAudioInInput(handle + 1, nullptr) == invalidHandle);
    std::vector<std::uint8_t> buffer(256 * 8 + 1, 0xCC);
    const auto start = std::chrono::steady_clock::now();
    for (int block = 0; block < 5; ++block) Require(sceAudioInInput(handle, buffer.data()) == 256);
    const auto elapsed = std::chrono::steady_clock::now() - start;
    Require(elapsed >= std::chrono::milliseconds(4 * 16) && elapsed < std::chrono::milliseconds(1000));
    for (std::size_t i = 0; i < 256 * 8; ++i) Require(buffer[i] == 0);
    Require(buffer[256 * 8] == 0xCC);
    Require(sceAudioInInput(handle, nullptr) == 0);
    const int hq = sceAudioInHqOpen(user, 0, 0, 128, 48000, 2);
    const int async = sceAudioInAsyncOpen(user, 1, 0, 128, 48000, 0x11);
    Require(hq > 0 && async > 0 && hq != handle && async != handle && hq != async);
    Require(sceAudioInGetSilentState(hq) == 1 && sceAudioInGetSilentState(async) == 1);
    std::fill(buffer.begin(), buffer.end(), 0xCC);
    Require(sceAudioInInput(hq, buffer.data()) == 128);
    for (std::size_t i = 0; i < 128 * 4; ++i) Require(buffer[i] == 0);
    Require(buffer[128 * 4] == 0xCC);
    std::fill(buffer.begin(), buffer.end(), 0xCC);
    Require(sceAudioInInput(async, buffer.data()) == 128);
    for (std::size_t i = 0; i < 128 * 4; ++i) Require(buffer[i] == 0);
    Require(buffer[128 * 4] == 0xCC);
    Require(sceAudioInInput(async, nullptr) == 0);
    for (int port = 3; port < 8; ++port) Require(sceAudioInOpen(user, 1, 0, 128, 48000, 1) > 0);
    Require(sceAudioInOpen(user, 1, 0, 128, 48000, 1) == portFull);
    Require(sceAudioInHqOpen(user, 1, 0, 128, 48000, 2) == portFull);
    Require(sceAudioInAsyncOpen(user, 1, 0, 128, 48000, 1) == portFull);
    Require(sceAudioInClose(hq) == 0);
    Require(sceAudioInClose(hq) == invalidHandle);
    Require(sceAudioInGetSilentState(hq) == invalidHandle);
    Require(sceAudioInAsyncOpen(user, 0, 0, 128, 48000, 2) == hq);
    Require(sceAudioInClose(async) == 0);
    Require(sceAudioInInput(async, buffer.data()) == invalidHandle);
    Require(sceAudioInHqOpen(user, 1, 0, 128, 48000, 2) == async);
    Require(sceAudioInClose(handle) == 0);
    Require(sceAudioInClose(handle) == invalidHandle);
    Require(sceAudioInInput(handle, buffer.data()) == invalidHandle);
    Require(sceAudioInOpen(user, 1, 0, 128, 48000, 0x11) == handle);
}

}

int main(int argc, char** argv) {
    Require(argc == 2);
    Require(sceAudioInInit() == 0);
    if (std::strcmp(argv[1], "capture") == 0) TestCapture();
    else TestNoDevice();
}
