#ifndef CORE_LIBS_PRX_LIBC_INCLUDE_WINDOWSSCANNING_HPP
#define CORE_LIBS_PRX_LIBC_INCLUDE_WINDOWSSCANNING_HPP

#include "WindowsFormatting.hpp"
#include "General.hpp"
#include <cerrno>
#include <cstdint>
#include <deque>

namespace LibcDetail {

struct NarrowScannedInteger {
    void* destination;
    size_t size;
    size_t assignment;
};

template <typename TScanner>
inline int ScanWindowsArguments_nid_no_patch(const char* format, const void* source, TScanner scan) {
    if (!format || !source) { errno = 22; return EOF; }
    std::string translated;
    std::vector<void*> pointers;
    std::deque<std::uint64_t> wideIntegers;
    std::vector<NarrowScannedInteger> narrowIntegers;
    size_t assignments = 0;
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
        const bool narrowInteger = std::strchr("diouxX", conversion) && (length.empty() || length == "h" || length == "hh");
        if (narrowInteger || (std::strchr("diouxXn", conversion) && (length == "l" || length == "j" || length == "z" || length == "t")))
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
        if (suppressed) continue;
        void* destination = args.Next<void*>();
        if (narrowInteger) {
            const size_t size = length.empty() ? sizeof(int) : length == "h" ? sizeof(short) : sizeof(char);
            narrowIntegers.push_back({destination, size, assignments});
            destination = &wideIntegers.emplace_back();
        }
        pointers.push_back(destination);
        if (conversion != 'n') ++assignments;
    }
    const int result = scan(translated.c_str(), reinterpret_cast<char*>(pointers.data()));
    for (size_t i = 0; i < narrowIntegers.size(); ++i)
        if (result != EOF && narrowIntegers[i].assignment < static_cast<size_t>(result))
            std::memcpy(narrowIntegers[i].destination, &wideIntegers[i], narrowIntegers[i].size);
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
