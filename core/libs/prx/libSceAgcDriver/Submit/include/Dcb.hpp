#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_SUBMIT_INCLUDE_DCB_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_SUBMIT_INCLUDE_DCB_HPP

#include "SceTypes.hpp"

extern "C" int APS5_VABI sceAgcDriverSubmitDcb(const Packet* packet);
extern "C" int APS5_VABI sceAgcDriverAgrSubmitDcb(const Packet* packet);
extern "C" int APS5_VABI sceAgcDriverSubmitMultiDcbs(std::uint32_t* const* dcbGpuAddrs, const std::uint32_t* dcbSizesInDwords, std::uint32_t count);
extern "C" int APS5_VABI sceAgcDriverAgrSubmitMultiDcbs(std::uint32_t* const* dcbGpuAddrs, const std::uint32_t* dcbSizesInDwords, std::uint32_t count);

#endif
