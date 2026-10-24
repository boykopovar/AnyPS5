#include "prx/libc/include/general/VabiMacros.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>

extern "C" {
void APS5_VABI _sceVideoRecordingQueryParam_nid_postfix(void* param);
int APS5_VABI sceVideoRecordingGetStatus(void);
int APS5_VABI sceVideoRecordingQueryMemSize(const void* param);
int APS5_VABI sceVideoRecordingOpen(const char* path, const void* param, void* heap, int heapSize);
int APS5_VABI sceVideoRecordingStart(void);
int APS5_VABI sceVideoRecordingStop(void);
int APS5_VABI sceVideoRecordingClose(int discard);
}

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

    Require(Throws([&] { _sceVideoRecordingQueryParam_nid_postfix(param.data()); }), "the default parameters were produced");
    Require(Throws([&] { sceVideoRecordingQueryMemSize(param.data()); }), "a heap size was reported");
    Require(Throws([&] { sceVideoRecordingQueryMemSize(nullptr); }), "a heap size was reported for no parameters");
    Require(Throws([&] { sceVideoRecordingOpen("clip.mp4", param.data(), heap.data(), static_cast<int>(heap.size())); }), "the recorder opened");
    Require(Throws([&] { sceVideoRecordingStart(); }), "the encoder started");
    Require(Throws([&] { sceVideoRecordingStop(); }), "the encoder stopped");
    Require(Throws([&] { sceVideoRecordingClose(0); }), "the recorder closed");
    Require(Throws([&] { sceVideoRecordingClose(1); }), "the recorder closed with discard");

    for (const std::uint8_t value : param) Require(value == Sentinel, "the parameters were written");
    for (const std::uint8_t value : heap) Require(value == Sentinel, "the heap was written");
    Require(sceVideoRecordingGetStatus() == StatusNone, "a failed open changed the status");

    std::puts("VideoRecording tests passed");
    return 0;
}
