// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdarg>
#include <cstdint>

namespace Icu {

#ifdef _WIN32
using TraceArguments = __builtin_sysv_va_list;
#else
using TraceArguments = std::va_list;
#endif

using TraceEntry = void (APS5_VABI*)(const void*, std::int32_t);
using TraceExit = void (APS5_VABI*)(const void*, std::int32_t, const char*, TraceArguments);
using TraceData = void (APS5_VABI*)(const void*, std::int32_t, std::int32_t, const char*, TraceArguments);

}

extern "C" {
void APS5_VABI utrace_entry_nid_postfix(std::int32_t);
void APS5_VABI utrace_exit_nid_postfix(std::int32_t, std::int32_t, ...);
void APS5_VABI utrace_data_nid_postfix(std::int32_t, std::int32_t, const char*, ...);
std::int32_t APS5_VABI utrace_format_nid_postfix(char*, std::int32_t, std::int32_t, const char*, ...);
std::int32_t APS5_VABI utrace_vformat_nid_postfix(char*, std::int32_t, std::int32_t, const char*, Icu::TraceArguments);
void APS5_VABI utrace_setFunctions_nid_postfix(const void*, Icu::TraceEntry, Icu::TraceExit, Icu::TraceData);
void APS5_VABI utrace_getFunctions_nid_postfix(const void**, Icu::TraceEntry*, Icu::TraceExit*, Icu::TraceData*);
void APS5_VABI utrace_setLevel_nid_postfix(std::int32_t);
std::int32_t APS5_VABI utrace_getLevel_nid_postfix();
const char* APS5_VABI utrace_functionName_nid_postfix(std::int32_t);
}
