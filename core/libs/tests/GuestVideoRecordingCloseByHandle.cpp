#include "prx/libSceVideoRecordingP/include/Api.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {

constexpr int InvalidArgument = static_cast<int>(0x80a80003u);
constexpr int NotOpen = static_cast<int>(0x80a80006u);

void Require(bool value, const char* what) {
    if (value) return;
    std::fprintf(stderr, "FAIL: %s\n", what);
    std::abort();
}

}

int main() {
    std::array<std::uint8_t, 64> buffer{};
    auto* handle = reinterpret_cast<VideoRecordingContext*>(buffer.data());

    Require(sceVideoRecordingCloseByHandle(nullptr, 0) == InvalidArgument, "CloseByHandle with a null handle");
    Require(sceVideoRecordingCloseByHandle(handle, 0) == NotOpen, "CloseByHandle with a handle no recorder owns");
    Require(sceVideoRecordingCloseByHandle(handle, 1) == NotOpen, "CloseByHandle with a handle no recorder owns, discarding");

    Require(sceVideoRecordingCloseWithTruncationByHandle(handle, 0, 0, 4) == InvalidArgument, "truncation mode above 3");
    Require(sceVideoRecordingCloseWithTruncationByHandle(nullptr, 0, 0, 0) == InvalidArgument, "truncation with a null handle");
    Require(sceVideoRecordingCloseWithTruncationByHandle(handle, 0, 1, 0) == InvalidArgument, "positive truncation");
    Require(sceVideoRecordingCloseWithTruncationByHandle(handle, 0, -61, 0) == InvalidArgument, "truncation below -60 seconds");
    Require(sceVideoRecordingCloseWithTruncationByHandle(handle, 0, -61, 1) == InvalidArgument, "truncation below -60 seconds with mode 1");
    Require(sceVideoRecordingCloseWithTruncationByHandle(handle, 0, -60001, 2) == InvalidArgument, "truncation below -60000 milliseconds");
    Require(sceVideoRecordingCloseWithTruncationByHandle(handle, 0, -60001, 3) == InvalidArgument, "truncation below -60000 milliseconds with mode 3");
    Require(sceVideoRecordingCloseWithTruncationByHandle(nullptr, 0, -61, 2) == InvalidArgument, "a null handle is refused within the millisecond limit");
    std::puts("VideoRecording close-by-handle tests passed");
    return 0;
}
