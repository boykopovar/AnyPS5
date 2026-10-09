#ifndef CORE_SHADER_RECOMPILIER_OPTIMIZATION_INCLUDE_OPTIMIZATION_REQUESTMEMORYVIEW_HPP
#define CORE_SHADER_RECOMPILIER_OPTIMIZATION_INCLUDE_OPTIMIZATION_REQUESTMEMORYVIEW_HPP

#include "Optimization/SrtWalker.hpp"
#include "Recompiler.hpp"
#include <cstdint>
#include <span>
#include <vector>

namespace ShaderRecompiler {

class RequestMemoryView {
public:
    explicit RequestMemoryView(std::span<const MemoryRegion> regions);
    [[nodiscard]] SrtRuntime MakeRuntime(std::span<const std::uint32_t> userData, std::uint64_t shaderBase);

private:
    static bool ReadGuestMemory(void* userContext, std::uint64_t address, std::uint32_t* value);
    static bool Covered(void* userContext, std::uint64_t address, std::uint64_t bytes);

    std::vector<MemoryRegion> regions;
};

}

#endif
