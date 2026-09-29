#ifndef CORE_LIBS_PRX_LIBC_INCLUDE_VERBOSELOG_HPP
#define CORE_LIBS_PRX_LIBC_INCLUDE_VERBOSELOG_HPP

#include <cstdio>

inline bool VerboseLog() {
    static const bool enabled = [] {
        std::FILE* file = std::fopen("verbose_log.txt", "rb");
        if (file != nullptr) std::fclose(file);
        return file != nullptr;
    }();
    return enabled;
}

#endif
