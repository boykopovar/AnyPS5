#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceAgcDriverGetShaderDebuggingStatus() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAgcDriverRegisterMultipleResources() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAgcDriverRegisterDefaultOwner(uint32_t* owner_handle) {
    (void)owner_handle;
    return static_cast<int>(0x8A6C9018);
}

int APS5_VABI sceAgcDriverSetValidationErrorOutputFrequency() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAgcDriverSetSubmitValidationMode() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAgcDriverGetSubmitValidationConfig() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAgcDriverGetSubmitValidationMode() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAgcDriverSetSubmitValidationConfig() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAgcDriverAcquireComputeQueue(uint32_t pipeId, uint32_t queueId, void* outQueue) {
    (void)pipeId;
    (void)queueId;
    (void)outQueue;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAgcDriverCreateQueue(uint32_t queueType, void* queueDesc, void* outQueue) {
    (void)queueType;
    (void)queueDesc;
    (void)outQueue;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAgcDriverCwsrResumeAcq(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAgcDriverCwsrSuspendAcq(void* saveArea) {
    (void)saveArea;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

void APS5_VABI sceAgcDriverDebugHardwareStatus(uint32_t target) {
    (void)target;
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceAgcDriverDestroyQueue(void* queue) {
    (void)queue;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAgcDriverGetGpuRefClks(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAgcDriverGetHsOffchipParam(void* outParam1, void* outParam2) {
    (void)outParam1;
    (void)outParam2;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAgcDriverGetRegShadowInfo(void* outInfo) {
    (void)outInfo;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAgcDriverGetRegShadowInfoAgr(void* outInfo) {
    (void)outInfo;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAgcDriverGetReservedDmemForAgc(void* outAddr, void* outSize) {
    (void)outAddr;
    (void)outSize;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

uint32_t APS5_VABI sceAgcDriverGetSetFlipPacketSizeInDwords(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

uint32_t APS5_VABI sceAgcDriverGetSetWorkloadCompletePacketSize(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

uint32_t APS5_VABI sceAgcDriverGetSetWorkloadsActivePacketSize(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

uint64_t APS5_VABI sceAgcDriverGetTraceInitiator(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAgcDriverGetWorkloadStreamInfo(uint32_t streamIndex, void* nameBuffer, uint64_t nameBufferSize, void* infoOut) {
    (void)streamIndex;
    (void)nameBuffer;
    (void)nameBufferSize;
    (void)infoOut;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAgcDriverIDHSSubmit(void* submitInfo) {
    (void)submitInfo;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAgcDriverModuleRegistration(uint64_t moduleIndex, void* interfacePtr) {
    (void)moduleIndex;
    (void)interfacePtr;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAgcDriverNotifyDefaultStates(void* segments0, void* segments1, void* segments2, uint32_t count0, uint32_t count1, uint32_t count2) {
    (void)segments0;
    (void)segments1;
    (void)segments2;
    (void)count0;
    (void)count1;
    (void)count2;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAgcDriverPassInfoDownward(void* infoAddr, uint32_t count) {
    (void)infoAddr;
    (void)count;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAgcDriverPatchClearState(void* registers, uint32_t count) {
    (void)registers;
    (void)count;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAgcDriverReleaseComputeQueue(void* queue) {
    (void)queue;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

uint32_t APS5_VABI sceAgcDriverSetFlip(uint32_t** writeCursorSlot, uint32_t packetSizeInDwords, uint32_t queueType, uint32_t videoOutHandle, int displayBufferIndex, uint32_t flipMode, int64_t flipArg) {
    (void)writeCursorSlot;
    (void)packetSizeInDwords;
    (void)queueType;
    (void)videoOutHandle;
    (void)displayBufferIndex;
    (void)flipMode;
    (void)flipArg;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAgcDriverSetWorkloadComplete(void* commandBuffer, uint32_t queueType, uint32_t streamId, uint32_t workloadIndex) {
    (void)commandBuffer;
    (void)queueType;
    (void)streamId;
    (void)workloadIndex;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAgcDriverSetWorkloadsActive(void* commandBuffer, uint32_t queueType, uint32_t streamId, void* workloadsAddr, uint32_t count) {
    (void)commandBuffer;
    (void)queueType;
    (void)streamId;
    (void)workloadsAddr;
    (void)count;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAgcDriverSetupRegisterShadow(uint32_t type, uint8_t enable, uint8_t flag1, uint8_t flag2, uint32_t size) {
    (void)type;
    (void)enable;
    (void)flag1;
    (void)flag2;
    (void)size;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

void APS5_VABI sceAgcDriverSuspendPointSubmit(void* queue, uint64_t value) {
    (void)queue;
    (void)value;
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceAgcDriverSysEnableSubmitDone45Exception(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

uint32_t APS5_VABI sceAgcDriverSysGetClientNumber(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAgcDriverSysIsGameClosed(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAgcDriverSysSubmitFlipHandleProxy(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAgcDriverTmpInitIdhs(void* context) {
    (void)context;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

uint32_t APS5_VABI sceAgcDriverUserDataImmediateWrite(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

uint32_t APS5_VABI sceAgcDriverUserDataWritePacket(void* commandBuffer, uint32_t queueType, uint32_t header, void* srcAddr, uint32_t size) {
    (void)commandBuffer;
    (void)queueType;
    (void)header;
    (void)srcAddr;
    (void)size;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

uint32_t APS5_VABI sceAgcDriverUserDataWritePopMarker(void* commandBuffer, uint32_t queueType) {
    (void)commandBuffer;
    (void)queueType;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

uint32_t APS5_VABI sceAgcDriverUserDataWritePushMarker(void* commandBuffer, uint32_t queueType, void* srcAddr, uint32_t size, uint32_t modifier) {
    (void)commandBuffer;
    (void)queueType;
    (void)srcAddr;
    (void)size;
    (void)modifier;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

uint32_t APS5_VABI sceAgcDriverUserDataWriteSetMarker(void* commandBuffer, uint32_t queueType, void* srcAddr, uint32_t size, uint32_t modifier) {
    (void)commandBuffer;
    (void)queueType;
    (void)srcAddr;
    (void)size;
    (void)modifier;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
