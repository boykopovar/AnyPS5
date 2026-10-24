#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

extern "C" {
std::uint32_t APS5_VABI sceRazorCpuIsCapturing(void);
int APS5_VABI sceRazorCpuJobManagerDispatch(const char* label, std::uint16_t firstId, std::uint16_t secondId, std::uint16_t firstCount, std::uint16_t secondCount, std::uint64_t firstValue, std::uint64_t secondValue);
int APS5_VABI sceRazorCpuJobManagerJob(std::uint16_t firstId, std::uint16_t secondId, std::uint16_t thirdId, std::uint16_t event);
int APS5_VABI sceRazorCpuJobManagerSequence(const char* label, std::uint16_t firstId, std::uint16_t secondId, std::uint16_t event);
int APS5_VABI sceRazorCpuPushMarker(const char* label, std::uint32_t color, std::uint32_t flags);
int APS5_VABI sceRazorCpuNamedSync(const char* label);
int APS5_VABI sceRazorCpuPushMarkerStatic(const char* name, std::uint32_t color, std::uint32_t flags);
int APS5_VABI sceRazorCpuPopMarker(void);
bool APS5_VABI sceRazorCpuFlushOccurred(std::uint64_t* timeSpentInFlush);
int APS5_VABI sceRazorCpuPlotValue(const char* series, float value);
int APS5_VABI sceRazorCpuWriteBookmark(const char* label, const char* description);
int APS5_VABI sceRazorCpuSync(void);
void APS5_VABI sceRazorCpuDisableFiberUserMarkers(void);
int APS5_VABI sceRazorCpuInitDataTags(void* buffer, std::size_t size);
int APS5_VABI sceRazorCpuShutdownDataTags(void);
std::size_t APS5_VABI sceRazorCpuGetDataTagStorageSize(std::uint32_t tagCount);
int APS5_VABI sceRazorCpuTagArray(const char* label, std::uint32_t category, void* array, std::size_t bufferSize, std::size_t elementSize);
int APS5_VABI sceRazorCpuTagBuffer(const char* label, std::uint32_t category, void* buffer, std::size_t bufferSize);
int APS5_VABI sceRazorCpuResizeTaggedBuffer(std::uint32_t category, void* buffer, std::size_t newBufferSize);
int APS5_VABI sceRazorCpuUnTagBuffer(std::uint32_t category, void* buffer);
int APS5_VABI sceRazorCpuBeginLogicalFileAccess(const char* filename, std::uint64_t tag, std::uint64_t size, std::uint8_t operation);
int APS5_VABI sceRazorCpuEndLogicalFileAccess(void);
}

namespace {

void Require(bool value) { if (!value) std::abort(); }

}

int main() {
    Require(sceRazorCpuIsCapturing() == 0);
    Require(sceRazorCpuJobManagerDispatch(nullptr, 1, 2, 3, 4, 5, 6) == 0);
    Require(sceRazorCpuJobManagerJob(1, 2, 3, 16) == 0);
    Require(sceRazorCpuJobManagerSequence(nullptr, 1, 2, 3) == 0);
    Require(sceRazorCpuNamedSync("frame") == 0);
    Require(sceRazorCpuPushMarkerStatic("outer", 0x80ffffffu, 2) == 0);
    Require(sceRazorCpuPushMarkerStatic("inner", 0x80ffffffu, 2) == 0);
    Require(sceRazorCpuPopMarker() == 0);
    Require(sceRazorCpuPopMarker() == 0);
    Require(sceRazorCpuPushMarker("copied", 0xff00ff00u, 1) == 0);
    Require(sceRazorCpuPopMarker() == 0);
    Require(sceRazorCpuPushMarkerStatic("unbalanced", 0x80ffffffu, 2) == 0);
    Require(sceRazorCpuIsCapturing() == 0);
    std::uint64_t timeSpentInFlush = 0x123456789abcdef0ull;
    Require(sceRazorCpuFlushOccurred(&timeSpentInFlush) == 0);
    Require(timeSpentInFlush == 0x123456789abcdef0ull);
    Require(sceRazorCpuFlushOccurred(nullptr) == 0);
    Require(sceRazorCpuPlotValue("read time (ms)", 1.5f) == 0);
    Require(sceRazorCpuWriteBookmark("read timeout", "lba=0x10") == 0);
    Require(sceRazorCpuWriteBookmark("read timeout", nullptr) == 0);
    Require(sceRazorCpuSync() == 0);
    Require(sceRazorCpuSync() == 0);
    Require(sceRazorCpuIsCapturing() == 0);

    sceRazorCpuDisableFiberUserMarkers();
    sceRazorCpuDisableFiberUserMarkers();
    Require(sceRazorCpuPushMarker("thread", 0x80ffffffu, 0) == 0);
    Require(sceRazorCpuPopMarker() == 0);

    Require(sceRazorCpuBeginLogicalFileAccess("data/level.pak", 0x0123456789abcdefull, 4096, 0) == 0);
    Require(sceRazorCpuEndLogicalFileAccess() == 0);
    Require(sceRazorCpuBeginLogicalFileAccess("save/slot0.bin", 0, 512, 1) == 0);
    Require(sceRazorCpuEndLogicalFileAccess() == 0);
    Require(sceRazorCpuEndLogicalFileAccess() == 0);

    unsigned char storage[256];
    std::memset(storage, 0x5a, sizeof(storage));
    int elements[16]{};
    unsigned char buffer[64]{};
    Require(sceRazorCpuInitDataTags(storage, sizeof(storage)) == 0);
    Require(sceRazorCpuTagArray("elements", 7, elements, sizeof(elements), sizeof(elements[0])) == 0);
    Require(sceRazorCpuTagBuffer("buffer", 7, buffer, sizeof(buffer)) == 0);
    Require(sceRazorCpuResizeTaggedBuffer(7, buffer, sizeof(buffer) / 2) == 0);
    Require(sceRazorCpuUnTagBuffer(7, buffer) == 0);
    Require(sceRazorCpuUnTagBuffer(7, elements) == 0);
    Require(sceRazorCpuShutdownDataTags() == 0);
    for (const unsigned char byte : storage) Require(byte == 0x5a);
    Require(sceRazorCpuTagBuffer("untracked", 3, buffer, sizeof(buffer)) == 0);
    Require(sceRazorCpuUnTagBuffer(3, buffer) == 0);
    Require(sceRazorCpuIsCapturing() == 0);

    bool threw = false;
    try {
        sceRazorCpuGetDataTagStorageSize(16);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw);
}
