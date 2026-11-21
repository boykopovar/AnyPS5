#include "prx/libSceAgc/Shader/include/PrimState.hpp"
#include "prx/libSceAgc/Shader/include/LinkShaders.hpp"
#include "prx/libSceAgc/Shader/include/InterpolantMapping.hpp"
#include "SceShaders.hpp"
#include "prx/libSceAgcDriver/Execution/include/ShaderPreparationScope.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceAgc/Shader/include/ShaderConstants.hpp"
#include "prx/libc/include/General.hpp"
#include <array>
#include <algorithm>
#include <cerrno>

extern "C" int APS5_VABI sceAgcLinkShaders(ShaderRegister* context, ShaderRegister* primitive,
    const void* reserved, const Shader* vertex, const Shader* pixel, std::uint32_t primitiveType) {
    using namespace ShaderRegs;
    const int savedErrno = errno;
    std::array<ShaderRegister, 34> previousContext{};
    std::array<ShaderRegister, 3> previousPrimitive{};
    Shader vertexHeader{};
    Shader pixelHeader{};
    const bool readable = context && primitive && vertex &&
        GuestCopyIn_nid_no_patch(previousContext.data(), context, sizeof(previousContext)) &&
        GuestCopyIn_nid_no_patch(previousPrimitive.data(), primitive, sizeof(previousPrimitive)) &&
        GuestCopyIn_nid_no_patch(&vertexHeader, vertex, sizeof(vertexHeader)) &&
        (!pixel || GuestCopyIn_nid_no_patch(&pixelHeader, pixel, sizeof(pixelHeader)));
    errno = savedErrno;
    if (reserved || !readable ||
        vertexHeader.type != static_cast<std::uint8_t>(ShaderBinaryType::Gs) ||
        (vertexHeader.num_output_semantics && !vertexHeader.output_semantics) ||
        (pixel && (pixelHeader.type != static_cast<std::uint8_t>(ShaderBinaryType::Ps) ||
                   pixelHeader.num_input_semantics > 32 ||
                   (pixelHeader.num_input_semantics && !pixelHeader.input_semantics))))
        return GRAPHICS5_ERROR_INVALID_SHADER_PROGRAM;

    // The simple vertex/pixel pipeline combines two primitive context registers
    // and 32 interpolant registers. Publish only after both helpers succeed.
    AgcDriver::ShaderPreparationScope transaction({vertex, pixel});
    std::array<ShaderRegister, 34> contextValues{};
    std::array<ShaderRegister, 3> primitiveValues{};
    sceAgcCreatePrimState(contextValues.data(), primitiveValues.data(), nullptr, vertex, primitiveType);
    sceAgcCreateInterpolantMapping_0100(contextValues.data() + 2, vertex, pixel);
    if (pixel != nullptr) {
        const std::array<const Shader*, 2> stages{vertex, pixel};
        AgcDriverResolveGraphicsStagesAbi_nid_postfix(stages, contextValues, primitiveValues);
    }
    AgcDriverResolveGraphicsAbi_nid_postfix(vertex, pixel, primitiveType);
    bool stored = GuestCopyOut_nid_no_patch(context, contextValues.data(), sizeof(contextValues));
    if (stored && !GuestCopyOut_nid_no_patch(primitive, primitiveValues.data(), sizeof(primitiveValues))) {
        GuestCopyOut_nid_no_patch(context, previousContext.data(), sizeof(previousContext));
        stored = false;
    }
    errno = savedErrno;
    if (!stored) return GRAPHICS5_ERROR_INVALID_SHADER_PROGRAM;
    transaction.Commit();
    return 0;
}
