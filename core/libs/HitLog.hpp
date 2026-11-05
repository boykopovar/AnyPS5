#ifndef CORE_LIBS_HITLOG_HPP
#define CORE_LIBS_HITLOG_HPP

#include <atomic>
#include <cstdio>

#define APS5_HIT(tag, ...) \
    do { \
        static std::atomic<int> aps5HitCount{0}; \
        if (aps5HitCount.fetch_add(1, std::memory_order_relaxed) < 3) { \
            std::fprintf(stderr, "[" tag "] " __VA_ARGS__); \
            std::fputc('\n', stderr); \
            std::fflush(stderr); \
        } \
    } while (0)

#endif  // CORE_LIBS_HITLOG_HPP
