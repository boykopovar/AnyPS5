#pragma once

#include "prx/libSceHmd2/Hmd2.hpp"
#include "SceTypes.hpp"
#include <cstddef>
#include <cstring>
#include <mutex>
#include <stdexcept>

namespace Hmd2 {

struct ReprojectionStatus;

}

namespace Hmd2 {

struct ReprojectionOutput {
    std::uint32_t textureDescriptor[8];
};

}

namespace Hmd2 {

struct ReprojectionInitializeParameter {
    void* workMemory;
    void* displayMemory;
    std::int32_t threadPriority;
    std::uint32_t threadAffinity;
    std::uint32_t queueParameter24;
    std::uint32_t queueParameter28;
    std::uint32_t renderingMode;
    std::uint32_t reserved36;
    void* auxiliaryMemory;
    std::uint32_t timingInterval;
    std::uint32_t reserved52[5];
};

}

namespace Hmd2 {

struct ReprojectionLayer;

}

namespace Hmd2 {

struct ReprojectionBufferedLayer;

}

namespace Hmd2 {

struct ReprojectionOption {
    std::uint32_t size;
    std::uint32_t flags;
    std::uint32_t reserved[30];
};

}

namespace Hmd2 {

struct ReprojectionParameter {
    const ReprojectionLayer* layers;
    std::int32_t layerCount;
    std::uint8_t padding[4];
    const ReprojectionOption* option;
};

}

namespace Hmd2 {

struct ReprojectionBufferedParameter {
    const ReprojectionBufferedLayer* layers;
    std::int32_t layerCount;
    std::uint8_t padding[4];
    const ReprojectionOption* option;
};

}

namespace Hmd2 {

struct MirroringParameter {
    void* workMemory;
    void* displayMemory;
};

}

namespace Hmd2 {

struct MirroringOption {
    std::uint32_t mode;
    std::uint32_t reserved[7];
};

}

namespace Hmd2 {

struct GazeResultParameter {
    std::uint32_t size;
    std::int32_t handle;
    std::uint8_t reserved[8];
};

}

namespace Hmd2 {

struct GazeResult;

}

namespace Hmd2 {

struct FoveatedRenderingResult {
    std::uint32_t size;
    std::int32_t status;
    float direction[3];
    std::uint8_t reserved[4];
};

}

namespace Hmd2 {

struct GazeCalibrationData {
    void* data;
    std::int32_t size;
};

}

namespace Hmd2 {

struct GazeUserCalibrationData {
    const void* data;
    std::int32_t size;
};

}

namespace Hmd2 {

struct GazeUserCalibrationStatus {
    std::int32_t status;
    std::int32_t eye;
    float point[3];
};

}

namespace Hmd2 {

struct GazeRawData;

}

namespace Hmd2 {

struct GazeObjectSelectionTransform {
    float values[16];
};

}

namespace Hmd2 {

struct GazeObjectSelectionObject {
    std::uint64_t id;
    std::uint32_t enabled;
    std::uint32_t reserved12;
    GazeObjectSelectionTransform transform;
    const GazeObjectSelectionTransform* boundingBoxes;
    std::uint32_t boundingBoxCount;
    std::uint32_t reserved92;
};

}

namespace Hmd2 {

struct GazeObjectSelectionParameter {
    std::uint32_t size;
    std::uint32_t reserved4;
    std::uint64_t reserved8;
    std::uint64_t gazeTimestamp;
    GazeObjectSelectionTransform transform;
    std::uint32_t selectionMode;
    std::uint32_t filterMode;
    std::uint64_t timestamp;
    const GazeObjectSelectionObject* objects;
    std::uint32_t objectCount;
    std::uint32_t reserved116;
};

}

namespace Hmd2 {

struct GazeObjectSelectionResult {
    std::uint32_t size;
    std::uint32_t selected;
    std::uint64_t objectId;
    std::uint64_t gazeTimestamp;
    std::uint32_t gazeValid;
    float origin[3];
    float direction[3];
    float distance;
};

}

namespace Hmd2 {

struct GazeObjectSelectionInitializeParameter {
    std::uint32_t size;
    std::uint32_t maxObjects;
    void* memory;
    std::uint32_t memorySize;
    std::uint32_t reserved;
};

}

namespace Hmd2 {

struct GazeObjectSelectionMemoryParameter {
    std::uint32_t size;
    std::uint32_t maxObjects;
};

}

namespace Hmd2 {

struct GazeObjectSelectionMemoryInfo {
    std::uint32_t size;
    std::uint32_t memorySize;
    std::uint64_t alignment;
};

}

namespace Hmd2 {

struct CalibrationPoint {
    float x;
    float y;
    float z;
};

}

extern "C" {

int APS5_VABI sceHmd2ReprojectionGetStatus(Hmd2::ReprojectionStatus* status);
int APS5_VABI sceHmd2GazeGetResult(const Hmd2::GazeResultParameter* param, Hmd2::GazeResult* result);
int APS5_VABI sceHmd2GazeGetResultForFoveatedRendering(const Hmd2::GazeResultParameter* param, Hmd2::FoveatedRenderingResult* result);
int APS5_VABI sceHmd2ReprojectionBeginFrame(std::uint64_t frameId);
int APS5_VABI sceHmd2ReprojectionDisableMirroring();
int APS5_VABI sceHmd2ReprojectionDisableVrMode();
int APS5_VABI sceHmd2ReprojectionEnableMirroring(const Hmd2::MirroringParameter* parameter);
int APS5_VABI sceHmd2ReprojectionEnableVrMode(std::uint64_t mode);
int APS5_VABI sceHmd2ReprojectionGetPredictedDisplayTime(std::uint64_t frameId, std::uint64_t* time);
int APS5_VABI sceHmd2ReprojectionInitialize(const Hmd2::ReprojectionInitializeParameter* parameter, const void* reserved);
int APS5_VABI sceHmd2ReprojectionSetAllowPositionalReprojection(std::uint8_t mode);
int APS5_VABI sceHmd2ReprojectionSetMirroringOption(const Hmd2::MirroringOption* option);
int APS5_VABI sceHmd2ReprojectionSetParam(std::uint32_t bufferSetId, std::uint64_t frameId, const Hmd2::ReprojectionParameter* parameter, std::uint64_t reserved);
int APS5_VABI sceHmd2ReprojectionSetParamWithBuffer(std::uint32_t bufferSetId, std::uint64_t frameId, const Hmd2::ReprojectionBufferedParameter* parameter, std::uint64_t reserved);
int APS5_VABI sceHmd2ReprojectionSetRenderConfig(std::uint64_t flags, std::uint64_t reserved, std::uint64_t reserved2);
int APS5_VABI sceHmd2ReprojectionSetTiming(std::int32_t timing);
int APS5_VABI sceHmd2ReprojectionTerminate();
int APS5_VABI sceHmd2ReprojectionSeeThroughDraw();
int APS5_VABI sceHmd2ReprojectionSeeThroughIC();
int APS5_VABI sceHmd2ReprojectionVideoOutEq();
int APS5_VABI sceHmd2ReprojectionWaitShader();
int APS5_VABI sceHmd2SeeThroughCallbackGetPoseDataFunc();
int APS5_VABI sceHmd2ReprojectionClear();
int APS5_VABI sceHmd2ReprojectionSet();
int APS5_VABI sceHmd2ReprojectionSetEventGpuEnd(KernelEqueue queue, std::int32_t eventId);
int APS5_VABI sceHmd2ReprojectionSetEventGpuStart(KernelEqueue queue, std::int32_t eventId);
int APS5_VABI sceHmd2RpeorjectionSetParam();
std::int32_t APS5_VABI Hmd2BeforeInitializeZero();
int APS5_VABI sceHmd2SetIpdOffset(std::uint8_t enabled, float offset);
int APS5_VABI sceHmd2GazeActivateGazeTracking(std::int32_t mode);
std::int32_t APS5_VABI Hmd2BeforeInitializeNotInitialized();
std::int32_t APS5_VABI Hmd2BeforeInitializeError0004();
int APS5_VABI sceHmd2GazeStartUserCalibration();
int APS5_VABI sceHmd2GazeStopUserCalibration();
int APS5_VABI sceHmd2GazeSetUserCalibrationPoint(Hmd2::CalibrationPoint point);
int APS5_VABI sceHmd2GazeSetEnabledEye(std::int32_t eye);
std::int32_t APS5_VABI Hmd2BeforeInitializeParameterNull();
int APS5_VABI sceHmd2GazeGetCalibrationData(std::int32_t type, Hmd2::GazeCalibrationData* data);
int APS5_VABI sceHmd2GazeSetUserCalibrationData(const Hmd2::GazeUserCalibrationData* data);
int APS5_VABI sceHmd2GazeObjectSelectionInitialize(const Hmd2::GazeObjectSelectionInitializeParameter* parameter);
std::int32_t APS5_VABI Hmd2BeforeInitializeParameterInvalid();
std::int32_t APS5_VABI Hmd2BeforeInitializeReprojectionNotInitialized();
int APS5_VABI sceHmd2ReprojectionSetUserEventStart(KernelEqueue queue, std::int32_t eventId);
int APS5_VABI sceHmd2ReprojectionSetUserEventEnd(KernelEqueue queue, std::int32_t eventId);
int APS5_VABI sceHmd2ReprojectionClearUserEventStart();
int APS5_VABI sceHmd2ReprojectionClearUserEventEnd();
int APS5_VABI sceHmd2ReprojectionGetCurrentOutput(Hmd2::ReprojectionOutput* output);
std::int32_t APS5_VABI Hmd2BeforeInitializeError002C();
int APS5_VABI sceHmd2GazeObjectSelectionTerminate();
std::int32_t APS5_VABI Hmd2BeforeInitializeError1002();
std::int32_t APS5_VABI Hmd2BeforeInitializeError1003();
std::int32_t APS5_VABI Hmd2BeforeInitializeMinusOne();
std::int32_t APS5_VABI Hmd2BeforeInitializeValue0400();
std::int32_t APS5_VABI Hmd2BeforeInitializeValue20000();
int APS5_VABI sceHmd2InternalSetPortStatus(void);
int APS5_VABI sceHmd2InternalLedDetectorStartBySlot(void);
int APS5_VABI sceHmd2InternalImageStartBySlot(void);
int APS5_VABI sceHmd2InternalSetPortConnection(void);
int APS5_VABI sceHmd2InternalSetCameraSeeThroughMode(void);
int APS5_VABI sceHmd2InternalSetFakeLensSeparationDistance(void);
int APS5_VABI sceHmd2InternalDfuSend(void);
int APS5_VABI sceHmd2InternalLedDetectorOpenBySlot(void);
int APS5_VABI sceHmd2InternalImageOpenBySlot(void);
int APS5_VABI sceHmd2InternalLedDetectorStopBySlot(void);
int APS5_VABI sceHmd2InternalSetDeviceConnection(void);
int APS5_VABI sceHmd2InternalLedDetectorCloseBySlot(void);
int APS5_VABI sceHmd2InternalSetSidetone(void);
int APS5_VABI sceHmd2InternalGetDeviceId(void);
int APS5_VABI sceHmd2InternalSetBrightness(void);
int APS5_VABI sceHmd2InternalImageStopBySlot(void);
int APS5_VABI sceHmd2InternalSetVibrationMode(void);
int APS5_VABI sceHmd2InternalGetVr2dData(void);
int APS5_VABI sceHmd2InternalImageCloseBySlot(void);
std::int32_t APS5_VABI Hmd2CheckFirstThenError0004(const void* a0);
int APS5_VABI sceHmd2GazeGetDeviceStatus(std::int32_t* status);
int APS5_VABI sceHmd2GazeGetUserCalibrationStatus(Hmd2::GazeUserCalibrationStatus* status);
std::int32_t APS5_VABI Hmd2CheckFirstThenZero(const void* a0);
int APS5_VABI sceHmd2SetFieldOfView(const Hmd2::FieldOfView* fieldOfView);
std::int32_t APS5_VABI Hmd2CheckFirstThenError1002(const void* a0);
std::int32_t APS5_VABI Hmd2CheckFirstThenMinusOne(const void* a0);
std::int32_t APS5_VABI Hmd2CheckFirstTwoThenParameterInvalid(const void* a0, const void* a1);
int APS5_VABI sceHmd2GazeObjectSelectionQueryMemory(const Hmd2::GazeObjectSelectionMemoryParameter* parameter, Hmd2::GazeObjectSelectionMemoryInfo* info);
int APS5_VABI sceHmd2GazeObjectSelectionGetSelectedObject(const Hmd2::GazeObjectSelectionParameter* parameter, Hmd2::GazeObjectSelectionResult* result);
std::int32_t APS5_VABI Hmd2CheckSecondThenError002C(const void* a0, const void* a1);
std::int32_t APS5_VABI Hmd2CheckFirstThenParameterInvalid(const void* a0);
std::int32_t APS5_VABI Hmd2CheckAllSixThenZero(const void* a0, const void* a1, const void* a2, const void* a3, const void* a4, const void* a5);
std::int32_t APS5_VABI Hmd2CheckSecondThenParameterInvalid(const void* a0, const void* a1);
int APS5_VABI sceHmd2GazeGetCalibrationDataMaxSize(std::int32_t type, std::uint32_t* size);
std::int32_t APS5_VABI Hmd2CheckFirstThenError800d0203(const void* a0);
std::int32_t APS5_VABI Hmd2CheckFirstThenError1003(const void* a0);
std::int32_t APS5_VABI Hmd2CheckFirstThenError1010(const void* a0);
std::int32_t APS5_VABI Hmd2CheckFirstTwoThenError1002(const void* a0, const void* a1);
std::int32_t APS5_VABI Hmd2CheckFirstTwoThenError1003(const void* a0, const void* a1);
std::int32_t APS5_VABI Hmd2CheckFirstAndThirdThenZero(const void* a0, const void* a1, const void* a2);
std::int32_t APS5_VABI Hmd2CheckFirstAndThirdThenError1003(const void* a0, const void* a1, const void* a2);
std::int32_t APS5_VABI Hmd2CheckSecondThenInvalidHandle(const void* a0, const void* a1);
int APS5_VABI sceHmd2GazeInsertRawDataWithHandle(std::int32_t handle, Hmd2::GazeRawData* data);
std::int32_t APS5_VABI Hmd2CheckSecondThenError1002(const void* a0, const void* a1);
std::int32_t APS5_VABI Hmd2WriteStateTwo(void* a0);
std::int32_t APS5_VABI Hmd2WriteSize0x2000Align0x100(std::uint64_t* a0, std::uint64_t* a1);
std::int32_t APS5_VABI Hmd2WriteFourConstants(void* a0);
std::int32_t APS5_VABI Hmd2WriteSixHalvesThenInvalidHandle(std::uint64_t a0, void* a1);
std::int32_t APS5_VABI Hmd2WriteTwelveHalvesThenInvalidHandle(std::uint64_t a0, void* a1);
std::int32_t APS5_VABI Hmd2SlotUpTo3CheckSecondThirdThenZero(std::uint64_t a0, const void* a1, const void* a2);
std::int32_t APS5_VABI Hmd2SlotUpTo1ThenError1002(std::uint64_t a0);
std::int32_t APS5_VABI Hmd2SlotUpTo1CheckSecondThenError1002(std::uint64_t a0, const void* a1);
std::int32_t APS5_VABI Hmd2SlotUpTo1ThenMinusOne(std::uint64_t a0);
std::int32_t APS5_VABI Hmd2SlotUpTo2ThenMinusOne(std::uint64_t a0);
std::int32_t APS5_VABI Hmd2SlotUpTo2ThenError1002(std::uint64_t a0);
std::int32_t APS5_VABI Hmd2SlotUpTo2ThenError1003(std::uint64_t a0);
std::int32_t APS5_VABI Hmd2SlotUpTo2CheckSecondThenError002C(std::uint64_t a0, const void* a1);
std::int32_t APS5_VABI Hmd2IndexZeroCheckSecondThenZero(std::uint64_t a0, const void* a1);

}
