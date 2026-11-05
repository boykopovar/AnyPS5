#include "prx/libSceVideoRecordingP/include/Api.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

static_assert(sizeof(VideoRecordingQueryParam) == 64);
static_assert(offsetof(VideoRecordingQueryParam, mode) == 12);
static_assert(offsetof(VideoRecordingQueryParam, option) == 28);

static_assert(sizeof(VideoRecordingSupportInfo) == 18);

static_assert(sizeof(VideoRecordingParam) == 64);
static_assert(offsetof(VideoRecordingParam, duration) == 8);
static_assert(offsetof(VideoRecordingParam, mode) == 12);
static_assert(offsetof(VideoRecordingParam, affinityMask) == 16);
static_assert(offsetof(VideoRecordingParam, threadPriority) == 24);
static_assert(offsetof(VideoRecordingParam, option) == 32);
static_assert(offsetof(VideoRecordingParam, format) == 36);
static_assert(sizeof(VideoRecordingParam2) == 40);
static_assert(offsetof(VideoRecordingParam2, threadPriority) == 16);
static_assert(offsetof(VideoRecordingParam2, duration) == 20);
static_assert(offsetof(VideoRecordingParam2, format) == 36);

namespace {
constexpr int SceVideoRecordingStatusNone = 0;
constexpr int ErrorInvalidArgument = static_cast<int>(0x80a80003);
constexpr int ErrorNotOpen = static_cast<int>(0x80a80006);
constexpr int ErrorNoVideoFormat = static_cast<int>(0x80a80008);
constexpr int MinimumHeapSize = 0x1000;
constexpr std::uint64_t MinimumParamSize = 0x10;
constexpr std::uint32_t MaximumDuration = 3600;

bool IsVideoFormat(std::uint32_t format) {
 switch (format) {
 case 0x1:
 case 0x2:
 case 0x10:
 case 0x11:
 case 0x28:
 case 0x40:
 case 0x41:
  return true;
 default:
  return false;
 }
}

int CheckParam(const VideoRecordingParam* param) {
 if (param == nullptr || param->size < MinimumParamSize) return ErrorInvalidArgument;
 if (param->duration == 0 || param->duration > MaximumDuration || param->mode > 1) return ErrorInvalidArgument;
 const std::uint32_t option = param->size > offsetof(VideoRecordingParam, option) ? param->option : 0;
 const std::uint32_t format = param->size > offsetof(VideoRecordingParam, format) ? param->format : 0;
 if (option == 0 && format == 0) return MinimumHeapSize;
 if (option > 1 || (format != 0 && !IsVideoFormat(format))) return ErrorInvalidArgument;
 if (format == 0) return ErrorNoVideoFormat;
 return 0;
}
}

extern "C" {

void APS5_VABI _sceVideoRecordingQueryParam_nid_postfix(VideoRecordingQueryParam* param) {
 (void)param;
 NotImplemented_nid_no_patch("_sceVideoRecordingQueryParam");
}

int APS5_VABI sceVideoRecordingClose(int discard) {
 (void)discard;
 return ErrorNotOpen;
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
 if (path == nullptr || param == nullptr || heap == nullptr || heapSize < 0) return ErrorInvalidArgument;
 const int result = CheckParam(param);
 if (result < 0) return result;
 if (result == MinimumHeapSize && heapSize < MinimumHeapSize) return ErrorInvalidArgument;
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
 const int result = CheckParam(param);
 if (result == 0) NotImplemented_nid_no_patch("sceVideoRecordingQueryMemSize (heap size for a video format)");
 return result;
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
 return ErrorNotOpen;
}

int APS5_VABI sceVideoRecordingStop(void) {
 return ErrorNotOpen;
}

int APS5_VABI sceVideoRecordingTerm(void) {
 return 0;
}

APS5_EXPORT("iQS6DUtLybE", videoRecordingUnknown_iQS6DUtLybE);
int APS5_VABI videoRecordingUnknown_iQS6DUtLybE(void) {
 NotImplemented_nid_no_patch("iQS6DUtLybE");
 return 0;
}

int APS5_VABI sceVideoRecordingGetCopyrightImageMemory() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceVideoRecordingCloseWithTruncationByHandle() {
    NotImplemented_nid_no_patch(__func__);
}


}
