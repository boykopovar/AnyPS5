#ifndef CORE_LIBS_PRX_LIBC_INCLUDE_SCANQUADLENGTH_HPP
#define CORE_LIBS_PRX_LIBC_INCLUDE_SCANQUADLENGTH_HPP

#include <cstring>

namespace LibcDetail {

inline const char* FindQuadScanDirective(const char* format) {
    if (format == nullptr) return nullptr;
    while ((format = std::strchr(format, '%')) != nullptr) {
        const char* const directive = format++;
        if (*format == '%') { ++format; continue; }
        if (*format == '*') ++format;
        while (*format >= '0' && *format <= '9') ++format;
        if (*format == 'q') return directive;
        while (*format && std::strchr("hljztL", *format)) ++format;
        if (*format == '[') {
            ++format;
            if (*format == '^') ++format;
            if (*format == ']') ++format;
            while (*format && *format != ']') ++format;
        }
        if (*format) ++format;
    }
    return nullptr;
}

}

#endif
