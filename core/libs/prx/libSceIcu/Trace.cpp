// Derived from icu4c/source/common/utrace.cpp@84e1f26ea77152936e70d53178a816dbfbf69989 (ICU 68.2): the trace
// entry and exit forwarding, the function and level registry below.
// https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/common/utrace.cpp
//
// The upstream file's notice, verbatim:
//   © 2016 and later: Unicode, Inc. and others.
//   License & terms of use: http://www.unicode.org/copyright.html
//   Copyright (C) 1997-2016, International Business Machines Corporation and others. All Rights Reserved.

#include "prx/libSceIcu/Trace.hpp"
#include <cstdarg>
#include <cstdint>

namespace {

UTraceEntry EntryFunction = nullptr;
UTraceExit ExitFunction = nullptr;
UTraceData DataFunction = nullptr;
const void* TraceContext = nullptr;
std::int32_t TraceLevel = 8;

constexpr std::int32_t TraceOff = -1;
constexpr std::int32_t TraceVerbose = 9;
constexpr std::int32_t ExitValueInt32 = 1;
constexpr std::int32_t ExitValueStatus = 2;
constexpr std::int32_t ExitValuePointer = 4;

const char ExitFormat[] = "Returns.";
const char ExitFormatValue[] = "Returns %d.";
const char ExitFormatStatus[] = "Returns.  Status = %d.";
const char ExitFormatValueStatus[] = "Returns %d.  Status = %d.";
const char ExitFormatPointerStatus[] = "Returns %d.  Status = %p.";

}

extern "C" {

void APS5_VABI utrace_entry_nid_postfix(std::int32_t fnNumber) {
    if (EntryFunction != nullptr) EntryFunction(TraceContext, fnNumber);
}

void APS5_VABI utrace_exit_nid_postfix(std::int32_t fnNumber, std::int32_t returnType, ...) {
    if (ExitFunction == nullptr) return;
    const char* format = ExitFormat;
    switch (returnType) {
    case ExitValueInt32:
        format = ExitFormatValue;
        break;
    case ExitValueStatus:
        format = ExitFormatStatus;
        break;
    case ExitValueInt32 | ExitValueStatus:
        format = ExitFormatValueStatus;
        break;
    case ExitValuePointer | ExitValueStatus:
        format = ExitFormatPointerStatus;
        break;
    default:
        return;
    }
#ifdef _WIN32
    __builtin_sysv_va_list arguments;
    __builtin_sysv_va_start(arguments, returnType);
    ExitFunction(TraceContext, fnNumber, format, arguments);
    __builtin_sysv_va_end(arguments);
#else
    va_list arguments;
    va_start(arguments, returnType);
    ExitFunction(TraceContext, fnNumber, format, arguments);
    va_end(arguments);
#endif
}

void APS5_VABI utrace_data_nid_postfix(std::int32_t fnNumber, std::int32_t level, const char* format, ...) {
    if (DataFunction == nullptr || format == nullptr) return;
#ifdef _WIN32
    __builtin_sysv_va_list arguments;
    __builtin_sysv_va_start(arguments, format);
    DataFunction(TraceContext, fnNumber, level, format, arguments);
    __builtin_sysv_va_end(arguments);
#else
    va_list arguments;
    va_start(arguments, format);
    DataFunction(TraceContext, fnNumber, level, format, arguments);
    va_end(arguments);
#endif
}

void APS5_VABI utrace_setFunctions_nid_postfix(const void* context, UTraceEntry entry, UTraceExit exit, UTraceData data) {
    EntryFunction = entry;
    ExitFunction = exit;
    DataFunction = data;
    TraceContext = context;
}

void APS5_VABI utrace_getFunctions_nid_postfix(const void** context, UTraceEntry* entry, UTraceExit* exit,
    UTraceData* data) {
    if (entry != nullptr) *entry = EntryFunction;
    if (exit != nullptr) *exit = ExitFunction;
    if (data != nullptr) *data = DataFunction;
    if (context != nullptr) *context = TraceContext;
}

void APS5_VABI utrace_setLevel_nid_postfix(std::int32_t level) {
    if (level < TraceOff) level = TraceOff;
    if (level > TraceVerbose) level = TraceVerbose;
    TraceLevel = level;
}

std::int32_t APS5_VABI utrace_getLevel_nid_postfix() {
    return TraceLevel;
}

}
