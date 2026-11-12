#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/FileStream.hpp"
#include "SceTypes.hpp"
#include <cstdarg>
#include <cstddef>
#include <cstdio>

#ifdef _WIN32
#define CURL_ARGS_START(last) __builtin_sysv_va_list args; __builtin_sysv_va_start(args, last)
#define CURL_ARGS_END() __builtin_sysv_va_end(args)
#else
#define CURL_ARGS_START(last) std::va_list args; va_start(args, last)
#define CURL_ARGS_END() va_end(args)
#endif

namespace {

VaList* GuestArguments(void* arguments) {
    return static_cast<VaList*>(arguments);
}

}

extern "C" {

int APS5_VABI curl_mvfprintf_nid_postfix(std::FILE* whereto, const char* format, VaList* args) {
    return vfprintf_nid_postfix(reinterpret_cast<FileStream*>(whereto), format, args);
}

int APS5_VABI curl_mfprintf_nid_postfix(std::FILE* whereto, const char* format, ...) {
    CURL_ARGS_START(format);
    const int result = curl_mvfprintf_nid_postfix(whereto, format, GuestArguments(&args));
    CURL_ARGS_END();
    return result;
}

int APS5_VABI curl_mvsprintf_nid_postfix(char* buffer, const char* format, VaList* args) {
    return vsprintf_nid_postfix(buffer, format, args);
}

int APS5_VABI curl_msprintf_nid_postfix(char* buffer, const char* format, ...) {
    CURL_ARGS_START(format);
    const int result = curl_mvsprintf_nid_postfix(buffer, format, GuestArguments(&args));
    CURL_ARGS_END();
    return result;
}

int APS5_VABI curl_mvsnprintf_nid_postfix(char* buffer, size_t maxlength, const char* format, VaList* args) {
    return vsnprintf_nid_postfix(buffer, maxlength, format, args);
}

int APS5_VABI curl_msnprintf_nid_postfix(char* buffer, size_t maxlength, const char* format, ...) {
    CURL_ARGS_START(format);
    const int result = curl_mvsnprintf_nid_postfix(buffer, maxlength, format, GuestArguments(&args));
    CURL_ARGS_END();
    return result;
}

int APS5_VABI curl_mvprintf_nid_postfix(const char* format, VaList* args) {
    return vprintf_nid_postfix(format, args);
}

int APS5_VABI curl_mprintf_nid_postfix(const char* format, ...) {
    CURL_ARGS_START(format);
    const int result = curl_mvprintf_nid_postfix(format, GuestArguments(&args));
    CURL_ARGS_END();
    return result;
}

char* APS5_VABI curl_mvaprintf_nid_postfix(const char* format, VaList* args) {
    char* text = nullptr;
    if (vasprintf_nid_postfix(&text, format, args) < 0) return nullptr;
    return text;
}

char* APS5_VABI curl_maprintf_nid_postfix(const char* format, ...) {
    CURL_ARGS_START(format);
    char* text = curl_mvaprintf_nid_postfix(format, GuestArguments(&args));
    CURL_ARGS_END();
    return text;
}

}
