#pragma once

#include <cstdint>

namespace GnmWorkload {

inline int CreateStream(std::uint64_t name, std::uint32_t* stream) {
    if (name == 0 || !stream) return 3;
    *stream = 1;
    return 0;
}

inline int Begin(std::uint32_t stream, std::uint64_t* workload) {
    if (!workload) return 3;
    *workload = stream < 0x10 ? 1 : 0;
    return stream < 0x10 ? 0 : 1;
}

inline int End(std::uint64_t workload) {
    if (workload == 0) return 2;
    return ((workload >> 56) & 0xff) > 0xf ? 2 : 0;
}

}
