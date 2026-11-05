#include "prx/libSceFios2/include/Api.hpp"
#include "Runtime.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include <algorithm>
#include <atomic>
#include <vector>

namespace {
std::atomic<std::uint8_t> g_decompressorThreads{0};
}

extern "C" {

int APS5_VABI sceFiosArchiveGetDecompressorThreadCount() {
    return g_decompressorThreads.load(std::memory_order_relaxed);
}

int APS5_VABI sceFiosArchiveSetDecompressorThreadCount(std::uint8_t count) {
    auto current = g_decompressorThreads.load(std::memory_order_relaxed);
    std::uint8_t updated = 0;
    do {
        updated = count == 0 ? 1 : std::min(current, count);
    } while (!g_decompressorThreads.compare_exchange_weak(current, updated, std::memory_order_relaxed));
    return updated;
}

SceFiosOp APS5_VABI sceFiosArchiveGetMountBufferSize(const SceFiosOpAttr*, const char*, SceFiosSize*) {
    return 0;
}

SceFiosSize APS5_VABI sceFiosArchiveGetMountBufferSizeSync(const SceFiosOpAttr*, const char*, SceFiosSize*) {
    return SCE_FIOS_ERROR_UNIMPLEMENTED;
}

SceFiosOp APS5_VABI sceFiosArchiveMount(const SceFiosOpAttr*, SceFiosFH*, const char*, const char*, const void*, std::uint64_t, const void*) {
    return 0;
}

int APS5_VABI sceFiosArchiveMountSync(const SceFiosOpAttr*, SceFiosFH*, const char*, const char*, const void*, std::uint64_t, const void*) {
    return SCE_FIOS_ERROR_UNIMPLEMENTED;
}

SceFiosOp APS5_VABI sceFiosArchiveMountWithOrder(const SceFiosOpAttr*, SceFiosFH*, const char*, const char*, const void*, std::uint64_t, const void*, std::uint32_t) {
    return 0;
}

int APS5_VABI sceFiosArchiveMountWithOrderSync(const SceFiosOpAttr*, SceFiosFH*, const char*, const char*, const void*, std::uint64_t, const void*, std::uint32_t) {
    return SCE_FIOS_ERROR_UNIMPLEMENTED;
}

SceFiosOp APS5_VABI sceFiosArchiveUnmount(const SceFiosOpAttr*, SceFiosFH) {
    return 0;
}

int APS5_VABI sceFiosArchiveUnmountSync(const SceFiosOpAttr*, SceFiosFH) {
    return SCE_FIOS_ERROR_UNIMPLEMENTED;
}

int APS5_VABI sceFiosIOFilterAdd(std::int32_t index, SceFiosIOFilterCallback callback, void*) {
    if (callback == nullptr) return SCE_FIOS_ERROR_BAD_PTR;
    if (index < 0 || index >= 256) return SCE_FIOS_ERROR_BAD_INDEX;
    NotImplemented_nid_no_patch(__func__);
    return SCE_FIOS_ERROR_UNIMPLEMENTED;
}

int APS5_VABI sceFiosIOFilterPsarcDearchiver(void* descriptor, std::int32_t mode, void*) {
    if (mode == 0 && descriptor == nullptr) return SCE_FIOS_ERROR_BAD_PTR;
    NotImplemented_nid_no_patch(__func__);
    return SCE_FIOS_ERROR_UNIMPLEMENTED;
}

int APS5_VABI sceFiosInitialize(const SceFiosParams* params) {
    return Fios::Initialize(params) ? SCE_FIOS_OK : SCE_FIOS_ERROR_BAD_PTR;
}

void APS5_VABI sceFiosTerminate() {
    Fios::Terminate();
}

bool APS5_VABI sceFiosIsInitialized(SceFiosParams* outParams) {
    return Fios::IsInitialized(outParams);
}

void APS5_VABI sceFiosUpdateParameters(const SceFiosParams* params) {
    Fios::UpdateParameters(params);
}

void APS5_VABI sceFiosSetGlobalDefaultOpAttr(const SceFiosOpAttr* attr) {
    Fios::SetGlobalAttr(attr);
}

bool APS5_VABI sceFiosGetGlobalDefaultOpAttr(SceFiosOpAttr* outAttr) {
    SceFiosOpAttr attr{};
    const bool present = Fios::GlobalAttr(attr);
    if (outAttr != nullptr && present) Fios::CopyOut(outAttr, &attr, sizeof(attr));
    return present;
}

void APS5_VABI sceFiosSetThreadDefaultOpAttr(const SceFiosOpAttr* attr) {
    Fios::SetThreadAttr(attr);
}

bool APS5_VABI sceFiosGetThreadDefaultOpAttr(SceFiosOpAttr* outAttr) {
    SceFiosOpAttr attr{};
    const bool present = Fios::ThreadAttr(attr);
    if (outAttr != nullptr && present) Fios::CopyOut(outAttr, &attr, sizeof(attr));
    return present;
}

void APS5_VABI sceFiosGetDefaultOpAttr(SceFiosOpAttr* outAttr) {
    SceFiosOpAttr attr{};
    if (!Fios::ThreadAttr(attr)) Fios::GlobalAttr(attr);
    if (outAttr != nullptr) Fios::CopyOut(outAttr, &attr, sizeof(attr));
}

void APS5_VABI sceFiosSuspend() {
    Fios::Suspend();
}

void APS5_VABI sceFiosResume() {
    Fios::Resume();
}

std::uint32_t APS5_VABI sceFiosGetSuspendCount() {
    return Fios::SuspendCount();
}

bool APS5_VABI sceFiosIsSuspended() {
    return Fios::SuspendCount() != 0;
}

void APS5_VABI sceFiosCancelAllOps() {
    Fios::CancelAll();
}

void APS5_VABI sceFiosShutdownAndCancelOps() {
    Fios::Terminate();
}

void APS5_VABI sceFiosCloseAllFiles() {
    Fios::CloseAllFiles();
}

bool APS5_VABI sceFiosIsIdle() {
    return Fios::IsIdle();
}

static std::uint32_t CopyHandles(std::uint32_t (*list)(SceFiosHandle*, std::uint32_t), SceFiosHandle* out, std::uint32_t capacity) {
    const std::uint32_t total = list(nullptr, 0);
    std::vector<SceFiosHandle> handles(std::min(total, capacity));
    const std::uint32_t listed = list(handles.data(), static_cast<std::uint32_t>(handles.size()));
    const std::uint32_t count = std::min(listed, static_cast<std::uint32_t>(handles.size()));
    if (out != nullptr && count != 0) Fios::CopyOut(out, handles.data(), count * sizeof(SceFiosHandle));
    return listed;
}

std::uint32_t APS5_VABI sceFiosGetAllFHs(SceFiosFH* outArray, std::uint32_t arraySize) {
    return CopyHandles(Fios::ListFiles, outArray, arraySize);
}

std::uint32_t APS5_VABI sceFiosGetAllDHs(SceFiosDH* outArray, std::uint32_t arraySize) {
    return CopyHandles(Fios::ListDirectories, outArray, arraySize);
}

std::uint32_t APS5_VABI sceFiosGetAllOps(SceFiosOp* outArray, std::uint32_t arraySize) {
    return CopyHandles(Fios::ListOps, outArray, arraySize);
}

bool APS5_VABI sceFiosIsValidHandle(SceFiosHandle handle) {
    return Fios::IsValidHandle(handle);
}

void APS5_VABI sceFiosOpRescheduleWithPriority(SceFiosOp op, const void*, std::int32_t) {
    if (Fios::OpAttr(op) == nullptr || Fios::OpDone(op)) return;
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI sceFiosOpReschedule(SceFiosOp op, const void* scheduler) {
    const SceFiosOpAttr* attr = Fios::OpAttr(op);
    if (attr == nullptr || Fios::OpDone(op)) return;
    sceFiosOpRescheduleWithPriority(op, scheduler, attr->priority);
}

void APS5_VABI sceFiosCacheFlushSync() {}

void APS5_VABI sceFiosCacheFlushFileSync(const char*) {}

void APS5_VABI sceFiosCacheFlushFileRangeSync(const char*, SceFiosOffset, SceFiosSize) {}

std::int64_t APS5_VABI sceFiosCacheContainsFileRangeSync(const char*, std::uint64_t, std::uint64_t) {
    return SCE_FIOS_ERROR_BAD_INDEX;
}

int APS5_VABI sceFiosIOFilterGetInfo(std::int32_t, void**, void**) {
    return SCE_FIOS_ERROR_BAD_INDEX;
}

int APS5_VABI sceFiosIOFilterRemove(std::int32_t) {
    return SCE_FIOS_ERROR_BAD_INDEX;
}

int APS5_VABI sceFiosIOFilterCache(const void* config, std::int32_t, const void* context) {
    if (config == nullptr || context == nullptr) return SCE_FIOS_ERROR_BAD_PTR;
    NotImplemented_nid_no_patch(__func__);
    return SCE_FIOS_ERROR_UNIMPLEMENTED;
}

SceFiosOp APS5_VABI sceFiosCachePrefetchFH(const SceFiosOpAttr*, SceFiosFH) {
    return 0;
}

SceFiosOp APS5_VABI sceFiosCachePrefetchFHRange(const SceFiosOpAttr*, SceFiosFH, SceFiosOffset, SceFiosSize) {
    return 0;
}

int APS5_VABI sceFiosCachePrefetchFHRangeSync(const SceFiosOpAttr*, SceFiosFH, SceFiosOffset, SceFiosSize) {
    return SCE_FIOS_OK;
}

int APS5_VABI sceFiosCachePrefetchFHSync(const SceFiosOpAttr*, SceFiosFH) {
    return SCE_FIOS_OK;
}

SceFiosOp APS5_VABI sceFiosCachePrefetchFile(const SceFiosOpAttr*, const char*) {
    return 0;
}

SceFiosOp APS5_VABI sceFiosCachePrefetchFileRange(const SceFiosOpAttr*, const char*, SceFiosOffset, SceFiosSize) {
    return 0;
}

int APS5_VABI sceFiosCachePrefetchFileRangeSync(const SceFiosOpAttr*, const char*, SceFiosOffset, SceFiosSize) {
    return SCE_FIOS_OK;
}

int APS5_VABI sceFiosCachePrefetchFileSync(const SceFiosOpAttr*, const char*) {
    return SCE_FIOS_ERROR_UNIMPLEMENTED;
}

std::int64_t APS5_VABI sceFiosCacheContainsFileSync(const char* path, std::uint64_t* outCached) {
    const std::uint64_t cached = 0;
    if (outCached != nullptr && !Fios::CopyOut(outCached, &cached, sizeof(cached))) return SCE_FIOS_ERROR_BAD_PTR;
    Fios::OpInfo info;
    if (const std::int32_t error = Fios::CopyPath(path, info.path)) return error;
    SceFiosStat stat{};
    if (const std::int32_t error = Fios::StatPath(info.path, stat)) return error;
    if ((stat.statFlags & SCE_FIOS_STAT_DIRECTORY) != 0) return SCE_FIOS_ERROR_NOT_A_FILE;
    return SCE_FIOS_OK;
}

int APS5_VABI sceFiosOverlayGetList(std::int32_t* ids, std::uint64_t capacity, std::uint64_t* count) {
    if (count == nullptr || (capacity != 0 && ids == nullptr)) return SCE_FIOS_ERROR_BAD_PTR;
    const std::uint64_t empty = 0;
    return Fios::CopyOut(count, &empty, sizeof(empty)) ? SCE_FIOS_OK : SCE_FIOS_ERROR_BAD_PTR;
}

int APS5_VABI sceFiosOverlayGetInfo(std::int32_t, void* outInfo) {
    return outInfo == nullptr ? SCE_FIOS_ERROR_BAD_PTR : SCE_FIOS_ERROR_BAD_OVERLAY;
}

int APS5_VABI sceFiosOverlayRemove(std::int32_t) {
    return SCE_FIOS_ERROR_BAD_OVERLAY;
}

int APS5_VABI sceFiosOverlayModify(std::int32_t, const void* overlay) {
    return overlay == nullptr ? SCE_FIOS_ERROR_BAD_PTR : SCE_FIOS_ERROR_BAD_OVERLAY;
}

int APS5_VABI sceFiosOverlayAdd(const void* overlay, void*) {
    if (overlay == nullptr) return SCE_FIOS_ERROR_BAD_PTR;
    NotImplemented_nid_no_patch(__func__);
    return SCE_FIOS_ERROR_UNIMPLEMENTED;
}

int APS5_VABI sceFiosOverlayResolveSync(std::uint32_t capacity, const char*, const char*, void*) {
    if (capacity > 0x400) return SCE_FIOS_ERROR_BAD_SIZE;
    return SCE_FIOS_ERROR_BAD_OVERLAY;
}

int APS5_VABI sceFiosResolveSync(const SceFiosOpAttr*, const void* request, void* result) {
    if (Fios::IsInitialized(nullptr)) NotImplemented_nid_no_patch(__func__);
    if (request == nullptr || result == nullptr) return SCE_FIOS_ERROR_BAD_PTR;
    std::uint8_t copied[0x410];
    if (!Fios::CopyIn(copied, request, sizeof(copied)) || !Fios::CopyOut(result, copied, sizeof(copied))) return SCE_FIOS_ERROR_BAD_PTR;
    return SCE_FIOS_OK;
}

SceFiosOp APS5_VABI sceFiosResolve(const SceFiosOpAttr*, const void*, void*) {
    if (Fios::IsInitialized(nullptr)) NotImplemented_nid_no_patch(__func__);
    return 0;
}

bool APS5_VABI sceFiosOpIsDone(SceFiosOp op) {
    return Fios::OpDone(op);
}

int APS5_VABI sceFiosOpWait(SceFiosOp op) {
    return Fios::Wait(op);
}

int APS5_VABI sceFiosOpWaitUntil(SceFiosOp op, SceFiosTime deadline) {
    return Fios::WaitUntil(op, deadline);
}

int APS5_VABI sceFiosOpSetBuffer(SceFiosOp op, void* buffer) {
    return Fios::SetOpBuffer(op, buffer);
}

void APS5_VABI sceFiosOpDelete(SceFiosOp op) {
    Fios::DeleteOp(op);
}

int APS5_VABI sceFiosOpSyncWait(SceFiosOp op) {
    Fios::Completion completion;
    if (!Fios::SyncWait(op, completion)) return SCE_FIOS_ERROR_BAD_OP;
    return completion.error != SCE_FIOS_OK ? completion.error : static_cast<std::int32_t>(completion.result);
}

std::int64_t APS5_VABI sceFiosOpSyncWaitForIO(SceFiosOp op) {
    Fios::Completion completion;
    if (!Fios::SyncWait(op, completion)) return SCE_FIOS_ERROR_BAD_OP;
    return completion.error != SCE_FIOS_OK ? completion.error : completion.result;
}

int APS5_VABI sceFiosOpGetError(SceFiosOp op) {
    std::int32_t error = SCE_FIOS_ERROR_BAD_OP;
    Fios::OpError(op, error);
    return error;
}

void APS5_VABI sceFiosOpCancel(SceFiosOp op) {
    Fios::CancelOp(op);
}

bool APS5_VABI sceFiosOpIsCancelled(SceFiosOp op) {
    return Fios::OpIsCancelled(op);
}

const SceFiosOpAttr* APS5_VABI sceFiosOpGetAttr(SceFiosOp op) {
    return Fios::OpAttr(op);
}

const char* APS5_VABI sceFiosOpGetPath(SceFiosOp op) {
    return Fios::OpPath(op);
}

void* APS5_VABI sceFiosOpGetBuffer(SceFiosOp op) {
    return Fios::OpBuffer(op);
}

std::int64_t APS5_VABI sceFiosOpGetOffset(SceFiosOp op) {
    std::int64_t request = 0, actual = 0, offset = 0;
    return Fios::QueryOp(op, request, actual, offset) ? offset : SCE_FIOS_ERROR_BAD_OP;
}

std::int64_t APS5_VABI sceFiosOpGetRequestCount(SceFiosOp op) {
    std::int64_t request = 0, actual = 0, offset = 0;
    return Fios::QueryOp(op, request, actual, offset) ? request : SCE_FIOS_ERROR_BAD_OP;
}

std::int64_t APS5_VABI sceFiosOpGetActualCount(SceFiosOp op) {
    std::int64_t request = 0, actual = 0, offset = 0;
    return Fios::QueryOp(op, request, actual, offset) ? actual : SCE_FIOS_ERROR_BAD_OP;
}

}
