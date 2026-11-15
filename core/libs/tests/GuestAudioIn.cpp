#include "prx/libSceAudioIn/include/AudioIn.hpp"
#include "prx/libSceAudioIn/AudioInCapture.hpp"
#include "prx/libSceAudioIn/AudioInDeviceSelection.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <algorithm>
#include <array>
#include <cerrno>
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

static void SetEnvironment(const char* name, const std::string& value) {
#ifdef _WIN32
    _putenv_s(name, value.c_str());
#else
    setenv(name, value.c_str(), 1);
#endif
}

namespace {

constexpr int user = 0x10000000;

void TestCaptureDeviceSelection() {
    using audioin::detail::CaptureDeviceMatchKind;
    const std::array<const char*, 3> names{"Microphone A", nullptr, "Microphone B"};
    auto getName = [&names](int index) { return names[static_cast<std::size_t>(index)]; };
    Require(audioin::detail::selectCaptureDevice("", 3, getName).kind == CaptureDeviceMatchKind::Default);
    Require(audioin::detail::selectCaptureDevice("default", 3, getName).kind == CaptureDeviceMatchKind::Default);
    const auto found = audioin::detail::selectCaptureDevice("Microphone B", 3, getName);
    Require(found.kind == CaptureDeviceMatchKind::Found && found.index == 2);
    Require(audioin::detail::selectCaptureDevice("Missing", 3, getName).kind == CaptureDeviceMatchKind::Missing);
    const auto ambiguousNames = [](int index) { return index < 2 ? "USB microphone" : "Other"; };
    Require(audioin::detail::selectCaptureDevice("USB microphone", 3, ambiguousNames).kind == CaptureDeviceMatchKind::Ambiguous);
    Require(audioin::detail::selectCaptureDevice("Microphone A", -1, getName).kind == CaptureDeviceMatchKind::Missing);
}

void TestConfiguredDeviceUnavailable() {
    constexpr int invalidDevice = static_cast<int>(0x8026010f);
    SetEnvironment("SDL_AUDIODRIVER", "none");
    SetEnvironment("ANYPS5_AUDIO_IN_DEVICE", "AnyPS5 missing capture device 7fd5622b");
    Require(sceAudioInOpen(user, 0, 0, 256, 48000, 2) == invalidDevice);
    SetEnvironment("ANYPS5_AUDIO_IN_DEVICE", "");
}

void TestOpenEx() {
    constexpr int invalidSize = static_cast<int>(0x80260102);
    constexpr int invalidFreq = static_cast<int>(0x80260103);
    constexpr int invalidType = static_cast<int>(0x80260104);
    Require(sceAudioInOpenEx(user, 6, 0, 7, 256, 48000, 2) == invalidType);
    Require(sceAudioInOpenEx(user, 0, 0, 7, 512, 48000, 2) == invalidSize);
    Require(sceAudioInOpenEx(user, 0, 0, 7, 256, 44100, 2) == invalidFreq);
    Require(sceAudioInHqOpenEx(user, 0, 0, 7, 128, 44100, 2) == invalidFreq);
    int port = sceAudioInOpenEx(user, 0, 0, 7, 256, 48000, 2);
    Require(port > 0 && sceAudioInClose(port) == 0);
    port = sceAudioInHqOpenEx(user, 0, 0, 7, 256, 16000, 2);
    Require(port > 0 && sceAudioInClose(port) == 0);
    Require(sceAudioInSetCompressorPreGain() == 0 && sceAudioInSetNoiseGateThreshold() == 0);
}

void TestValidation() {
    constexpr int invalidHandle = static_cast<int>(0x80260101);
    constexpr int invalidSize = static_cast<int>(0x80260102);
    constexpr int invalidFreq = static_cast<int>(0x80260103);
    constexpr int invalidType = static_cast<int>(0x80260104);
    constexpr int invalidPointer = static_cast<int>(0x80260105);
    constexpr int invalidParam = static_cast<int>(0x80260106);
    constexpr int invalidState = static_cast<int>(0x80260109);
    Require(sceAudioInOpen(user, 6, 0, 256, 48000, 2) == invalidType);
    Require(sceAudioInOpen(user, 0, 1, 256, 48000, 2) == invalidParam);
    Require(sceAudioInOpen(user, 0, 0, 512, 48000, 2) == invalidSize);
    Require(sceAudioInOpen(user, 0, 0, 256, 44100, 2) == invalidFreq);
    Require(sceAudioInOpen(user, 0, 0, 256, 48000, 3) == invalidParam);
    Require(sceAudioInOpen(user, 0, 0, 256, 48000, 0x20) == invalidParam);
    Require(sceAudioInHqOpen(user, 6, 0, 128, 48000, 2) == invalidType);
    Require(sceAudioInHqOpen(user, 0, 1, 128, 48000, 2) == invalidParam);
    Require(sceAudioInHqOpen(user, 0, 0, 512, 48000, 2) == invalidSize);
    Require(sceAudioInHqOpen(user, 0, 0, 128, 44100, 2) == invalidFreq);
    Require(sceAudioInHqOpen(user, 0, 0, 128, 48000, 3) == invalidParam);
    Require(sceAudioInAsyncOpen(user, 6, 0, 128, 48000, 2) == invalidType);
    Require(sceAudioInAsyncOpen(user, 0, 1, 128, 48000, 2) == invalidParam);
    Require(sceAudioInAsyncOpen(user, 0, 0, 385, 48000, 2) == invalidSize);
    Require(sceAudioInAsyncOpen(user, 0, 0, 200, 48000, 2) == invalidSize);
    Require(sceAudioInAsyncOpen(user, 0, 0, 256, 44100, 2) == invalidFreq);
    Require(sceAudioInAsyncOpen(user, 0, 0, 256, 48000, 0) == invalidSize);
    Require(sceAudioInHqOpen(user, 0, 0, 256, 48000, 2) == invalidSize);
    Require(sceAudioInHqOpen(user, 0, 0, 128, 16000, 2) == invalidFreq);
    for (const int badHandle : {-1, 0, 1, 9}) {
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
    Require(sceAudioInGetGain(port, &gain) == invalidState && gain == 1.0f && sceAudioInGetRerouteCount(port) == invalidState);
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
    Require(sceAudioInIsPadEmbeddedMic(mic) == static_cast<int32_t>(0x80260109));
}

void TestFirmwareSignatures() {
    constexpr int invalidHandle = static_cast<int>(0x80260101);
    constexpr int invalidSize = static_cast<int>(0x80260102);
    constexpr int invalidFreq = static_cast<int>(0x80260103);
    constexpr int invalidType = static_cast<int>(0x80260104);
    constexpr int invalidParam = static_cast<int>(0x80260106);
    constexpr int invalidState = static_cast<int>(0x80260109);
    constexpr int invalidDevice = static_cast<int>(0x8026010f);
    constexpr std::int32_t handleBase = 0x30000000;

    Require(sceAudioInDeviceIdOpen(user, 6, 0, 256, 44100, 3) == invalidType);
    Require(sceAudioInDeviceIdOpen(user, 0, 0, 256, 44100, 3) == invalidFreq);
    Require(sceAudioInDeviceIdOpen(user, 0, 0, 512, 48000, 3) == invalidSize);
    Require(sceAudioInDeviceIdOpen(user, 0, 0, 256, 48000, 3) == invalidParam);
    Require(sceAudioInDeviceIdOpen(user, 0, 0, 256, 48000, 0x20) == invalidParam);
    const int did = sceAudioInDeviceIdOpen(user, 5, 0x1f, 256, 48000, 2);
    Require(did >= handleBase && (did & 0x7f000000) == handleBase && sceAudioInClose(did) == 0);
    const int asyncId = sceAudioInDeviceIdAsyncOpen(user, 0, 7, 128, 16000, 0);
    Require(asyncId >= handleBase && sceAudioInClose(asyncId) == 0);
    const int hqId = sceAudioInDeviceIdHqOpen(user, 0, 3, 256, 16000, 0x12);
    Require(hqId >= handleBase && sceAudioInClose(hqId) == 0);
    const int deviceEx = sceAudioInDeviceOpenEx(user, 4, 0xa, 9, 128, 48000, 0x11);
    Require(deviceEx >= handleBase && sceAudioInClose(deviceEx) == 0);
    Require(sceAudioInDeviceHqOpen(user, 0, 512, 48000, 2) == invalidSize);
    const int deviceHq = sceAudioInDeviceHqOpen(user, 0, 128, 48000, 2);
    Require(deviceHq >= handleBase && sceAudioInClose(deviceHq) == 0);

    Require(sceAudioInIsSharedDevice(8) == invalidHandle);
    Require(sceAudioInIsSharedDevice(-1) == invalidHandle);
    Require(sceAudioInIsSharedDevice(0) == invalidState);
    const int shared = sceAudioInOpen(user, 0, 0, 256, 48000, 2);
    Require(shared >= handleBase);
    Require(sceAudioInGetHandleStatusInfo(0, nullptr) == invalidHandle);
    Require(sceAudioInGetHandleStatusInfo(shared + 1, nullptr) == invalidHandle);
    Require(sceAudioInClose(shared) == 0);
    Require(sceAudioInGetHandleStatusInfo(shared, nullptr) == invalidState);

    Require(sceAudioInSetMicLevel(1, 0, 101, 0, 0) == invalidParam);
    Require(sceAudioInSetMicLevel(9, 0, 101, 0, 0) == invalidParam);
    Require(sceAudioInSetMicLevel(5, 0, 0, 0, 0) == invalidDevice);
    Require(sceAudioInSetMicLevel(0, 0, 0, 0, 0) == invalidDevice);
    Require(sceAudioInSetDeviceMuteStatus(5, 0, 1) == invalidDevice);
    Require(sceAudioInSetDeviceMuteStatus(8, 0, 1) == invalidDevice);
    Require(sceAudioInSetDeviceMuteStatus(-1, 0, 1) == invalidDevice);
    Require(sceAudioInSetUserMute(5, 0, 0) == invalidDevice);

    Require(sceAudioInVmicCreate(user, 512, 44100, 0) == invalidSize);
    Require(sceAudioInVmicCreate(user, 256, 44100, 0) == invalidFreq);
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
    TestFirmwareSignatures();
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
    constexpr int invalidPointer = static_cast<int>(0x80260105);
    constexpr int invalidState = static_cast<int>(0x80260109);
    constexpr int portFull = static_cast<int>(0x80260107);
    SetEnvironment("SDL_AUDIODRIVER", "none");
    TestConfiguredDeviceUnavailable();
    TestCaptureDeviceSelection();
    TestValidation();
    TestOpenEx();
    TestFirmwareSignatures();
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
    errno = EBUSY;
    Require(sceAudioInInput(handle, reinterpret_cast<void*>(static_cast<std::uintptr_t>(0x10))) == invalidPointer && errno == EBUSY);
    Require(sceAudioInInput(handle, buffer.data()) == 256);
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
    Require(sceAudioInClose(hq) == invalidState);
    Require(sceAudioInGetSilentState(hq) == invalidState);
    Require(sceAudioInAsyncOpen(user, 0, 0, 128, 48000, 2) == hq);
    Require(sceAudioInClose(async) == 0);
    Require(sceAudioInInput(async, buffer.data()) == invalidState);
    Require(sceAudioInHqOpen(user, 1, 0, 128, 48000, 2) == async);
    Require(sceAudioInClose(handle) == 0);
    Require(sceAudioInClose(handle) == invalidState);
    Require(sceAudioInInput(handle, buffer.data()) == invalidState);
    Require(sceAudioInOpen(user, 1, 0, 128, 48000, 0x11) == handle);
}

}

int main(int argc, char** argv) {
    Require(argc == 2);
    Require(sceAudioInInit() == 0);
    if (std::strcmp(argv[1], "capture") == 0) TestCapture();
    else if (std::strcmp(argv[1], "selection") == 0) {
        TestConfiguredDeviceUnavailable();
        TestCaptureDeviceSelection();
    }
    else TestNoDevice();
}
