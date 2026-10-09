#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceAgcAcbPushMarkerSpan() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAgcAcbSetMarkerSpan() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAgcAcbSetWorkloadComplete() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAgcAcbSetWorkloadStreamInactive() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAgcAcbSetWorkloadsActive() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAgcAcquireMemSetEngine() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAgcDcbPushMarkerSpan() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAgcDcbSetMarkerSpan() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAgcDcbSetWorkloadStreamInactive() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAgcDebugRaiseException() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAgcGetDefaultCxStateFlat() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAgcGetSemaphoreLabel() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAgcSetAmmSemaphoreMemory() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAgcSetSemaphoreMemory() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAgcCbMemsetExclusive() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAgcBranchPatchSetThenTarget_0300() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAgcGetGsPrimPayload() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAgcSetShaderInstrumentation() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAgcGetShaderInstrumentation() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAgcBranchPatchSetElseTarget_0300() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

APS5_EXPORT("vieBRwlh1Lw", sceAgcUnknown_vieBRwlh1Lw);
int APS5_VABI sceAgcUnknown_vieBRwlh1Lw(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

uint32_t* APS5_VABI sceAgcAcbAtomicGds(void* commandBuffer, uint32_t atomicOp, uint32_t gdsOffset, uint32_t size, uint32_t count, uint64_t srcData, uint16_t dstSelect, uint32_t value, uint32_t value2, void* gpuAddr, void* returnAddr) {
    (void)commandBuffer;
    (void)atomicOp;
    (void)gdsOffset;
    (void)size;
    (void)count;
    (void)srcData;
    (void)dstSelect;
    (void)value;
    (void)value2;
    (void)gpuAddr;
    (void)returnAddr;
    NotImplemented_nid_no_patch(__func__);
    return nullptr;
}

uint32_t* APS5_VABI sceAgcCbMemSemaphore(void* commandBuffer, void* semaphoreAddr, uint32_t operation, uint32_t modifier, uint32_t value) {
    (void)commandBuffer;
    (void)semaphoreAddr;
    (void)operation;
    (void)modifier;
    (void)value;
    NotImplemented_nid_no_patch(__func__);
    return nullptr;
}

uint32_t* APS5_VABI sceAgcDcbAtomicGds(void* commandBuffer, uint32_t engine, uint32_t atomicOp, uint32_t gdsOffset, uint32_t count, uint32_t gdsMemOffset, uint16_t atomicCmp, uint16_t atomicSrc, uint32_t value, uint32_t modifier, uint64_t srcData, uint64_t cmpData) {
    (void)commandBuffer;
    (void)engine;
    (void)atomicOp;
    (void)gdsOffset;
    (void)count;
    (void)gdsMemOffset;
    (void)atomicCmp;
    (void)atomicSrc;
    (void)value;
    (void)modifier;
    (void)srcData;
    (void)cmpData;
    NotImplemented_nid_no_patch(__func__);
    return nullptr;
}

int APS5_VABI sceAgcGetDataPacketPayloadAddress(void* outAddress, void* packet, uint32_t flag) {
    (void)outAddress;
    (void)packet;
    (void)flag;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAgcGetFusedShaderSize(void* outSize, void* shaderLo, void* shaderHi) {
    (void)outSize;
    (void)shaderLo;
    (void)shaderHi;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAgcSuspendPointAndCheckStatus(void* outStatus) {
    (void)outStatus;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAgcUpdateInterpolantMapping(void* mapping, void* vsShader, void* psShader) {
    (void)mapping;
    (void)vsShader;
    (void)psShader;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
