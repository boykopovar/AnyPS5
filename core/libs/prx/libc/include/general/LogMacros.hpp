#ifndef CORE_LIBS_PRX_LIBC_INCLUDE_GENERAL_LOGMACROS_HPP
#define CORE_LIBS_PRX_LIBC_INCLUDE_GENERAL_LOGMACROS_HPP

#include <atomic>
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

using LogSinkFn = int (*)(std::FILE* stream, const char* text, std::size_t length);

inline std::atomic<LogSinkFn>& LogSink() {
    static std::atomic<LogSinkFn> sink{nullptr};
    return sink;
}

inline long long LogTimestampMicros() {
    static const bool timestamps = std::getenv("APS5_LOG_TIMESTAMPS") != nullptr;
    if (!timestamps) return -1;
    using Clock = std::chrono::steady_clock;
    static const auto started = Clock::now();
    return static_cast<long long>(std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - started).count());
}

inline int FormatLogTimestamp(char* out, std::size_t capacity, long long micros) {
    return std::snprintf(out, capacity, "[+%llu.%06llu s] ", static_cast<unsigned long long>(micros / 1000000), static_cast<unsigned long long>(micros % 1000000));
}

#ifdef __MINGW_PRINTF_FORMAT
[[gnu::format(__MINGW_PRINTF_FORMAT, 2, 3)]]
#else
[[gnu::format(printf, 2, 3)]]
#endif
inline int WriteLogLine(std::FILE* stream, const char* format, ...) {
    char small[1024];
    std::size_t prefixLength = 0;
    const long long micros = LogTimestampMicros();
    if (micros >= 0) {
        const int written = FormatLogTimestamp(small, sizeof(small), micros);
        if (written < 0 || written >= static_cast<int>(sizeof(small))) return -1;
        prefixLength = static_cast<std::size_t>(written);
    }
    std::va_list arguments;
    va_start(arguments, format);
    std::va_list again;
    va_copy(again, arguments);
    const int length = std::vsnprintf(small + prefixLength, sizeof(small) - prefixLength, format, arguments);
    va_end(arguments);
    std::string large;
    const auto remaining = sizeof(small) - prefixLength;
    if (length >= static_cast<int>(remaining)) {
        large.resize(prefixLength + static_cast<std::size_t>(length));
        if (prefixLength != 0) std::memcpy(large.data(), small, prefixLength);
        std::vsnprintf(large.data() + prefixLength, large.size() - prefixLength + 1, format, again);
    }
    va_end(again);
    if (length < 0) return length;
    const auto totalLength = prefixLength + static_cast<std::size_t>(length);
    const char* text = large.empty() ? small : large.data();
    if (const auto sink = LogSink().load(std::memory_order_acquire)) return sink(stream, text, totalLength);
    std::fwrite(text, 1, totalLength, stream);
    return std::fflush(stream);
}

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
WriteLogLine(stream, "[%s:%d %s] " fmt "\n", _APS5_FILE_, __LINE__, TrimNidPostfix(__func__), __VA_ARGS__)

#define _APS5_LOG_IMPL_NF(stream, fmt) \
WriteLogLine(stream, "[%s:%d %s] " fmt "\n", _APS5_FILE_, __LINE__, TrimNidPostfix(__func__))

#define APS5_LOG_OUT(fmt, ...) _APS5_LOG_IMPL(stdout, fmt, __VA_ARGS__)
#define APS5_LOG_ERR(fmt, ...) _APS5_LOG_IMPL(stderr, fmt, __VA_ARGS__)
#define APS5_LOG_CHARS_OUT(fmt) _APS5_LOG_IMPL_NF(stdout, fmt)
#define APS5_LOG_CHARS_ERR(fmt) _APS5_LOG_IMPL_NF(stderr, fmt)

#define APS5_LOG_OUT_IF(enabled, fmt, ...) \
do { \
if (enabled) { \
_APS5_LOG_IMPL(stdout, fmt, __VA_ARGS__); \
} \
} while (0)

#define APS5_LOG_CHARS_OUT_IF(enabled, fmt) \
do { \
if (enabled) { \
_APS5_LOG_IMPL_NF(stdout, fmt); \
} \
} while (0)

#define APS5_LOG_TIMING(fmt, ...) APS5_LOG_OUT_IF(APS5_ENABLE_TIMING_LOG, fmt, __VA_ARGS__)
#define APS5_LOG_CHARS_OUT_TIMING(fmt) APS5_LOG_CHARS_OUT_IF(APS5_ENABLE_TIMING_LOG, fmt)

#ifdef APS5_DEBUG_LOG
#define APS5_LOG_OUT_DEBUG(fmt, ...) _APS5_LOG_IMPL(stdout, fmt, __VA_ARGS__)
#define APS5_LOG_CHARS_OUT_DEBUG(fmt) _APS5_LOG_IMPL_NF(stdout, fmt)
#else
#define APS5_LOG_OUT_DEBUG(fmt, ...) ((void)0)
#define APS5_LOG_CHARS_OUT_DEBUG(fmt) ((void)0)
#endif

#endif
