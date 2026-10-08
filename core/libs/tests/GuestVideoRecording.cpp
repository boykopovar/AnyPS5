#include "SceTypes.hpp"
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

extern "C" int APS5_VABI sceVideoRecordingQueryMemSize(const void* param);
extern "C" int APS5_VABI sceVideoRecordingOpen(const char* path, const void* param, void* heap, int heapSize);
extern "C" int APS5_VABI sceVideoRecordingGetStatus(void);
extern "C" int APS5_VABI sceVideoRecordingSetInfo(int32_t info, const void* data, size_t size);
extern "C" int APS5_VABI sceVideoRecordingGetInfo(int32_t info, void* data, size_t size);

namespace {

constexpr int32_t ERROR_UNSUPPORTED = static_cast<int32_t>(0x80A80008);
constexpr int32_t STATUS_NONE = 0;
constexpr int32_t INFO_SUBTITLE = 0x0002;

void Require(bool value) { if (!value) std::abort(); }

template <typename TAction>
bool Throws(TAction action) {
    try {
        action();
    } catch (const std::logic_error&) {
        return true;
    }
    return false;
}

}

int main() {
    std::array<unsigned char, 64> param{};
    std::array<unsigned char, 4096> heap{};
    const char* path = "/video/clip.mp4";

    Require(sceVideoRecordingGetStatus() == STATUS_NONE);
    Require(Throws([] { sceVideoRecordingQueryMemSize(nullptr); }));
    const int memSize = sceVideoRecordingQueryMemSize(param.data());
    Require(memSize == ERROR_UNSUPPORTED);

    Require(Throws([&] { sceVideoRecordingOpen(nullptr, param.data(), heap.data(), static_cast<int>(heap.size())); }));
    Require(Throws([&] { sceVideoRecordingOpen(path, nullptr, heap.data(), static_cast<int>(heap.size())); }));
    Require(sceVideoRecordingOpen(path, param.data(), heap.data(), static_cast<int>(heap.size())) == ERROR_UNSUPPORTED);
    Require(sceVideoRecordingOpen(path, param.data(), nullptr, memSize) == ERROR_UNSUPPORTED);
    Require(sceVideoRecordingGetStatus() == STATUS_NONE);

    const char subtitle[] = "clip";
    Require(sceVideoRecordingSetInfo(INFO_SUBTITLE, subtitle, sizeof(subtitle)) == 0);
    std::array<char, sizeof(subtitle)> read{};
    Require(sceVideoRecordingGetInfo(INFO_SUBTITLE, read.data(), read.size()) == 0);
    Require(std::memcmp(read.data(), subtitle, sizeof(subtitle)) == 0);
    Require(sceVideoRecordingGetStatus() == STATUS_NONE);
}
