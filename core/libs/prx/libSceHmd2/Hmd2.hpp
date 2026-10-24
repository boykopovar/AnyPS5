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
inline constexpr std::int32_t Error002C = static_cast<std::int32_t>(0x8a72002c);
inline constexpr std::int32_t Error1002 = static_cast<std::int32_t>(0x8a721002);
inline constexpr std::int32_t Error1003 = static_cast<std::int32_t>(0x8a721003);
inline constexpr std::int32_t Error1010 = static_cast<std::int32_t>(0x8a721010);
inline constexpr std::int32_t Error800d0203 = static_cast<std::int32_t>(0x800d0203);
inline constexpr std::int32_t Error100F = static_cast<std::int32_t>(0x8a72100f);

inline constexpr std::uint32_t UserIdSdkVersion = 0x07000000;

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
