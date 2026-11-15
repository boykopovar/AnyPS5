#pragma once

#include <cstdint>
#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceAjmGetFailedInstance_nid_disambig1(uint32_t context, void* failed);
int APS5_VABI sceAjmInstanceGetSize_nid_disambig1(uint32_t context, uint32_t codec, uint64_t flags);
int APS5_VABI sceAjmTelemetryGetLog_nid_disambig1(uint32_t context, uint8_t* flag, void* log);

}
