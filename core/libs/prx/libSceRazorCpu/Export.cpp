#include "prx/libSceRazorCpu/include/Api.hpp"
#include <array>
#include <atomic>
#include <cerrno>
#include <cstdint>
#include <cstddef>
#include <stdexcept>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

static constexpr int SCE_RAZOR_CPU_ERROR_NOT_INITIALIZED = static_cast<int>(0x80580000);
static constexpr int SCE_RAZOR_CPU_ERROR_ALREADY_INITIALIZED = static_cast<int>(0x80580001);
static constexpr int SCE_RAZOR_CPU_ERROR_INVALID_ARGUMENT = static_cast<int>(0x80580004);
static constexpr int SCE_RAZOR_CPU_ERROR_INSUFFICIENT_BUFFER = static_cast<int>(0x80580005);
static constexpr int SCE_RAZOR_CPU_ERROR_CAPTURE_NOT_ENABLED = static_cast<int>(0x8058000c);

namespace {

std::atomic<bool> g_initialized{false};

}

extern "C" {

uint32_t APS5_VABI sceRazorCpuIsCapturing(void) {
 return 0;
}

int APS5_VABI sceRazorCpuJobManagerDispatch(const char* label, uint16_t firstId, uint16_t secondId, uint16_t firstCount, uint16_t secondCount, uint64_t firstValue, uint64_t secondValue) {
 (void)label;
 (void)firstId;
 (void)secondId;
 (void)firstCount;
 (void)secondCount;
 (void)firstValue;
 (void)secondValue;
 return 0;
}

int APS5_VABI sceRazorCpuJobManagerJob(uint16_t firstId, uint16_t secondId, uint16_t thirdId, uint16_t event) {
 (void)firstId;
 (void)secondId;
 (void)thirdId;
 (void)event;
 return 0;
}

int APS5_VABI sceRazorCpuJobManagerSequence(const char* label, uint16_t firstId, uint16_t secondId, uint16_t event) {
 (void)label;
 (void)firstId;
 (void)secondId;
 (void)event;
 return 0;
}

int APS5_VABI sceRazorCpuPushMarker(const char* label, uint32_t color, uint32_t flags) {
 (void)label;
 (void)color;
 (void)flags;
 return 0;
}

int APS5_VABI sceRazorCpuNamedSync(const char* label) {
 (void)label;
 return 0;
}

int APS5_VABI sceRazorCpuPushMarkerStatic(const char* name, uint32_t color, uint32_t flags) {
 (void)name;
 (void)color;
 (void)flags;
 return 0;
}

int APS5_VABI sceRazorCpuPopMarker(void) {
 return 0;
}

bool APS5_VABI sceRazorCpuFlushOccurred(uint64_t* timeSpentInFlush) {
 if (!g_initialized.load(std::memory_order_acquire)) {
  return false;
 }
 if (timeSpentInFlush != nullptr) {
  const int savedErrno = errno;
  const std::uint64_t zero = 0;
  (void)GuestCopyOut_nid_no_patch(timeSpentInFlush, &zero, sizeof(zero));
  errno = savedErrno;
 }
 return false;
}

int APS5_VABI sceRazorCpuPlotValue(const char* series, float value) {
 (void)series;
 (void)value;
 return 0;
}

int APS5_VABI sceRazorCpuWriteBookmark(const char* label, const char* description) {
 (void)label;
 (void)description;
 return 0;
}

int APS5_VABI sceRazorCpuSync(void) {
 return 0;
}

void APS5_VABI sceRazorCpuDisableFiberUserMarkers(void) {
}

int APS5_VABI sceRazorCpuInitDataTags(void* buffer, size_t size) {
 if (!g_initialized.load(std::memory_order_acquire)) {
  return 0;
 }
 if (buffer == nullptr) {
  return SCE_RAZOR_CPU_ERROR_INVALID_ARGUMENT;
 }
 if (size < 96u) {
  return SCE_RAZOR_CPU_ERROR_INSUFFICIENT_BUFFER;
 }
 std::array<std::uint8_t, 256> zeros{};
 const int savedErrno = errno;
 size_t offset = 0;
 while (offset < size) {
  const size_t chunk = size - offset < zeros.size() ? size - offset : zeros.size();
  if (!GuestCopyOut_nid_no_patch(static_cast<std::uint8_t*>(buffer) + offset, zeros.data(), chunk)) {
   errno = savedErrno;
   return SCE_RAZOR_CPU_ERROR_INVALID_ARGUMENT;
  }
  offset += chunk;
 }
 errno = savedErrno;
 return 0;
}

int APS5_VABI sceRazorCpuShutdownDataTags(void) {
 return 0;
}

size_t APS5_VABI sceRazorCpuGetDataTagStorageSize(uint32_t tagCount) {
 if (!g_initialized.load(std::memory_order_acquire)) {
  return 0;
 }
 return static_cast<size_t>(tagCount) * 96u;
}

int APS5_VABI sceRazorCpuTagArray(const char* label, uint32_t category, void* array, size_t bufferSize, size_t elementSize) {
 (void)label;
 (void)category;
 (void)array;
 (void)bufferSize;
 (void)elementSize;
 return 0;
}

int APS5_VABI sceRazorCpuTagBuffer(const char* label, uint32_t category, void* buffer, size_t bufferSize) {
 (void)label;
 (void)category;
 (void)buffer;
 (void)bufferSize;
 return 0;
}

int APS5_VABI sceRazorCpuResizeTaggedBuffer(uint32_t category, void* buffer, size_t newBufferSize) {
 (void)category;
 (void)buffer;
 (void)newBufferSize;
 return 0;
}

int APS5_VABI sceRazorCpuUnTagBuffer(uint32_t category, void* buffer) {
 (void)category;
 (void)buffer;
 return 0;
}

int APS5_VABI sceRazorCpuBeginLogicalFileAccess(const char* filename, uint64_t tag, uint64_t size, uint8_t operation) {
 (void)filename;
 (void)tag;
 (void)size;
 (void)operation;
 return 0;
}

int APS5_VABI sceRazorCpuEndLogicalFileAccess(void) {
 return 0;
}

int APS5_VABI sceRazorCpuFiberLogNameChange(void) {
 return 0;
}

int APS5_VABI sceRazorCpuFiberSwitch(void) {
 return 0;
}

int APS5_VABI sceRazorCpuGpuMarkerSync(void) {
 return 0;
}

int APS5_VABI sceRazorCpuInitializeGpuMarkerContext(const uint8_t* context) {
 if (!context) return 22;
 const int savedErrno = errno;
 std::uint8_t firstByte{};
 if (!GuestCopyIn_nid_no_patch(&firstByte, context, sizeof(firstByte))) {
  errno = savedErrno;
  return 22;
 }
 if (firstByte != 1) {
  errno = savedErrno;
  return 22;
 }
 errno = savedErrno;
 return 0;
}

int APS5_VABI sceRazorCpuInitializeInternal(void* context) {
 if (g_initialized.load(std::memory_order_acquire)) {
  return SCE_RAZOR_CPU_ERROR_ALREADY_INITIALIZED;
 }
 if (context == nullptr) {
  return SCE_RAZOR_CPU_ERROR_INVALID_ARGUMENT;
 }
 const int savedErrno = errno;
 void* callback = nullptr;
 const bool copied = GuestCopyIn_nid_no_patch(&callback, static_cast<const std::uint8_t*>(context) + 0x48, sizeof(callback));
 errno = savedErrno;
 if (!copied) {
  return SCE_RAZOR_CPU_ERROR_INVALID_ARGUMENT;
 }
 if (callback != nullptr) {
  throw std::runtime_error("libSceRazorCpu: non-null sceRazorCpuInitializeInternal callback is not implemented");
 }
 if (g_initialized.exchange(true, std::memory_order_acq_rel)) {
  return SCE_RAZOR_CPU_ERROR_ALREADY_INITIALIZED;
 }
 return 0;
}

void APS5_VABI sceRazorCpuSetPopMarkerCallback(void* callback) {
 (void)callback;
}

void APS5_VABI sceRazorCpuSetPushMarkerCallback(void* callback) {
 (void)callback;
}

void APS5_VABI sceRazorCpuSetPushMarkerStaticCallback(void* callback) {
 (void)callback;
}

int APS5_VABI sceRazorCpuStartCaptureInternal(void) {
 if (!g_initialized.load(std::memory_order_acquire)) {
  return SCE_RAZOR_CPU_ERROR_NOT_INITIALIZED;
 }
 return SCE_RAZOR_CPU_ERROR_CAPTURE_NOT_ENABLED;
}

int APS5_VABI sceRazorCpuStopCaptureInternal(void) {
 if (!g_initialized.load(std::memory_order_acquire)) {
  return SCE_RAZOR_CPU_ERROR_NOT_INITIALIZED;
 }
 return SCE_RAZOR_CPU_ERROR_CAPTURE_NOT_ENABLED;
}

int APS5_VABI sceRazorCpuWorkloadRunBegin(void) {
 return 0;
}

int APS5_VABI sceRazorCpuWorkloadRunEnd(void) {
 return 0;
}

int APS5_VABI sceRazorCpuWorkloadSubmit(void) {
 return 0;
}

int APS5_VABI sceRazorCpuUserSignalWaitBegin() {
 return 0;
}

int APS5_VABI sceRazorCpuUserSignalPost() {
 return 0;
}

int APS5_VABI sceRazorCpuWriteBookmarkEnd() {
 return 0;
}

int APS5_VABI sceRazorCpuUserSignalWaitEnd() {
 return 0;
}

int APS5_VABI sceRazorCpuWorkloadRunBeginEx() {
 return 0;
}

int APS5_VABI sceRazorCpuUserSignalPostEx() {
 return 0;
}

int APS5_VABI sceRazorCpuUserSignalWaitBeginEx() {
 return 0;
}

}
