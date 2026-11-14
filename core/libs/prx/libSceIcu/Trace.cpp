// Derived from icu4c/source/common/utrace.cpp@84e1f26ea77152936e70d53178a816dbfbf69989 (ICU 68.2):
// https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/common/utrace.cpp
//
// The upstream file's notice, verbatim:
//   © 2016 and later: Unicode, Inc. and others.
//   License & terms of use: http://www.unicode.org/copyright.html
//   Copyright (C) 2003-2014, International Business Machines
//   Corporation and others.  All Rights Reserved.

#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/WindowsFormatting.hpp"
#include <cstdarg>
#include <cstdint>
#include <optional>

#ifdef _WIN32
#define APS5_TRACE_VA_LIST __builtin_sysv_va_list
#define APS5_TRACE_VA_START(list, last) __builtin_sysv_va_start(list, last)
#define APS5_TRACE_VA_END(list) __builtin_sysv_va_end(list)
#else
#define APS5_TRACE_VA_LIST std::va_list
#define APS5_TRACE_VA_START(list, last) va_start(list, last)
#define APS5_TRACE_VA_END(list) va_end(list)
#endif

namespace {

constexpr std::int32_t TraceOff = -1;
constexpr std::int32_t TraceError = 0;
constexpr std::int32_t TraceVerbose = 9;

constexpr std::int32_t ExitI32 = 1;
constexpr std::int32_t ExitPtr = 2;
constexpr std::int32_t ExitStatus = 0x10;

constexpr std::int32_t FunctionStart = 0;
constexpr std::int32_t FunctionLimit = 2;
constexpr std::int32_t ConversionStart = 0x1000;
constexpr std::int32_t ConversionLimit = 0x1008;
constexpr std::int32_t CollationStart = 0x2000;
constexpr std::int32_t CollationLimit = 0x2009;
constexpr std::int32_t DataStart = 0x3000;
constexpr std::int32_t DataLimit = 0x3004;

UTraceEntry traceEntry = nullptr;
UTraceExit traceExit = nullptr;
UTraceData traceData = nullptr;
const void* traceContext = nullptr;
std::int32_t traceLevel = TraceError;

const char* const FunctionNames[] = {"u_init", "u_cleanup"};

const char* const ConversionNames[] = {
    "ucnv_open", "ucnv_openPackage", "ucnv_openAlgorithmic", "ucnv_clone",
    "ucnv_close", "ucnv_flushCache", "ucnv_load", "ucnv_unload",
};

const char* const CollationNames[] = {
    "ucol_open", "ucol_close", "ucol_strcoll", "ucol_getSortKey", "ucol_getLocale",
    "ucol_nextSortKeyPart", "ucol_strcollIter", "ucol_openFromShortString", "ucol_strcollUTF8",
};

const char* const DataNames[] = {"resc", "bundle-open", "file-open", "res-open"};

class Output {
    char* buffer;
    std::int32_t capacity;
    std::int32_t indent;

public:
    std::int32_t index = 0;

    Output(char* destination, std::int32_t size, std::int32_t spaces) : buffer(destination), capacity(size), indent(spaces) {}

    void Char(char c, bool indented = true) {
        const std::int32_t spaces = indented ? indent : 0;
        if (index == 0 || (c != '\n' && c != 0 && index < capacity && buffer[index - 1] == '\n') || (c == '\n' && index >= capacity)) {
            for (std::int32_t i = 0; i < spaces; ++i) {
                if (index < capacity) buffer[index] = ' ';
                ++index;
            }
        }
        if (index < capacity) buffer[index] = c;
        if (c != 0) ++index;
    }

    void Hex(std::int64_t value, std::int32_t digits) {
        static constexpr char HexDigits[] = "0123456789abcdef";
        for (std::int32_t shift = (digits - 1) * 4; shift >= 0; shift -= 4) Char(HexDigits[(value >> shift) & 0xf], false);
    }

    void Pointer(const void* value) {
        Hex(static_cast<std::int64_t>(reinterpret_cast<std::uintptr_t>(value)), 16);
    }

    void String(const char* text) {
        if (!text) text = "*NULL*";
        char c;
        do {
            c = *text++;
            Char(c);
        } while (c != 0);
    }

    void UString(const char16_t* text, std::int32_t length) {
        if (!text) {
            String(nullptr);
            return;
        }
        for (std::int32_t i = 0; i < length || length == -1; ++i) {
            const char16_t c = text[i];
            Hex(c, 4);
            Char(' ');
            if (length == -1 && c == 0) break;
        }
    }
};

class Arguments {
    VaList* source;
    std::optional<LibcDetail::FormatArguments> list;

public:
    explicit Arguments(VaList* arguments) : source(arguments) {}

    template<class T> T Next() {
        if (!list) {
            if (!source) APS5_INVALID_ARG_EX;
            list.emplace(source);
        }
        return list->Next<T>();
    }
};

void Vector(Output& out, char type, Arguments& args, std::int64_t& value) {
    const auto* bytes = args.Next<const char*>();
    const std::int32_t length = args.Next<std::int32_t>();
    if (!bytes) {
        out.String("*NULL* ");
    } else {
        const auto* halves = reinterpret_cast<const std::int16_t*>(bytes);
        const auto* words = reinterpret_cast<const std::int32_t*>(bytes);
        const auto* longs = reinterpret_cast<const std::int64_t*>(bytes);
        const auto* pointers = reinterpret_cast<const void* const*>(bytes);
        std::int32_t digits = 0;
        for (std::int32_t i = 0; i < length || length == -1; ++i) {
            switch (type) {
            case 'b':
                digits = 2;
                value = *bytes++;
                break;
            case 'h':
                digits = 4;
                value = *halves++;
                break;
            case 'd':
                digits = 8;
                value = *words++;
                break;
            case 'l':
                digits = 16;
                value = *longs++;
                break;
            case 'p':
                digits = 0;
                out.Pointer(*pointers);
                value = *pointers ? 1 : 0;
                ++pointers;
                break;
            case 'c':
                digits = 0;
                out.Char(*bytes);
                value = *bytes++;
                break;
            case 's':
                digits = 0;
                out.String(static_cast<const char*>(*pointers));
                out.Char('\n');
                value = *pointers ? 1 : 0;
                ++pointers;
                break;
            case 'S':
                digits = 0;
                out.UString(static_cast<const char16_t*>(*pointers), -1);
                out.Char('\n');
                value = *pointers ? 1 : 0;
                ++pointers;
                break;
            }
            if (digits > 0) {
                out.Hex(value, digits);
                out.Char(' ');
            }
            if (length == -1 && value == 0) break;
        }
    }
    out.Char('[');
    out.Hex(length, 8);
    out.Char(']');
}

}

extern "C" {

void IcuTraceCleanup_nid_no_patch() {
    traceEntry = nullptr;
    traceExit = nullptr;
    traceData = nullptr;
    traceLevel = TraceOff;
    traceContext = nullptr;
}

void APS5_VABI utrace_entry_nid_postfix(std::int32_t function) {
    if (traceEntry) traceEntry(traceContext, function);
}

void APS5_VABI utrace_exit_nid_postfix(std::int32_t function, std::int32_t returnType, ...) {
    if (!traceExit) return;
    const char* format;
    switch (returnType) {
    case 0:
        format = "Returns.";
        break;
    case ExitI32:
        format = "Returns %d.";
        break;
    case ExitStatus:
        format = "Returns.  Status = %d.";
        break;
    case ExitI32 | ExitStatus:
        format = "Returns %d.  Status = %d.";
        break;
    case ExitPtr | ExitStatus:
        format = "Returns %d.  Status = %p.";
        break;
    default:
        APS5_INVALID_ARG_EX;
    }
    APS5_TRACE_VA_LIST args;
    APS5_TRACE_VA_START(args, returnType);
    traceExit(traceContext, function, format, reinterpret_cast<VaList*>(args));
    APS5_TRACE_VA_END(args);
}

void APS5_VABI utrace_data_nid_postfix(std::int32_t function, std::int32_t level, const char* format, ...) {
    if (!traceData) return;
    APS5_TRACE_VA_LIST args;
    APS5_TRACE_VA_START(args, format);
    traceData(traceContext, function, level, format, reinterpret_cast<VaList*>(args));
    APS5_TRACE_VA_END(args);
}

std::int32_t APS5_VABI utrace_vformat_nid_postfix(char* buffer, std::int32_t capacity, std::int32_t indent, const char* format, VaList* list) {
    if (!format || (!buffer && capacity > 0)) APS5_INVALID_ARG_EX;
    Arguments args(list);
    Output out(buffer, capacity, indent);
    std::int64_t value = 0;
    std::size_t position = 0;
    for (;;) {
        char c = format[position++];
        if (c != '%') {
            out.Char(c);
            if (c == 0) break;
            continue;
        }
        c = format[position++];
        switch (c) {
        case 'c':
            out.Char(static_cast<char>(args.Next<std::int32_t>()));
            break;
        case 's':
            out.String(args.Next<const char*>());
            break;
        case 'S': {
            const auto* text = args.Next<const char16_t*>();
            out.UString(text, args.Next<std::int32_t>());
            break;
        }
        case 'b':
            out.Hex(args.Next<std::int32_t>(), 2);
            break;
        case 'h':
            out.Hex(args.Next<std::int32_t>(), 4);
            break;
        case 'd':
            out.Hex(args.Next<std::int32_t>(), 8);
            break;
        case 'l':
            value = args.Next<std::int64_t>();
            out.Hex(value, 16);
            break;
        case 'p':
            out.Pointer(args.Next<const void*>());
            break;
        case 0:
            out.Char('%');
            --position;
            break;
        case 'v': {
            const char type = format[position];
            if (type != 0) ++position;
            Vector(out, type, args, value);
            break;
        }
        default:
            out.Char(c);
        }
    }
    return out.index + 1;
}

std::int32_t APS5_VABI utrace_format_nid_postfix(char* buffer, std::int32_t capacity, std::int32_t indent, const char* format, ...) {
    APS5_TRACE_VA_LIST args;
    APS5_TRACE_VA_START(args, format);
    const std::int32_t length = utrace_vformat_nid_postfix(buffer, capacity, indent, format, reinterpret_cast<VaList*>(args));
    APS5_TRACE_VA_END(args);
    return length;
}

void APS5_VABI utrace_setFunctions_nid_postfix(const void* context, UTraceEntry entry, UTraceExit exit, UTraceData data) {
    traceEntry = entry;
    traceExit = exit;
    traceData = data;
    traceContext = context;
}

void APS5_VABI utrace_getFunctions_nid_postfix(const void** context, UTraceEntry* entry, UTraceExit* exit, UTraceData* data) {
    if (!context || !entry || !exit || !data) APS5_INVALID_ARG_EX;
    *entry = traceEntry;
    *exit = traceExit;
    *data = traceData;
    *context = traceContext;
}

void APS5_VABI utrace_setLevel_nid_postfix(std::int32_t level) {
    if (level < TraceOff) level = TraceOff;
    if (level > TraceVerbose) level = TraceVerbose;
    traceLevel = level;
}

std::int32_t APS5_VABI utrace_getLevel_nid_postfix() {
    return traceLevel;
}

const char* APS5_VABI utrace_functionName_nid_postfix(std::int32_t function) {
    if (FunctionStart <= function && function < FunctionLimit) return FunctionNames[function - FunctionStart];
    if (ConversionStart <= function && function < ConversionLimit) return ConversionNames[function - ConversionStart];
    if (CollationStart <= function && function < CollationLimit) return CollationNames[function - CollationStart];
    if (DataStart <= function && function < DataLimit) return DataNames[function - DataStart];
    return "[BOGUS Trace Function Number]";
}

}
