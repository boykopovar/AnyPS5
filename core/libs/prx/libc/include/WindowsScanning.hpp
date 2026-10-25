#ifndef CORE_LIBS_PRX_LIBC_INCLUDE_WINDOWSSCANNING_HPP
#define CORE_LIBS_PRX_LIBC_INCLUDE_WINDOWSSCANNING_HPP

#include "WindowsFormatting.hpp"
#include "General.hpp"
#include <algorithm>
#include <cerrno>
#include <cstring>

namespace LibcDetail {

template <typename TScanner>
inline int ScanWindowsArguments_nid_no_patch(const char* format, const void* source, TScanner scan) {
    if (!format || !source) { errno = 22; return EOF; }
    std::string translated;
    std::vector<void*> pointers;
    std::vector<int> suppressedEnds(static_cast<size_t>(std::count(format, format + std::strlen(format), '%')), -1);
    size_t suppressedCount = 0;
    struct NarrowInteger {
        void* destination;
        size_t size;
        bool isUnsigned;
        long long value;
        unsigned long long unsignedValue;
        int end;
    };
    std::vector<NarrowInteger> narrowIntegers;
    narrowIntegers.reserve(suppressedEnds.size());
    FormatArguments args(source);
    while (*format) {
        const char value = *format++;
        translated += value;
        if (value != '%') continue;
        if (*format == '%') { translated += *format++; continue; }
        const size_t directive = translated.size() - 1;
        const bool suppressed = *format == '*';
        if (suppressed) translated += *format++;
        while (*format >= '0' && *format <= '9') translated += *format++;
        if (*format == 'q') {
            translated.resize(directive);
            break;
        }
        std::string length;
        if (*format && std::strchr("hljztL", *format)) {
            length += *format++;
            if ((length == "h" && *format == 'h') || (length == "l" && *format == 'l')) length += *format++;
        }
        const char conversion = *format;
        if (!conversion) { errno = 22; return EOF; }
        if (!std::strchr("diouxXaAeEfFgGcspn[", conversion)) {
            NotImplemented_nid_no_patch("vsscanf format conversion");
            return EOF;
        }
        ++format;
        const bool narrow = !suppressed && std::strchr("diouxX", conversion) && (length.empty() || length == "h" || length == "hh");
        if (narrow || (std::strchr("diouxXn", conversion) && (length == "l" || length == "j" || length == "z" || length == "t")))
            translated += "ll";
        else translated += length;
        translated += conversion;
        if (conversion == '[') {
            if (*format == '^') translated += *format++;
            if (*format == ']') translated += *format++;
            while (*format && *format != ']') translated += *format++;
            if (*format != ']') { errno = 22; return EOF; }
            translated += *format++;
        }
        if (narrow) {
            const bool isUnsigned = std::strchr("ouxX", conversion) != nullptr;
            narrowIntegers.push_back({args.Next<void*>(), length.empty() ? sizeof(int) : length == "h" ? sizeof(short) : sizeof(char), isUnsigned, 0, 0, -1});
            translated += "%n";
            auto& integer = narrowIntegers.back();
            pointers.push_back(isUnsigned ? static_cast<void*>(&integer.unsignedValue) : static_cast<void*>(&integer.value));
            pointers.push_back(&integer.end);
        } else if (!suppressed) pointers.push_back(args.Next<void*>());
        else if (conversion != 'n') {
            translated += "%n";
            pointers.push_back(&suppressedEnds[suppressedCount++]);
        }
    }
    const int result = scan(translated.c_str(), reinterpret_cast<char*>(pointers.data()));
    for (const auto& integer : narrowIntegers) {
        if (integer.end < 0) continue;
        if (integer.isUnsigned) {
            if (integer.size == sizeof(char)) *static_cast<unsigned char*>(integer.destination) = static_cast<unsigned char>(integer.unsignedValue);
            else if (integer.size == sizeof(short)) *static_cast<unsigned short*>(integer.destination) = static_cast<unsigned short>(integer.unsignedValue);
            else *static_cast<unsigned int*>(integer.destination) = static_cast<unsigned int>(integer.unsignedValue);
        } else if (integer.size == sizeof(char)) *static_cast<signed char*>(integer.destination) = static_cast<signed char>(integer.value);
        else if (integer.size == sizeof(short)) *static_cast<short*>(integer.destination) = static_cast<short>(integer.value);
        else *static_cast<int*>(integer.destination) = static_cast<int>(integer.value);
    }
    if (result == EOF && std::any_of(suppressedEnds.begin(), suppressedEnds.begin() + suppressedCount, [](int end) { return end >= 0; }))
        return 0;
    return result;
}

inline int ScanWindows(const char* input, const char* format, const void* source) {
    if (!input) { errno = 22; return EOF; }
    return ScanWindowsArguments_nid_no_patch(format, source, [input](const char* translated, char* pointers) {
        return std::vsscanf(input, translated, pointers);
    });
}

inline int ScanFileWindows_nid_no_patch(std::FILE* stream, const char* format, const void* source) {
    if (!stream) { errno = 22; return EOF; }
    return ScanWindowsArguments_nid_no_patch(format, source, [stream](const char* translated, char* pointers) {
        return std::vfscanf(stream, translated, pointers);
    });
}

}

#endif
