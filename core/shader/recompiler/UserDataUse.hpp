#ifndef CORE_SHADER_RECOMPILER_USERDATAUSE_HPP
#define CORE_SHADER_RECOMPILER_USERDATAUSE_HPP

#include <cstdint>
#include <utility>
#include <vector>

namespace ShaderRecompiler {

struct BufferBaseWord {
    std::uint32_t buffer;
    std::uint64_t address;
    std::int64_t flatOffset;
};

struct SrtUserDataUse {
    static constexpr std::uint32_t TrackedWords = 64;
    std::uint64_t asBase = 0;
    std::uint64_t asValue = 0;
    std::vector<std::pair<std::uint64_t, std::int32_t>> reads;
    std::vector<const void*> readInstructions;
    std::vector<const void*> operandReads;
    std::vector<std::pair<std::uint32_t, const void*>> sourceLeads;
};

}

#endif
