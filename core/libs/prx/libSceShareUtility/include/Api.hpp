#pragma once

#include "SceTypes.hpp"
#include <cstdint>

extern "C" {

int APS5_VABI sceShareUtilityAdvanceFromFileWithContentIdList();
int APS5_VABI sceShareUtilityAdvanceFromFileWithTitleIdList();
int APS5_VABI sceShareUtilityAdvanceGetServiceInfo();
int APS5_VABI sceShareUtilityAdvanceInitialize(std::uint64_t argument1, std::uint64_t argument2);
int APS5_VABI sceShareUtilityAdvanceQueryServiceInfo();
int APS5_VABI sceShareUtilityAdvanceQueryServiceInfoA();
int APS5_VABI sceShareUtilityAdvanceResetPlayingContent();
int APS5_VABI sceShareUtilityAdvanceSetPlayingContent(std::int64_t argument1);
int APS5_VABI sceShareUtilityAdvanceSetUploadContentData(const void* argument1, std::uint64_t argument2);
int APS5_VABI sceShareUtilityAdvanceTerminate();
int APS5_VABI sceShareUtilityAdvanceValidateContents();
int APS5_VABI sceShareUtilityAdvanceWakeUpShareMenu(std::int64_t argument1, std::uint32_t argument2);
int APS5_VABI sceShareUtilityAdvanceWakeUpShareMenuBroadcast();
int APS5_VABI sceShareUtilityAdvanceWakeUpShareMenuDefault();
int APS5_VABI sceShareUtilityAdvanceWakeUpShareMenuForShareContent(const void* argument1, std::uint64_t argument2);
int APS5_VABI sceShareUtilityInitialize(std::uint64_t argument1, std::uint64_t argument2);
int APS5_VABI sceShareUtilityInitializeEx2(std::uint64_t argument1, std::uint64_t argument2);
int APS5_VABI sceShareUtilityOpenShareMenu(std::int64_t argument1, std::uint32_t argument2);
int APS5_VABI sceShareUtilityOpenShareMenuBroadcast();
int APS5_VABI sceShareUtilityOpenShareMenuDefault();
int APS5_VABI sceShareUtilityOpenShareMenuForShareContent(const void* argument1, std::uint64_t argument2);
int APS5_VABI sceShareUtilityTerminate();

}
