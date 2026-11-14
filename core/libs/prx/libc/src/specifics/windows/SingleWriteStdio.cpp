#if defined(__MINGW32__) && !defined(__USE_MINGW_ANSI_STDIO)
#define __USE_MINGW_ANSI_STDIO 1
#endif
#include <cstdarg>
#include <cstddef>
#include <cstdio>
#include <cstdlib>

extern "C" {

int __real___mingw_vfprintf(std::FILE* stream, const char* format, std::va_list arguments);

}

namespace {

int WriteWhole(std::FILE* stream, const char* text, std::size_t size) noexcept {
    if (size == 0) return 0;
    return std::fwrite(text, 1, size, stream) == size ? static_cast<int>(size) : -1;
}

int PrintWhole(std::FILE* stream, const char* format, std::va_list arguments) noexcept {
    char local[1024];
    std::va_list measured;
    va_copy(measured, arguments);
    const int length = std::vsnprintf(local, sizeof(local), format, measured);
    va_end(measured);
    if (length < 0) return __real___mingw_vfprintf(stream, format, arguments);
    const auto size = static_cast<std::size_t>(length);
    if (size < sizeof(local)) return WriteWhole(stream, local, size);
    char* const large = static_cast<char*>(std::malloc(size + 1));
    if (large == nullptr) return __real___mingw_vfprintf(stream, format, arguments);
    const int again = std::vsnprintf(large, size + 1, format, arguments);
    const int result = again < 0 ? -1 : WriteWhole(stream, large, static_cast<std::size_t>(again) < size ? static_cast<std::size_t>(again) : size);
    std::free(large);
    return result;
}

}

extern "C" {

int __wrap___mingw_vfprintf(std::FILE* stream, const char* format, std::va_list arguments) {
    return PrintWhole(stream, format, arguments);
}

int __wrap___mingw_fprintf(std::FILE* stream, const char* format, ...) {
    std::va_list arguments;
    va_start(arguments, format);
    const int result = PrintWhole(stream, format, arguments);
    va_end(arguments);
    return result;
}

int __wrap___mingw_vprintf(const char* format, std::va_list arguments) {
    return PrintWhole(stdout, format, arguments);
}

int __wrap___mingw_printf(const char* format, ...) {
    std::va_list arguments;
    va_start(arguments, format);
    const int result = PrintWhole(stdout, format, arguments);
    va_end(arguments);
    return result;
}

}
