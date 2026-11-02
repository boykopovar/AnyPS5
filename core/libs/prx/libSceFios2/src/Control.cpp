#include "prx/libSceFios2/include/Api.hpp"
#include "Runtime.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include <algorithm>
#include <vector>

extern "C" {

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
    std::vector<SceFiosHandle> handles(capacity);
    const std::uint32_t total = list(handles.data(), capacity);
    const std::uint32_t count = std::min(total, capacity);
    if (out != nullptr && count != 0) Fios::CopyOut(out, handles.data(), count * sizeof(SceFiosHandle));
    return total;
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

bool APS5_VABI sceFiosOpIsDone(SceFiosOp op) {
    return Fios::OpDone(op);
}

int APS5_VABI sceFiosOpWait(SceFiosOp op) {
    return Fios::Wait(op);
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
