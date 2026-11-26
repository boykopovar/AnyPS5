#ifndef CORE_LIBS_PRX_LIBC_INCLUDE_SCANQUADLENGTH_HPP
#define CORE_LIBS_PRX_LIBC_INCLUDE_SCANQUADLENGTH_HPP

#include <cstring>

namespace LibcDetail {

inline const char* FindQuadScanDirective(const char* format) {
    if (format == nullptr) return nullptr;
    while ((format = std::strchr(format, '%')) != nullptr) {
        const char* const directive = format++;
        if (*format == '%') { ++format; continue; }
        const char* pos = format;
        while (*pos >= '0' && *pos <= '9') ++pos;
        if (pos != format && *pos == '$') format = pos + 1;
        while (*format == '*' || *format == '\'') ++format;
        while (*format >= '0' && *format <= '9') ++format;
        if (*format == 'm') {
            const char* afterM = format + 1;
            while (*afterM && std::strchr("hljztL", *afterM)) ++afterM;
            if (*afterM == '[' || *afterM == 's' || *afterM == 'c') format = afterM;
        }
        while (*format && std::strchr("hljztL", *format)) ++format;
        if (*format == 'q') return directive;
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
