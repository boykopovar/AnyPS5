#include "prx/libc/include/general/VabiMacros.hpp"

#include <Testing/Test.hpp>

#include <cstdint>

extern "C" {
std::uint32_t APS5_VABI sceRazorCpuIsCapturing(void);
int APS5_VABI sceRazorCpuJobManagerDispatch(const void* args);
int APS5_VABI sceRazorCpuJobManagerJob(const void* args);
int APS5_VABI sceRazorCpuJobManagerSequence(const void* args);
int APS5_VABI sceRazorCpuPushMarkerStatic(const char* name, std::uint32_t color, std::uint32_t flags);
int APS5_VABI sceRazorCpuPopMarker(void);
int APS5_VABI sceRazorCpuFlushOccurred(std::uint64_t* timeSpentInFlush);
int APS5_VABI sceRazorCpuPlotValue(const char* series, float value);
int APS5_VABI sceRazorCpuWriteBookmark(const char* label, const char* description);
}

namespace {

using Testing::Case;
using Testing::RequireEqual;

constexpr std::uint32_t markerColor = 0x80ffffffu;

const Case notCapturing{"IsCapturing_NoCapture_ReturnsZero", [] {
    RequireEqual(sceRazorCpuIsCapturing(), 0u, "capturing state");
}};

const Case jobManagerHooks{"JobManagerHooks_NullArgs_Succeed", [] {
    RequireEqual(sceRazorCpuJobManagerDispatch(nullptr), 0, "dispatch");
    RequireEqual(sceRazorCpuJobManagerJob(nullptr), 0, "job");
    RequireEqual(sceRazorCpuJobManagerSequence(nullptr), 0, "sequence");
}};

const Case nestedMarkers{"PushMarkerStatic_NestedMarkers_PushAndPopSucceed", [] {
    RequireEqual(sceRazorCpuPushMarkerStatic("outer", markerColor, 2), 0, "push outer");
    RequireEqual(sceRazorCpuPushMarkerStatic("inner", markerColor, 2), 0, "push inner");
    RequireEqual(sceRazorCpuPopMarker(), 0, "pop inner");
    RequireEqual(sceRazorCpuPopMarker(), 0, "pop outer");
}};

const Case unbalancedMarker{"PushMarkerStatic_UnbalancedMarker_SucceedsWithoutStartingCapture", [] {
    RequireEqual(sceRazorCpuPushMarkerStatic("unbalanced", markerColor, 2), 0, "push unbalanced");
    RequireEqual(sceRazorCpuIsCapturing(), 0u, "capturing state");
}};

const Case flushTime{"FlushOccurred_OutputPointer_ReportsZeroFlushTime", [] {
    std::uint64_t timeSpentInFlush = 0x123456789abcdef0ull;
    RequireEqual(sceRazorCpuFlushOccurred(&timeSpentInFlush), 0, "flush occurred");
    RequireEqual(timeSpentInFlush, std::uint64_t{0}, "time spent in flush");
}};

const Case flushNull{"FlushOccurred_NullPointer_Succeeds", [] {
    RequireEqual(sceRazorCpuFlushOccurred(nullptr), 0, "flush occurred without output");
}};

const Case plotValue{"PlotValue_NamedSeries_Succeeds", [] {
    RequireEqual(sceRazorCpuPlotValue("read time (ms)", 1.5f), 0, "plot value");
}};

const Case bookmarks{"WriteBookmark_WithAndWithoutDescription_Succeeds", [] {
    RequireEqual(sceRazorCpuWriteBookmark("read timeout", "lba=0x10"), 0, "bookmark with description");
    RequireEqual(sceRazorCpuWriteBookmark("read timeout", nullptr), 0, "bookmark without description");
}};

} // namespace
