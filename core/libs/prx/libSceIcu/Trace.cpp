// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
#include "prx/libSceIcu/libSceIcu.hpp"
#include <algorithm>
#include <limits>
#include <stdexcept>

#ifdef _WIN32
#define ICU_TRACE_START(arguments, last) __builtin_sysv_va_start(arguments, last)
#define ICU_TRACE_END(arguments) __builtin_sysv_va_end(arguments)
#else
#define ICU_TRACE_START(arguments, last) va_start(arguments, last)
#define ICU_TRACE_END(arguments) va_end(arguments)
#endif

namespace {

Icu::TraceEntry entryFunction = nullptr;
Icu::TraceExit exitFunction = nullptr;
Icu::TraceData dataFunction = nullptr;
const void* traceContext = nullptr;
std::int32_t traceLevel = 0;

class TraceWriter {
    char* buffer;
    std::int32_t capacity;
    std::int32_t position = 0;

    void Advance() {
        if (position == std::numeric_limits<std::int32_t>::max()) throw std::invalid_argument("ICU trace: output overflow");
        ++position;
    }

    void Store(char value) {
        if (position >= capacity) return;
        if (!buffer) throw std::invalid_argument("ICU trace: null output buffer");
        buffer[position] = value;
    }

public:
    TraceWriter(char* buffer, std::int32_t capacity) : buffer(buffer), capacity(capacity) {}

    void Character(char value, std::int32_t indent) {
        if (!position || (value != '\n' && value && position < capacity && buffer[position - 1] == '\n') ||
            (value == '\n' && position >= capacity)) {
            for (std::int32_t i = 0; i < indent; ++i) {
                Store(' ');
                Advance();
            }
        }
        Store(value);
        if (value) Advance();
    }

    void Hex(std::uint64_t value, int digits) {
        for (int shift = (digits - 1) * 4; shift >= 0; shift -= 4)
            Character("0123456789abcdef"[(value >> shift) & 15], 0);
    }

    void Pointer(const void* pointer) {
        Hex(reinterpret_cast<std::uintptr_t>(pointer), 16);
    }

    void String(const char* text, std::int32_t indent) {
        if (!text) text = "*NULL*";
        char value;
        do {
            value = *text++;
            Character(value, indent);
        } while (value);
    }

    void Unicode(const char16_t* text, std::int32_t length, std::int32_t indent) {
        if (!text) {
            String(nullptr, indent);
            return;
        }
        for (std::int64_t i = 0; i < length || length == -1; ++i) {
            if (i > std::numeric_limits<std::int32_t>::max()) throw std::invalid_argument("ICU trace: string length overflow");
            const auto unit = text[i];
            Hex(unit, 4);
            Character(' ', indent);
            if (length == -1 && !unit) break;
        }
    }

    std::int32_t Finish(std::int32_t indent) {
        Character(0, indent);
        if (position == std::numeric_limits<std::int32_t>::max()) throw std::invalid_argument("ICU trace: result overflow");
        return position + 1;
    }
};

}

extern "C" {

void APS5_VABI utrace_entry_nid_postfix(std::int32_t number) {
    if (entryFunction) entryFunction(traceContext, number);
}

void APS5_VABI utrace_exit_nid_postfix(std::int32_t number, std::int32_t type, ...) {
    if (!exitFunction) return;
    const char* format;
    switch (type) {
    case 0: format = "Returns."; break;
    case 1: format = "Returns %d."; break;
    case 16: format = "Returns.  Status = %d."; break;
    case 17: format = "Returns %d.  Status = %d."; break;
    case 18: format = "Returns %d.  Status = %p."; break;
    default: throw std::invalid_argument("ICU trace: invalid exit type");
    }
    Icu::TraceArguments arguments;
    ICU_TRACE_START(arguments, type);
    exitFunction(traceContext, number, format, arguments);
    ICU_TRACE_END(arguments);
}

void APS5_VABI utrace_data_nid_postfix(std::int32_t number, std::int32_t level, const char* format, ...) {
    if (!dataFunction) return;
    Icu::TraceArguments arguments;
    ICU_TRACE_START(arguments, format);
    dataFunction(traceContext, number, level, format, arguments);
    ICU_TRACE_END(arguments);
}

std::int32_t APS5_VABI utrace_vformat_nid_postfix(char* buffer, std::int32_t capacity, std::int32_t indent, const char* format, Icu::TraceArguments arguments) {
    if (!format) throw std::invalid_argument("ICU trace: null format");
    TraceWriter writer(buffer, capacity);
    std::int64_t longArgument = 0;
    for (;;) {
        auto value = *format++;
        if (value != '%') {
            writer.Character(value, indent);
            if (!value) break;
            continue;
        }
        value = *format++;
        switch (value) {
        case 'c': writer.Character(static_cast<char>(va_arg(arguments, int)), indent); break;
        case 's': writer.String(va_arg(arguments, const char*), indent); break;
        case 'S': {
            const auto* text = va_arg(arguments, const char16_t*);
            const auto length = va_arg(arguments, std::int32_t);
            writer.Unicode(text, length, indent);
            break;
        }
        case 'b': writer.Hex(va_arg(arguments, int), 2); break;
        case 'h': writer.Hex(va_arg(arguments, int), 4); break;
        case 'd': writer.Hex(va_arg(arguments, int), 8); break;
        case 'l':
            longArgument = va_arg(arguments, std::int64_t);
            writer.Hex(longArgument, 16);
            break;
        case 'p': writer.Pointer(va_arg(arguments, const void*)); break;
        case 0:
            writer.Character('%', indent);
            --format;
            break;
        case 'v': {
            const auto type = *format;
            if (type) ++format;
            const auto* data = va_arg(arguments, const void*);
            const auto length = va_arg(arguments, std::int32_t);
            if (!data) writer.String("*NULL* ", indent);
            else {
                for (std::int64_t i = 0; i < length || length == -1; ++i) {
                    if (i > std::numeric_limits<std::int32_t>::max()) throw std::invalid_argument("ICU trace: vector length overflow");
                    int digits = 0;
                    switch (type) {
                    case 'b': longArgument = static_cast<const std::int8_t*>(data)[i]; digits = 2; break;
                    case 'h': longArgument = static_cast<const std::int16_t*>(data)[i]; digits = 4; break;
                    case 'd': longArgument = static_cast<const std::int32_t*>(data)[i]; digits = 8; break;
                    case 'l': longArgument = static_cast<const std::int64_t*>(data)[i]; digits = 16; break;
                    case 'c':
                        longArgument = static_cast<const char*>(data)[i];
                        writer.Character(static_cast<char>(longArgument), indent);
                        break;
                    case 'p': {
                        const auto* pointer = static_cast<const void* const*>(data)[i];
                        longArgument = pointer ? 1 : 0;
                        writer.Pointer(pointer);
                        break;
                    }
                    case 's': case 'S': {
                        const auto* pointer = static_cast<const void* const*>(data)[i];
                        longArgument = pointer ? 1 : 0;
                        if (type == 's') writer.String(static_cast<const char*>(pointer), indent);
                        else writer.Unicode(static_cast<const char16_t*>(pointer), -1, indent);
                        writer.Character('\n', indent);
                        break;
                    }
                    default:
                        if (length == -1 && longArgument) throw std::invalid_argument("ICU trace: nonterminating vector");
                        break;
                    }
                    if (digits) {
                        writer.Hex(longArgument, digits);
                        writer.Character(' ', indent);
                    }
                    if (length == -1 && !longArgument) break;
                }
            }
            writer.Character('[', indent);
            writer.Hex(length, 8);
            writer.Character(']', indent);
            break;
        }
        default: writer.Character(value, indent); break;
        }
    }
    return writer.Finish(indent);
}

std::int32_t APS5_VABI utrace_format_nid_postfix(char* buffer, std::int32_t capacity, std::int32_t indent, const char* format, ...) {
    Icu::TraceArguments arguments;
    ICU_TRACE_START(arguments, format);
    const auto length = utrace_vformat_nid_postfix(buffer, capacity, indent, format, arguments);
    ICU_TRACE_END(arguments);
    return length;
}

void APS5_VABI utrace_setFunctions_nid_postfix(const void* context, Icu::TraceEntry entry, Icu::TraceExit exit, Icu::TraceData data) {
    entryFunction = entry;
    exitFunction = exit;
    dataFunction = data;
    traceContext = context;
}

void APS5_VABI utrace_getFunctions_nid_postfix(const void** context, Icu::TraceEntry* entry, Icu::TraceExit* exit, Icu::TraceData* data) {
    if (!context || !entry || !exit || !data) throw std::invalid_argument("ICU trace: null function output");
    *entry = entryFunction;
    *exit = exitFunction;
    *data = dataFunction;
    *context = traceContext;
}

void APS5_VABI utrace_setLevel_nid_postfix(std::int32_t level) {
    traceLevel = std::clamp(level, -1, 9);
}

std::int32_t APS5_VABI utrace_getLevel_nid_postfix() {
    return traceLevel;
}

const char* APS5_VABI utrace_functionName_nid_postfix(std::int32_t number) {
    static constexpr const char* functions[]{"u_init", "u_cleanup"};
    static constexpr const char* conversions[]{"ucnv_open", "ucnv_openPackage", "ucnv_openAlgorithmic", "ucnv_clone", "ucnv_close", "ucnv_flushCache", "ucnv_load", "ucnv_unload"};
    static constexpr const char* collations[]{"ucol_open", "ucol_close", "ucol_strcoll", "ucol_getSortKey", "ucol_getLocale", "ucol_nextSortKeyPart", "ucol_strcollIter", "ucol_openFromShortString", "ucol_strcollUTF8"};
    static constexpr const char* resources[]{"resc", "bundle-open", "file-open", "res-open"};
    if (number >= 0 && number < 2) return functions[number];
    if (number >= 0x1000 && number < 0x1008) return conversions[number - 0x1000];
    if (number >= 0x2000 && number < 0x2009) return collations[number - 0x2000];
    if (number >= 0x3000 && number < 0x3004) return resources[number - 0x3000];
    return "[BOGUS Trace Function Number]";
}

}
