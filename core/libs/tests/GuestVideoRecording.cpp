#include "prx/libSceVideoRecordingP/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>

namespace {

constexpr int StatusNone = 0;
constexpr int InvalidArgument = static_cast<int>(0x80a80003);
constexpr int NotOpen = static_cast<int>(0x80a80006);
constexpr int NoVideoFormat = static_cast<int>(0x80a80008);
constexpr int MinimumHeap = 0x1000;
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

VideoRecordingParam RecordingParam(std::uint32_t format) {
    VideoRecordingParam param{};
    param.size = sizeof(param);
    param.duration = 3600;
    param.mode = 0;
    param.affinityMask = 0x1fff;
    param.threadPriority = 700;
    param.option = 1;
    param.format = format;
    return param;
}

int Query(const VideoRecordingParam& param) {
    return sceVideoRecordingQueryMemSize(&param);
}

}

int main() {
    Require(sceVideoRecordingGetStatus() == StatusNone, "a recorder that was never opened reported a status");
    Require(sceVideoRecordingGetStatus() == StatusNone, "the status changed between two reads");
    Require(sceVideoRecordingStart() == NotOpen, "the encoder started without a recorder");
    Require(sceVideoRecordingStop() == NotOpen, "the encoder stopped without a recorder");
    Require(sceVideoRecordingClose(0) == NotOpen, "a recorder that was never opened closed");
    Require(sceVideoRecordingClose(1) == NotOpen, "a recorder that was never opened closed with discard");

    std::array<std::uint8_t, 24> query{};
    query.fill(Sentinel);
    Require(Throws([&] { _sceVideoRecordingQueryParam_nid_postfix(reinterpret_cast<VideoRecordingQueryParam*>(query.data())); }), "the default parameters were produced");

    Require(sceVideoRecordingQueryMemSize(nullptr) == InvalidArgument, "a heap size was reported for no parameters");
    for (std::uint32_t format = 0x3; format <= 0x44; ++format) {
        if (format == 0x10 || format == 0x11 || format == 0x28 || format == 0x40 || format == 0x41) continue;
        Require(Query(RecordingParam(format)) == InvalidArgument, "an unknown video format was accepted");
    }
    Require(Query(RecordingParam(0)) == NoVideoFormat, "the option without a video format was accepted");
    for (const std::uint64_t size : {0x0ull, 0x8ull, 0xfull}) {
        VideoRecordingParam param = RecordingParam(1);
        param.size = size;
        Require(Query(param) == InvalidArgument, "a block shorter than its duration and mode was accepted");
    }
    for (const std::uint64_t size : {0x10ull, 0x11ull, 0x18ull, 0x1cull, 0x1dull, 0x20ull}) {
        VideoRecordingParam param = RecordingParam(1);
        param.size = size;
        Require(Query(param) == MinimumHeap, "a block without option and format did not ask for 0x1000 bytes");
    }
    for (const std::uint64_t size : {0x21ull, 0x24ull}) {
        VideoRecordingParam param = RecordingParam(1);
        param.size = size;
        Require(Query(param) == NoVideoFormat, "a block that ends before its format was accepted");
    }
    for (const std::uint32_t duration : {0u, 3601u, 7200u, 0xffffffffu}) {
        VideoRecordingParam param = RecordingParam(1);
        param.duration = duration;
        Require(Query(param) == InvalidArgument, "a duration outside 1 to 3600 was accepted");
    }
    for (const std::uint32_t mode : {2u, 3u}) {
        VideoRecordingParam param = RecordingParam(1);
        param.mode = mode;
        Require(Query(param) == InvalidArgument, "a mode above 1 was accepted");
    }
    for (const std::uint32_t option : {2u, 3u}) {
        VideoRecordingParam param = RecordingParam(1);
        param.option = option;
        Require(Query(param) == InvalidArgument, "an option above 1 was accepted");
        param.format = 0;
        Require(Query(param) == InvalidArgument, "an option above 1 without a format was accepted");
    }
    VideoRecordingParam minimal = RecordingParam(0);
    minimal.option = 0;
    Require(Query(minimal) == MinimumHeap, "a block without option and format did not ask for 0x1000 bytes");
    for (const std::uint32_t value : {0u, 1u, 2u, 3u}) {
        VideoRecordingParam param = RecordingParam(0);
        param.option = 0;
        param.outputMode = static_cast<std::int32_t>(value);
        param.threadPriority = static_cast<std::int32_t>(value);
        param.affinityMask = value;
        param.reserved40 = value;
        param.reserved44 = value;
        Require(Query(param) == MinimumHeap, "fields the heap size does not depend on changed it");
    }

    std::array<std::uint8_t, 0x1000> heap{};
    heap.fill(Sentinel);
    const VideoRecordingParam video = RecordingParam(1);
    VideoRecordingParam refused = RecordingParam(1);
    refused.duration = 0;
    Require(sceVideoRecordingOpen(nullptr, &video, heap.data(), static_cast<int>(heap.size())) == InvalidArgument, "the recorder opened without a path");
    Require(sceVideoRecordingOpen("clip.mp4", nullptr, heap.data(), static_cast<int>(heap.size())) == InvalidArgument, "the recorder opened without parameters");
    Require(sceVideoRecordingOpen("clip.mp4", &video, nullptr, static_cast<int>(heap.size())) == InvalidArgument, "the recorder opened without a heap");
    Require(sceVideoRecordingOpen("clip.mp4", &video, heap.data(), -1) == InvalidArgument, "the recorder opened with a negative heap size");
    Require(sceVideoRecordingOpen("clip.mp4", &refused, heap.data(), 0x7fffffff) == InvalidArgument, "the recorder opened with parameters QueryMemSize refuses");
    Require(sceVideoRecordingOpen("clip.mp4", &minimal, heap.data(), MinimumHeap - 1) == InvalidArgument, "the recorder opened with a heap smaller than QueryMemSize asks");
    VideoRecordingParam optionOnly = RecordingParam(0);
    Require(sceVideoRecordingOpen("clip.mp4", &optionOnly, heap.data(), static_cast<int>(heap.size())) == NoVideoFormat, "the recorder opened with the option and no format");
    for (const std::uint8_t value : heap) Require(value == Sentinel, "the heap was written");
    Require(sceVideoRecordingGetStatus() == StatusNone, "a failed open changed the status");
    Require(sceVideoRecordingStart() == NotOpen, "a failed open left a recorder to start");
    Require(sceVideoRecordingClose(1) == NotOpen, "a failed open left a recorder to close");

    Require(sceVideoRecordingGetInfo(0xa001, nullptr, 0) == NotOpen, "info 0xa001 validation");
    Require(sceVideoRecordingGetInfo(0xa005, nullptr, 0) == NotOpen, "info 0xa005 validation");
    Require(sceVideoRecordingGetInfo(0, nullptr, 0) == InvalidArgument, "unknown info validation");
    std::puts("VideoRecording tests passed");
    return 0;
}
