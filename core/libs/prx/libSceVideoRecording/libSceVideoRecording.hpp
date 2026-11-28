#pragma once

#include <cstddef>
#include <cstdint>
#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceVideoRecordingSetInfo_nid_disambig1(std::int32_t info, const void* data, std::size_t size);
int APS5_VABI sceVideoRecordingClose_nid_disambig1(int discard);
int APS5_VABI sceVideoRecordingGetStatus_nid_disambig1(void);
int APS5_VABI sceVideoRecordingOpen2_nid_disambig1(const char* path, const void* param, void* heap, int heapSize);
int APS5_VABI sceVideoRecordingQueryMemSize2_nid_disambig1(const void* param);
int APS5_VABI sceVideoRecordingStart_nid_disambig1(void);
int APS5_VABI sceVideoRecordingStop_nid_disambig1(void);

}
