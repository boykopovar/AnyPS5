#ifndef CORE_SHADER_RECOMPILER_USERDATAUSE_HPP
#define CORE_SHADER_RECOMPILER_USERDATAUSE_HPP

#include <cstdint>
#include <utility>
#include <vector>

namespace ShaderRecompiler {

struct SrtUserDataUse {
    static constexpr std::uint32_t TrackedWords = 64;
    std::uint64_t asBase = 0;
    std::uint64_t asValue = 0;
    std::vector<std::pair<std::uint64_t, std::int32_t>> reads;
};

}

#endif
