#include "SceShaders.hpp"
#include "prx/libSceAgcDriver/Execution/include/ShaderPreparationScope.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceAgc/Shader/include/ShaderConstants.hpp"
#include <array>
#include <algorithm>

extern "C" int APS5_VABI sceAgcCreatePrimState(ShaderRegister*, ShaderRegister*, const Shader*, const Shader*, std::uint32_t);
extern "C" int APS5_VABI sceAgcCreateInterpolantMapping_0100(ShaderRegister*, const Shader*, const Shader*);

extern "C" int APS5_VABI sceAgcLinkShaders(ShaderRegister* context, ShaderRegister* primitive,
    const void* reserved, const Shader* vertex, const Shader* pixel, std::uint32_t primitiveType) {
    using namespace ShaderRegs;
    if (!context || !primitive || reserved || !vertex ||
        vertex->type != static_cast<std::uint8_t>(ShaderBinaryType::Gs) ||
        (vertex->num_output_semantics && !vertex->output_semantics) ||
        (pixel && (pixel->type != static_cast<std::uint8_t>(ShaderBinaryType::Ps) ||
                   pixel->num_input_semantics > 32 ||
                   (pixel->num_input_semantics && !pixel->input_semantics))))
        return GRAPHICS5_ERROR_INVALID_SHADER_PROGRAM;

    // The simple vertex/pixel pipeline combines two primitive context registers
    // and 32 interpolant registers. Publish only after both helpers succeed.
    AgcDriver::ShaderPreparationScope transaction;
    std::array<ShaderRegister, 34> contextValues{};
    std::array<ShaderRegister, 3> primitiveValues{};
    sceAgcCreatePrimState(contextValues.data(), primitiveValues.data(), nullptr, vertex, primitiveType);
    sceAgcCreateInterpolantMapping_0100(contextValues.data() + 2, vertex, pixel);
    AgcDriverResolveGraphicsAbi_nid_postfix(vertex, pixel, primitiveType);
    transaction.Commit();
    std::copy(contextValues.begin(), contextValues.end(), context);
    std::copy(primitiveValues.begin(), primitiveValues.end(), primitive);
    return 0;
}
