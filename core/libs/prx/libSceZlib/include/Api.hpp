#pragma once

#include "SceTypes.hpp"

extern "C" {

std::int32_t APS5_VABI sceZlibInitialize(const void* buffer, std::uint64_t length);
std::int32_t APS5_VABI sceZlibFinalize();
std::int32_t APS5_VABI sceZlibInflate(const void* source, std::uint32_t sourceLength, void* destination, std::uint32_t destinationLength, std::uint64_t* requestId);
std::int32_t APS5_VABI sceZlibWaitForDone(std::uint64_t* requestId, std::uint32_t* timeout);
std::int32_t APS5_VABI sceZlibGetResult(std::uint64_t requestId, std::uint32_t* destinationLength, std::int32_t* status);

}
