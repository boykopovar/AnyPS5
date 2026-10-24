#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

struct VideoRecordingContext;

struct VideoRecordingQueryParam {
    std::uint64_t size;
    std::uint32_t reserved8;
    std::int32_t mode;
    std::uint8_t reserved16[12];
    std::uint32_t option;
    std::uint8_t reserved32[32];
};

static_assert(sizeof(VideoRecordingQueryParam) == 64);
static_assert(offsetof(VideoRecordingQueryParam, mode) == 12);
static_assert(offsetof(VideoRecordingQueryParam, option) == 28);


struct VideoRecordingSupportInfo {
    std::uint8_t data[18];
};

static_assert(sizeof(VideoRecordingSupportInfo) == 18);

struct VideoRecordingParam {
    std::uint64_t size;
    std::int32_t threadPriority;
    std::uint32_t duration;
    std::uint32_t mode;
    std::int32_t outputMode;
    std::uint32_t option;
    std::uint32_t format;
};

struct VideoRecordingParam2 {
    std::uint64_t size;
    std::uint64_t affinityMask;
    std::int32_t threadPriority;
    std::uint32_t duration;
    std::uint32_t mode;
    std::int32_t outputMode;
    std::uint32_t option;
    std::uint32_t format;
};

static_assert(sizeof(VideoRecordingParam) == 32);
static_assert(offsetof(VideoRecordingParam, duration) == 12);
static_assert(offsetof(VideoRecordingParam, format) == 28);
static_assert(sizeof(VideoRecordingParam2) == 40);
static_assert(offsetof(VideoRecordingParam2, threadPriority) == 16);
static_assert(offsetof(VideoRecordingParam2, duration) == 20);
static_assert(offsetof(VideoRecordingParam2, format) == 36);

namespace {
constexpr int SceVideoRecordingStatusNone = 0;
}

extern "C" {

void APS5_VABI _sceVideoRecordingQueryParam_nid_postfix(VideoRecordingQueryParam* param) {
 (void)param;
 NotImplemented_nid_no_patch("_sceVideoRecordingQueryParam");
}

int APS5_VABI sceVideoRecordingClose(int discard) {
 (void)discard;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceVideoRecordingCloseByHandle(VideoRecordingContext* handle, int discard) {
 (void)handle;
 (void)discard;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceVideoRecordingCopyBGRAtoNV12(void* dst, const void* src, uint32_t width, uint32_t height) {
 (void)dst;
 (void)src;
 (void)width;
 (void)height;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceVideoRecordingGetInfo(std::int32_t info, void*, std::size_t) {
    switch (info) {
    case 0xa001:
    case 0xa002:
    case 0xa003:
    case 0xa005:
    case 0xa00a:
    case 0xa00b:
    case 0xa00c:
        return static_cast<int>(0x80a80006);
    default:
        return static_cast<int>(0x80a80003);
    }
}

int APS5_VABI sceVideoRecordingGetInfoByHandle(VideoRecordingContext* handle, int32_t info, void* data, size_t size) {
 (void)handle;
 (void)info;
 (void)data;
 (void)size;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceVideoRecordingGetStatus(void) {
 return SceVideoRecordingStatusNone;
}

int APS5_VABI sceVideoRecordingGetSupportInfo(VideoRecordingSupportInfo* info) {
 (void)info;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceVideoRecordingGetVideoOutMode(std::uint32_t* mode) {
 (void)mode;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceVideoRecordingInit(void) {
 return 0;
}

int APS5_VABI sceVideoRecordingInitialize(void* param) {
 (void)param;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceVideoRecordingOpen(const char* path, const VideoRecordingParam* param, void* heap, int heapSize) {
 (void)path;
 (void)param;
 (void)heap;
 (void)heapSize;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceVideoRecordingOpen2(const char* path, const VideoRecordingParam2* param, void* heap, int heapSize) {
 (void)path;
 (void)param;
 (void)heap;
 (void)heapSize;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

std::int64_t APS5_VABI sceVideoRecordingQueryHddSize2(const VideoRecordingParam2* param) {
 (void)param;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceVideoRecordingQueryMemSize(const VideoRecordingParam* param) {
 (void)param;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceVideoRecordingQueryMemSize2(const VideoRecordingParam2* param) {
 (void)param;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceVideoRecordingRestModeProcessGetStatus(void* status) {
 (void)status;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceVideoRecordingRestModeProcessStart(void* param) {
 (void)param;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceVideoRecordingSendCommand(int cmd, const void* param) {
 (void)cmd;
 (void)param;
 return 0;
}

int APS5_VABI sceVideoRecordingSetInfo(std::int32_t info, const void* data, std::size_t size) {
    switch (info) {
    case 0x8002:
    case 0x8003:
        if (!data || size < sizeof(float)) return static_cast<int>(0x80a80003);
        return static_cast<int>(0x80a80002);
    case 0xa004:
    case 0xa006:
        return static_cast<int>(0x80a80006);
    default:
        return static_cast<int>(0x80a80003);
    }
}

int APS5_VABI sceVideoRecordingStart(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceVideoRecordingStop(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceVideoRecordingTerm(void) {
 return 0;
}

APS5_EXPORT("iQS6DUtLybE", videoRecordingUnknown_iQS6DUtLybE);
int APS5_VABI videoRecordingUnknown_iQS6DUtLybE(void) {
 NotImplemented_nid_no_patch("iQS6DUtLybE");
 return 0;
}

}
