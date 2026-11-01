#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_SUBMIT_INCLUDE_DCB_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_SUBMIT_INCLUDE_DCB_HPP

#include "SceTypes.hpp"
#include <cstdint>

namespace AgcDriver {

int SubmitMany(std::uint32_t* const* addresses, const std::uint32_t* sizes, std::uint32_t count, std::uint32_t queue);

}

extern "C" std::uint32_t APS5_VABI sceAgcDriverGetWaitRenderingPacketSizeInDwords();
extern "C" std::uint32_t APS5_VABI sceAgcDriverGetSetFlipPacketSizeInDwords();

extern "C" int APS5_VABI sceAgcDriverSubmitDcb(const Packet* packet);
extern "C" int APS5_VABI sceAgcDriverAgrSubmitDcb(const Packet* packet);
extern "C" int APS5_VABI sceAgcDriverSubmitMultiDcbs(std::uint32_t* const* dcbGpuAddrs, const std::uint32_t* dcbSizesInDwords, std::uint32_t count);
extern "C" int APS5_VABI sceAgcDriverAgrSubmitMultiDcbs(std::uint32_t* const* dcbGpuAddrs, const std::uint32_t* dcbSizesInDwords, std::uint32_t count);

extern "C" {

std::uint32_t APS5_VABI sceAgcDriverGetSetWorkloadsActivePacketSize();
std::uint32_t APS5_VABI sceAgcDriverGetSetWorkloadCompletePacketSize();
std::uint32_t APS5_VABI sceAgcDriverGetSetWorkloadStreamInactivePacketSize();
std::uint32_t APS5_VABI sceAgcDriverNopDataGetPacketSize(std::uint32_t bytes);
std::uint32_t APS5_VABI sceAgcDriverUserDataGetPacketSize(std::uint32_t bytes);
std::uint32_t APS5_VABI sceAgcDriverWaitUntilSafeForRendering(std::uint32_t** command, std::uint32_t capacity, std::uint32_t mode, std::uint32_t handle, int index);

}

#endif
