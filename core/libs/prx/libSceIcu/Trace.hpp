#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

extern "C" {

using UTraceEntry = void (APS5_VABI*)(const void* context, std::int32_t fnNumber);
using UTraceExit = void (APS5_VABI*)(const void* context, std::int32_t fnNumber, const char* format, ...);
using UTraceData = void (APS5_VABI*)(const void* context, std::int32_t fnNumber, std::int32_t level,
                                     const char* format, ...);

void APS5_VABI utrace_entry_nid_postfix(std::int32_t fnNumber);
void APS5_VABI utrace_exit_nid_postfix(std::int32_t fnNumber, std::int32_t returnType, ...);
void APS5_VABI utrace_data_nid_postfix(std::int32_t fnNumber, std::int32_t level, const char* format, ...);
void APS5_VABI utrace_setFunctions_nid_postfix(const void* context, UTraceEntry entry, UTraceExit exit, UTraceData data);
void APS5_VABI utrace_getFunctions_nid_postfix(const void** context, UTraceEntry* entry, UTraceExit* exit,
                                               UTraceData* data);
void APS5_VABI utrace_setLevel_nid_postfix(std::int32_t level);
std::int32_t APS5_VABI utrace_getLevel_nid_postfix();

}
