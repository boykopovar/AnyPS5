#include "prx/libSceRazorCpu/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace {

void Require(bool value) { if (!value) std::abort(); }

int g_markerCallbacks = 0;

void APS5_VABI CountMarkerCallback() {
    ++g_markerCallbacks;
}

}

int main() {
    Require(sceRazorCpuIsCapturing() == 0);
    Require(sceRazorCpuJobManagerDispatch(nullptr, 1, 2, 3, 4, 5, 6) == 0);
    Require(sceRazorCpuJobManagerDispatch("job", 0, 0, 0, 0, 0, 0) == 0);
    Require(sceRazorCpuJobManagerJob(1, 2, 3, 16) == 0);
    Require(sceRazorCpuJobManagerJob(0, 0, 0, 0) == 0);
    Require(sceRazorCpuJobManagerSequence(nullptr, 1, 2, 3) == 0);
    Require(sceRazorCpuJobManagerSequence("sequence", 0, 0, 0) == 0);
    Require(sceRazorCpuNamedSync("frame") == 0);
    Require(sceRazorCpuNamedSync(nullptr) == 0);
    Require(sceRazorCpuPushMarkerStatic("outer", 0x80ffffffu, 2) == 0);
    Require(sceRazorCpuPushMarkerStatic("inner", 0x80ffffffu, 2) == 0);
    Require(sceRazorCpuPushMarkerStatic(nullptr, 0, 0) == 0);
    Require(sceRazorCpuPopMarker() == 0);
    Require(sceRazorCpuPopMarker() == 0);
    Require(sceRazorCpuPushMarker("copied", 0xff00ff00u, 1) == 0);
    Require(sceRazorCpuPushMarker(nullptr, 0, 0) == 0);
    Require(sceRazorCpuPopMarker() == 0);
    Require(sceRazorCpuPushMarkerStatic("unbalanced", 0x80ffffffu, 2) == 0);
    Require(sceRazorCpuIsCapturing() == 0);
    std::uint64_t timeSpentInFlush = 0x123456789abcdef0ull;
    Require(sceRazorCpuFlushOccurred(&timeSpentInFlush) == 0);
    Require(timeSpentInFlush == 0x123456789abcdef0ull);
    Require(sceRazorCpuFlushOccurred(nullptr) == 0);
    Require(sceRazorCpuPlotValue("read time (ms)", 1.5f) == 0);
    Require(sceRazorCpuPlotValue(nullptr, 0.0f) == 0);
    Require(sceRazorCpuPlotValue("read time (ms)", -1.0f) == 0);
    Require(sceRazorCpuWriteBookmark("read timeout", "lba=0x10") == 0);
    Require(sceRazorCpuWriteBookmark("read timeout", nullptr) == 0);
    Require(sceRazorCpuWriteBookmark(nullptr, nullptr) == 0);
    Require(sceRazorCpuWriteBookmark(nullptr, "description") == 0);
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
    Require(sceRazorCpuBeginLogicalFileAccess(nullptr, 0, 0, 0) == 0);
    Require(sceRazorCpuEndLogicalFileAccess() == 0);
    Require(sceRazorCpuEndLogicalFileAccess() == 0);

    unsigned char storage[256];
    std::memset(storage, 0x5a, sizeof(storage));
    int elements[16]{};
    unsigned char buffer[64]{};
    Require(sceRazorCpuInitDataTags(storage, sizeof(storage)) == 0);
    Require(sceRazorCpuInitDataTags(nullptr, 0) == 0);
    Require(sceRazorCpuTagArray("elements", 7, elements, sizeof(elements), sizeof(elements[0])) == 0);
    Require(sceRazorCpuTagArray(nullptr, 0, nullptr, 0, 0) == 0);
    Require(sceRazorCpuTagBuffer("buffer", 7, buffer, sizeof(buffer)) == 0);
    Require(sceRazorCpuTagBuffer(nullptr, 0, nullptr, 0) == 0);
    Require(sceRazorCpuResizeTaggedBuffer(7, buffer, sizeof(buffer) / 2) == 0);
    Require(sceRazorCpuResizeTaggedBuffer(0, nullptr, 0) == 0);
    Require(sceRazorCpuUnTagBuffer(7, buffer) == 0);
    Require(sceRazorCpuUnTagBuffer(7, elements) == 0);
    Require(sceRazorCpuUnTagBuffer(0, nullptr) == 0);
    Require(sceRazorCpuShutdownDataTags() == 0);
    for (const unsigned char byte : storage) Require(byte == 0x5a);
    Require(sceRazorCpuTagBuffer("untracked", 3, buffer, sizeof(buffer)) == 0);
    Require(sceRazorCpuUnTagBuffer(3, buffer) == 0);
    Require(sceRazorCpuIsCapturing() == 0);

    Require(sceRazorCpuGetDataTagStorageSize(0) == 0);
    Require(sceRazorCpuGetDataTagStorageSize(16) == 0);
    Require(sceRazorCpuGetDataTagStorageSize(0xffffffffu) == 0);

    const auto callback = reinterpret_cast<void*>(&CountMarkerCallback);
    sceRazorCpuSetPushMarkerCallback(callback);
    sceRazorCpuSetPushMarkerStaticCallback(callback);
    sceRazorCpuSetPopMarkerCallback(callback);
    Require(sceRazorCpuPushMarker("callback", 0x80ffffffu, 0) == 0);
    Require(sceRazorCpuPushMarkerStatic("callback", 0x80ffffffu, 0) == 0);
    Require(sceRazorCpuPopMarker() == 0);
    Require(sceRazorCpuPopMarker() == 0);
    Require(g_markerCallbacks == 0);
    sceRazorCpuSetPushMarkerCallback(nullptr);
    sceRazorCpuSetPushMarkerStaticCallback(nullptr);
    sceRazorCpuSetPopMarkerCallback(nullptr);

    std::uint8_t context[16]{};
    Require(sceRazorCpuInitializeGpuMarkerContext(nullptr) == 22);
    Require(sceRazorCpuInitializeGpuMarkerContext(context) == 22);
    context[0] = 2;
    Require(sceRazorCpuInitializeGpuMarkerContext(context) == 22);
    context[0] = 1;
    context[1] = 1;
    Require(sceRazorCpuInitializeGpuMarkerContext(context) == 0);
    Require(context[0] == 1 && context[1] == 1);
    for (std::size_t index = 2; index < sizeof(context); ++index) Require(context[index] == 0);
    Require(sceRazorCpuGpuMarkerSync() == 0);
    Require(sceRazorCpuPushMarker("gpu", 0x80ffffffu, 0) == 0);
    Require(sceRazorCpuPopMarker() == 0);

    Require(sceRazorCpuFiberLogNameChange() == 0);
    Require(sceRazorCpuFiberSwitch() == 0);
    Require(sceRazorCpuWorkloadSubmit() == 0);
    Require(sceRazorCpuWorkloadRunBegin() == 0);
    Require(sceRazorCpuWorkloadRunEnd() == 0);
    Require(sceRazorCpuStartCaptureInternal() == static_cast<int>(0x80580000));
    Require(sceRazorCpuStopCaptureInternal() == static_cast<int>(0x80580000));
    Require(sceRazorCpuIsCapturing() == 0);
}
