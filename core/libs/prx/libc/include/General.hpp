#ifndef CORE_LIBS_PRX_LIBC_INCLUDE_GENERAL_HPP
#define CORE_LIBS_PRX_LIBC_INCLUDE_GENERAL_HPP
#include <cstring>

extern "C" void NotImplemented_nid_no_patch(const char* funcName);

inline const char* TrimNidPostfix(const char* func) {
    return func;
    const char* pos = std::strstr(func, "_nid_postfix");
    if (pos != nullptr) {
        static thread_local char buf[256];
        const std::size_t len = static_cast<std::size_t>(pos - func);
        std::memcpy(buf, func, len);
        buf[len] = '\0';
        return buf;
    }
    return func;
}

constexpr const char* BaseName(const char* path) {
    const char* last = path;
    const char* prev = nullptr;
    for (const char* p = path; *p; ++p)
        if (*p == '/' || *p == '\\') {
            prev = last;
            last = p + 1;
        }
    return (prev && *prev) ? prev : last;
}

#define _APS5_FILE_ BaseName(__FILE__)

#define _APS5_LOG_IMPL(stream, fmt, ...) \
(std::fprintf(stream, "[%s:%d %s] " fmt "\n", _APS5_FILE_, __LINE__, TrimNidPostfix(__func__), __VA_ARGS__), \
std::fflush(stream))

#define _APS5_LOG_IMPL_NF(stream, fmt) \
(std::fprintf(stream, "[%s:%d %s] " fmt "\n", _APS5_FILE_, __LINE__, TrimNidPostfix(__func__)), \
std::fflush(stream))

#define APS5_LOG_OUT(fmt, ...) _APS5_LOG_IMPL(stdout, fmt, __VA_ARGS__)
#define APS5_LOG_ERR(fmt, ...) _APS5_LOG_IMPL(stderr, fmt, __VA_ARGS__)
#define APS5_LOG_CHARS_OUT(fmt) _APS5_LOG_IMPL_NF(stdout, fmt)
#define APS5_LOG_CHARS_ERR(fmt) _APS5_LOG_IMPL_NF(stderr, fmt)

#endif
