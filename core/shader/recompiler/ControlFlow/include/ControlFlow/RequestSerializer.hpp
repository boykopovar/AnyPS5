#ifndef CORE_SHADER_RECOMPILIER_CONTROLFLOW_INCLUDE_CONTROLFLOW_REQUESTSERIALIZER_HPP
#define CORE_SHADER_RECOMPILIER_CONTROLFLOW_INCLUDE_CONTROLFLOW_REQUESTSERIALIZER_HPP

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "Recompiler.hpp"

namespace ShaderRecompiler {

    struct DeserializedLinkedProgram {
        std::vector<std::uint32_t> code;
        std::vector<std::byte> header;
        std::vector<std::uint32_t> userData;
        std::vector<CapturedShaderCall> calls;
    };

    struct DeserializedGraphicsCompileContext {
        std::vector<DeserializedLinkedProgram> linkedProgramStorage;
        std::vector<LinkedProgram> linkedPrograms;
        std::optional<MeshConfiguration> mesh;
        std::optional<TessellationConfiguration> tessellation;
        GraphicsDrawParameters draw{};
    };

    struct DeserializedRequest {
        std::vector<std::uint32_t> shaderCode;
        std::vector<std::byte> shaderHeader;
        std::vector<CapturedShaderCall> calls;
        std::vector<std::uint32_t> userData;
        std::optional<ShaderComputeStageInfo> compute;
        std::optional<ShaderPixelStageInfo> pixel;
        std::optional<ShaderVertexStageInfo> vertex;
        std::vector<std::vector<std::byte>> memoryBytesStorage;
        std::vector<MemoryRegion> memory;
        std::vector<std::uint32_t> supportedCapabilities;
        std::vector<std::string> supportedExtensionStorage;
        std::vector<std::string_view> supportedExtensions;
        std::optional<MeshTargetLimits> mesh;
        std::optional<TessellationTargetLimits> tessellation;
        std::unique_ptr<DeserializedGraphicsCompileContext> graphicsStorage;
        RecompileRequest request{};
    };

    class RequestSerializer {
    public:
        [[nodiscard]] std::string Serialize(const RecompileRequest& request) const;
        [[nodiscard]] DeserializedRequest Deserialize(std::string_view text) const;
    };

}

#endif
