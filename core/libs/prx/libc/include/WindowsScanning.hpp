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
    FormatArguments args(source);
    while (*format) {
        const char value = *format++;
        translated += value;
        if (value != '%') continue;
        if (*format == '%') { translated += *format++; continue; }
        const bool suppressed = *format == '*';
        if (suppressed) translated += *format++;
        while (*format >= '0' && *format <= '9') translated += *format++;
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
        if (std::strchr("diouxXn", conversion) && (length == "l" || length == "j" || length == "z" || length == "t"))
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
        if (!suppressed) pointers.push_back(args.Next<void*>());
        else if (conversion != 'n') {
            translated += "%n";
            pointers.push_back(&suppressedEnds[suppressedCount++]);
        }
    }
    const int result = scan(translated.c_str(), reinterpret_cast<char*>(pointers.data()));
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
