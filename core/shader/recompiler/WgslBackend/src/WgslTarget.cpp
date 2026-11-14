#include "WgslBackend/WgslTarget.hpp"
#include <array>
#include <string_view>

namespace ShaderRecompiler {

const SpirvTarget& WebGpuTarget() {
    static constexpr std::array<std::uint32_t, 2> capabilities{1u, 25u};
    static constexpr std::array<std::string_view, 0> extensions{};
    static const SpirvTarget target = [] {
        SpirvTarget result{};
        result.vulkanVersion = 0x00403000u;
        result.spirvVersion = 0x00010600u;
        result.subgroupSize = 1u;
        result.bdaAbiVersion = 0u;
        result.supportedCapabilities = capabilities;
        result.supportedExtensions = extensions;
        result.maxWorkgroupSize = {256u, 256u, 64u};
        result.maxWorkgroupInvocations = 256u;
        result.maxWorkgroupSharedMemoryBytes = 16384u;
        result.vertexSubgroups = false;
        result.nonConstantImageOffsets = true;
        result.wgsl = true;
        return result;
    }();
    return target;
}

}  // namespace ShaderRecompiler
