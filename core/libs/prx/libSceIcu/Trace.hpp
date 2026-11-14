#pragma once
#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

using UTraceEntry = void (APS5_VABI*)(const void*, std::int32_t);
using UTraceExit = void (APS5_VABI*)(const void*, std::int32_t, const char*, VaList*);
using UTraceData = void (APS5_VABI*)(const void*, std::int32_t, std::int32_t, const char*, VaList*);

extern "C" {
void IcuTraceCleanup_nid_no_patch();
void APS5_VABI utrace_setLevel_nid_postfix(std::int32_t);
std::int32_t APS5_VABI utrace_getLevel_nid_postfix();
void APS5_VABI utrace_setFunctions_nid_postfix(const void*, UTraceEntry, UTraceExit, UTraceData);
void APS5_VABI utrace_getFunctions_nid_postfix(const void**, UTraceEntry*, UTraceExit*, UTraceData*);
void APS5_VABI utrace_entry_nid_postfix(std::int32_t);
void APS5_VABI utrace_exit_nid_postfix(std::int32_t, std::int32_t, ...);
void APS5_VABI utrace_data_nid_postfix(std::int32_t, std::int32_t, const char*, ...);
std::int32_t APS5_VABI utrace_vformat_nid_postfix(char*, std::int32_t, std::int32_t, const char*, VaList*);
std::int32_t APS5_VABI utrace_format_nid_postfix(char*, std::int32_t, std::int32_t, const char*, ...);
const char* APS5_VABI utrace_functionName_nid_postfix(std::int32_t);
}
