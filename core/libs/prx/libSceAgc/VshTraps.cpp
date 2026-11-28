#include "prx/libSceAgc/libSceAgc.hpp"
#include "prx/libc/include/General.hpp"

#include <stdexcept>
#include <string>

namespace {

[[noreturn]] void Reject(const char* function) {
    throw std::runtime_error(std::string(function) + "() is not allowed to be called from agc vsh.");
}

}

extern "C" {

int APS5_VABI sceAgcSuspendPointAndCheckStatus_nid_no_patch(void) {
    Reject("sceAgcSuspendPointAndCheckStatus");
}
APS5_EXPORT("b+fis+WZ3Ig", sceAgcSuspendPointAndCheckStatus_nid_no_patch);

std::uint32_t* APS5_VABI sceAgcAcbAcquireMem(CommandBuffer*, std::uint32_t, const volatile void*, std::uint64_t, std::uint32_t) {
    Reject(__func__);
}

std::uint32_t* APS5_VABI sceAgcAcbAtomicGds(CommandBuffer*, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t, std::uint16_t, std::uint16_t, std::uint32_t, std::uint32_t, std::uint64_t, std::uint64_t) {
    Reject(__func__);
}

std::uint32_t* APS5_VABI sceAgcAcbAtomicMem(CommandBuffer*, std::uint8_t, std::uint8_t, const volatile void*, std::uint8_t, std::uint64_t, std::uint64_t, std::uint16_t) {
    Reject(__func__);
}

std::uint32_t* APS5_VABI sceAgcAcbCopyData(CommandBuffer*, std::uint8_t, std::uint8_t, std::uint64_t, std::uint8_t, std::uint8_t, std::uint64_t, std::uint8_t, std::uint8_t) {
    Reject(__func__);
}

std::uint32_t* APS5_VABI sceAgcAcbDmaData(CommandBuffer*, std::uint8_t, std::uint8_t, std::uint64_t, std::uint8_t, std::uint8_t, std::uint64_t, std::uint32_t, std::uint8_t, std::uint8_t) {
    Reject(__func__);
}

std::uint32_t* APS5_VABI sceAgcAcbJump(CommandBuffer*, std::uint8_t, const std::uint32_t*, std::uint32_t) {
    Reject(__func__);
}

std::uint32_t* APS5_VABI sceAgcAcbPrimeUtcl2(CommandBuffer*, const volatile void*, std::uint32_t) {
    Reject(__func__);
}

std::uint32_t* APS5_VABI sceAgcAcbResetQueue(CommandBuffer*, std::uint32_t, std::uint32_t) {
    Reject(__func__);
}

std::uint32_t* APS5_VABI sceAgcAcbRewind(CommandBuffer*, std::uint32_t) {
    Reject(__func__);
}

std::uint32_t* APS5_VABI sceAgcAcbSetFlip(CommandBuffer*, std::uint32_t, std::int32_t, std::uint32_t, std::int64_t) {
    Reject(__func__);
}

std::uint32_t* APS5_VABI sceAgcAcbSetMarker(CommandBuffer*, const char*, std::uint32_t) {
    Reject(__func__);
}

std::uint32_t* APS5_VABI sceAgcAcbWaitRegMem(CommandBuffer*, std::uint8_t, std::uint8_t, std::uint8_t, const volatile void*, std::uint64_t, std::uint64_t, std::uint32_t) {
    Reject(__func__);
}

std::uint32_t* APS5_VABI sceAgcAcbWaitUntilSafeForRendering(CommandBuffer*, std::uint32_t, std::uint32_t) {
    Reject(__func__);
}

std::uint32_t* APS5_VABI sceAgcAcbWriteData(CommandBuffer*, std::uint8_t, std::uint8_t, std::uint64_t, const void*, std::uint32_t, std::uint8_t, std::uint8_t) {
    Reject(__func__);
}

std::uint32_t* APS5_VABI sceAgcCbMemSemaphore(CommandBuffer*, const volatile void*, std::uint32_t, std::uint32_t, std::uint32_t) {
    Reject(__func__);
}

std::uint32_t* APS5_VABI sceAgcDcbAtomicGds(CommandBuffer*, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t, std::uint16_t, std::uint16_t, std::uint32_t, std::uint32_t, std::uint64_t, std::uint64_t) {
    Reject(__func__);
}

std::uint32_t* APS5_VABI sceAgcDcbContextStateOp(CommandBuffer*, std::uint32_t) {
    Reject(__func__);
}

std::uint32_t* APS5_VABI sceAgcDcbResetQueue(CommandBuffer*, std::uint32_t, std::uint32_t) {
    Reject(__func__);
}

std::uint32_t* APS5_VABI sceAgcDcbSetFlip(CommandBuffer*, std::uint32_t, std::int32_t, std::uint32_t, std::int64_t) {
    Reject(__func__);
}

std::uint32_t* APS5_VABI sceAgcDcbWaitUntilSafeForRendering(CommandBuffer*, std::uint32_t, std::uint32_t) {
    Reject(__func__);
}

void* APS5_VABI sceAgcGetRegisterDefaults(void) {
    Reject(__func__);
}

void* APS5_VABI sceAgcGetRegisterDefaultsInternal(void) {
    Reject(__func__);
}

int APS5_VABI sceAgcSuspendPoint(void) {
    Reject(__func__);
}

}
