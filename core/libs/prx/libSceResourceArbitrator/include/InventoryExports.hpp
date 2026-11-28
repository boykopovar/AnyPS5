#pragma once

#include <cstdint>
#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceResourceArbitratorChangeState(std::uint32_t id, const char* name, void* arg, std::uint32_t flags);
int APS5_VABI sceResourceArbitratorClearCallback(std::uint32_t id);
int APS5_VABI sceResourceArbitratorGetFeatureIdList(void* outList);
int APS5_VABI sceResourceArbitratorIsExecutable(std::uint32_t id, const char* name);
int APS5_VABI sceResourceArbitratorSetCallback(std::uint32_t id, void* callback, void* userArg);
int APS5_VABI libSceResourceArbitratorUnknown_3xRoVp7fPPY_nid_no_patch();
int APS5_VABI sceResourceArbitratorPostTelemetry(std::uint32_t id, std::uint32_t type, const char* data, std::uint32_t size);
int APS5_VABI libSceResourceArbitratorUnknown_OM6SXhDZdiU_nid_no_patch(std::uint32_t value, std::uint32_t id);
int APS5_VABI libSceResourceArbitratorUnknown_TYiIrKTzQ8s_nid_no_patch(std::uint8_t flag);
int APS5_VABI libSceResourceArbitratorUnknown_cT1vA0DTMcs_nid_no_patch(const void* featureList, const char* featureId);
int APS5_VABI libSceResourceArbitratorUnknown_jzKwHzhEBIQ_nid_no_patch(std::uint32_t id);
int APS5_VABI sceResourceArbitratorSendDebugFunction(std::uint32_t command);
int APS5_VABI libSceResourceArbitratorUnknown_rMJnXXj6GIo_nid_no_patch(std::int32_t id, std::int32_t* outIndex, std::int32_t* outValue);
int APS5_VABI libSceResourceArbitratorUnknown_ru9qkt9UCLQ_nid_no_patch(std::uint32_t id, void* outBuffer);

}
