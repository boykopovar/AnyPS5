#include "prx/libSceVideoRecordingP/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>

namespace {

constexpr int StatusNone = 0;
constexpr std::uint8_t Sentinel = 0xA5;

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "VideoRecording: %s\n", message);
        std::abort();
    }
}

template <typename TCall>
bool Throws(TCall call) {
    try {
        call();
    } catch (const std::exception&) {
        return true;
    }
    return false;
}

}

int main() {
    Require(sceVideoRecordingGetStatus() == StatusNone, "a recorder that was never opened reported a status");
    Require(sceVideoRecordingGetStatus() == StatusNone, "the status changed between two reads");

    std::array<std::uint8_t, 24> param{};
    param.fill(Sentinel);
    std::array<std::uint8_t, 64> heap{};
    heap.fill(Sentinel);

    Require(Throws([&] { _sceVideoRecordingQueryParam_nid_postfix(reinterpret_cast<VideoRecordingQueryParam*>(param.data())); }), "the default parameters were produced");
    Require(Throws([&] { sceVideoRecordingQueryMemSize(reinterpret_cast<const VideoRecordingParam*>(param.data())); }), "a heap size was reported");
    Require(Throws([&] { sceVideoRecordingQueryMemSize(nullptr); }), "a heap size was reported for no parameters");
    Require(Throws([&] { sceVideoRecordingOpen("clip.mp4", reinterpret_cast<const VideoRecordingParam*>(param.data()), heap.data(), static_cast<int>(heap.size())); }), "the recorder opened");
    Require(Throws([&] { sceVideoRecordingStart(); }), "the encoder started");
    Require(Throws([&] { sceVideoRecordingStop(); }), "the encoder stopped");
    Require(Throws([&] { sceVideoRecordingClose(0); }), "the recorder closed");
    Require(Throws([&] { sceVideoRecordingClose(1); }), "the recorder closed with discard");

    for (const std::uint8_t value : param) Require(value == Sentinel, "the parameters were written");
    for (const std::uint8_t value : heap) Require(value == Sentinel, "the heap was written");
    Require(sceVideoRecordingGetStatus() == StatusNone, "a failed open changed the status");

    Require(sceVideoRecordingGetInfo(0xa001, nullptr, 0) == static_cast<int>(0x80a80006), "info 0xa001 validation");
    Require(sceVideoRecordingGetInfo(0xa005, nullptr, 0) == static_cast<int>(0x80a80006), "info 0xa005 validation");
    Require(sceVideoRecordingGetInfo(0, nullptr, 0) == static_cast<int>(0x80a80003), "unknown info validation");
    std::puts("VideoRecording tests passed");
    return 0;
}
