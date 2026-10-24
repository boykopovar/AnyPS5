#ifndef CORE_LIBS_PRX_LIBSCEHMD2_HMD2_HPP
#define CORE_LIBS_PRX_LIBSCEHMD2_HMD2_HPP

#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstddef>
#include <cstdint>

namespace Hmd2 {

inline constexpr std::int32_t AlreadyInitialized = static_cast<std::int32_t>(0x8a720001);
inline constexpr std::int32_t NotInitialized = static_cast<std::int32_t>(0x8a720002);
inline constexpr std::int32_t InvalidHandle = static_cast<std::int32_t>(0x8a720003);
inline constexpr std::int32_t AlreadyOpened = static_cast<std::int32_t>(0x8a720006);
inline constexpr std::int32_t ParameterNull = static_cast<std::int32_t>(0x8a720008);
inline constexpr std::int32_t ParameterInvalid = static_cast<std::int32_t>(0x8a720009);
inline constexpr std::int32_t Error0004 = static_cast<std::int32_t>(0x8a720004);
inline constexpr std::int32_t ReprojectionNotInitialized = static_cast<std::int32_t>(0x8a72000b);
inline constexpr std::int32_t ReprojectionAlreadyInitialized = static_cast<std::int32_t>(0x8a72000c);
inline constexpr std::int32_t VideoOutInvalidValue = static_cast<std::int32_t>(0x80290001);
inline constexpr std::int32_t VideoOutNoDevice = static_cast<std::int32_t>(0x80290018);
inline constexpr std::int32_t Error002C = static_cast<std::int32_t>(0x8a72002c);
inline constexpr std::int32_t Error1002 = static_cast<std::int32_t>(0x8a721002);
inline constexpr std::int32_t Error1003 = static_cast<std::int32_t>(0x8a721003);
inline constexpr std::int32_t Error1010 = static_cast<std::int32_t>(0x8a721010);
inline constexpr std::int32_t Error800d0203 = static_cast<std::int32_t>(0x800d0203);
inline constexpr std::int32_t Error100F = static_cast<std::int32_t>(0x8a72100f);

inline constexpr std::uint32_t UserIdSdkVersion = 0x07000000;

inline constexpr std::uint64_t OutputMode89_91Hz = 0x2000c;
inline constexpr std::uint64_t OutputMode119_88Hz = 0x2000f;
inline constexpr std::int32_t ReprojectionTimingMin = 2000;
inline constexpr std::int32_t ReprojectionTimingMax = 7000;
inline constexpr std::uint32_t ReprojectionTimingDefault = 3000;

inline constexpr std::uint64_t ReprojectionBufferSize = 0x100828;
inline constexpr std::uint64_t ReprojectionBufferAlign = 0x100;
inline constexpr std::int32_t AllowPositionalReprojectionMax = 5;
inline constexpr std::uint64_t ReprojectionPositionalBufferSize = 0x1103000;
inline constexpr std::uint64_t ReprojectionPositionalBufferSizeStep = 0x1000000;
inline constexpr std::uint64_t ReprojectionPositionalBufferAlign = 0x1000;

enum class DeviceStatus : std::uint32_t {
    NotDetected = 2,
};

struct DeviceInformation {
    DeviceStatus status;
    std::int32_t userId;
    std::uint32_t panelWidth;
    std::uint32_t panelHeight;
    std::uint16_t latency90Hz;
    std::uint16_t latency120Hz;
    std::uint8_t hmuMount;
    std::uint8_t lensSeparationDistance;
    std::uint8_t reserved[2];
    float virtualImageDistance;
};

struct FieldOfView {
    float tanOut;
    float tanIn;
    float tanTop;
    float tanBottom;
};

static_assert(sizeof(DeviceInformation) == 28);
static_assert(offsetof(DeviceInformation, userId) == 4);
static_assert(offsetof(DeviceInformation, panelWidth) == 8);
static_assert(offsetof(DeviceInformation, latency90Hz) == 16);
static_assert(offsetof(DeviceInformation, hmuMount) == 20);
static_assert(offsetof(DeviceInformation, virtualImageDistance) == 24);
static_assert(sizeof(FieldOfView) == 16);

struct ReprojectionState {
    std::uint64_t outputMode;
    std::uint32_t timingUs;
};

static_assert(sizeof(ReprojectionState) == 16);
static_assert(alignof(ReprojectionState) == 8);
static_assert(offsetof(ReprojectionState, timingUs) == 8);

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

static_assert(sizeof(ReprojectionInitializeParameter) == 72);
static_assert(alignof(ReprojectionInitializeParameter) == 8);
static_assert(offsetof(ReprojectionInitializeParameter, threadPriority) == 16);
static_assert(offsetof(ReprojectionInitializeParameter, threadAffinity) == 20);
static_assert(offsetof(ReprojectionInitializeParameter, renderingMode) == 32);
static_assert(offsetof(ReprojectionInitializeParameter, auxiliaryMemory) == 40);
static_assert(offsetof(ReprojectionInitializeParameter, timingInterval) == 48);

struct SizeAlign {
    std::uint64_t size;
    std::uint64_t align;

    constexpr SizeAlign(std::uint64_t size = 0, std::uint64_t align = 0) noexcept : size(size), align(align) {}
};

static_assert(sizeof(SizeAlign) == 16);
static_assert(offsetof(SizeAlign, size) == 0);
static_assert(offsetof(SizeAlign, align) == 8);

}

extern "C" {
std::int32_t APS5_VABI sceHmd2Initialize(const void* param);
std::int32_t APS5_VABI sceHmd2Terminate();
std::int32_t APS5_VABI sceHmd2Open(std::int32_t userId, std::int32_t type, std::int32_t index, const void* param);
std::int32_t APS5_VABI sceHmd2Close(std::int32_t handle);
std::int32_t APS5_VABI sceHmd2GetDeviceInformation(Hmd2::DeviceInformation* info);
std::int32_t APS5_VABI sceHmd2GetDeviceInformationByHandle(std::int32_t handle, Hmd2::DeviceInformation* info);
std::int32_t APS5_VABI sceHmd2GetFieldOfView(std::int32_t handle, Hmd2::FieldOfView* fieldOfView);
std::int32_t APS5_VABI sceHmd2GetFieldOfViewWithoutHandle(Hmd2::FieldOfView* fieldOfView);
std::int32_t APS5_VABI sceHmd2SetVibration(std::int32_t handle, std::uint32_t level);
Hmd2::SizeAlign APS5_VABI sceHmd2ReprojectionQueryBufferSizeAlign();
Hmd2::SizeAlign APS5_VABI sceHmd2ReprojectionQueryDisplayBufferSizeAlign();
Hmd2::SizeAlign APS5_VABI sceHmd2ReprojectionQuerySeeThroughBufferSizeAlign();
Hmd2::SizeAlign APS5_VABI sceHmd2ReprojectionGetMirroringDisplayBufferSizeAlign();
Hmd2::SizeAlign APS5_VABI sceHmd2ReprojectionGetMirroringWorkMemorySizeAlign();
}

#endif
