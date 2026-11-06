#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_TESTS_SAVEDEXECMASKLOOP_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_TESTS_SAVEDEXECMASKLOOP_HPP

#include <cstdint>
#include <vector>

namespace AgcTest {

inline std::vector<std::uint32_t> SavedExecMaskLoop(std::uint32_t rounds) {
    std::vector<std::uint32_t> code{0xbea2047eu, 0xbea40380u};
    for (std::uint32_t step = 0; step < rounds; ++step) code.insert(code.end(), {0xbefe0422u, 0x7c080080u, 0x8aa26a7eu});
    const auto branch = 0xbf850000u | static_cast<std::uint16_t>(2 - static_cast<int>(code.size() + 3));
    code.insert(code.end(), {0x80248124u, 0xbf0a8424u, branch, 0xbefe0422u});
    return code;
}

}

#endif
