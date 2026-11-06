#include "prx/libSceAgcDriver/Graphics/include/Context.hpp"
#include "ControlFlow/GraphBuilder.hpp"
#include "RdnaDecoder/RdnaInstructionDecoder.hpp"
#include "Translation/InstructionTranslator.hpp"
#include <array>

void MeshShaderTranslationTests() {
    using namespace ShaderRecompiler;
    constexpr std::array code{0xbf810000u};
    const auto decoded = RdnaInstructionDecoder{}.Decode(code);
    const auto cfg = GraphBuilder{}.Build(decoded);
    for (const auto primitive : {1u, 2u, 3u, 4u, 6u}) {
        ShaderVertexInputInfo vertex;
        vertex.logicalStage = IrShaderStage::Mesh;
        vertex.mesh.inputPrimitive = primitive;
        vertex.mesh.primitivesPerGroup = 4;
        vertex.mesh.verticesPerGroup = vertex.mesh.InputVertexCount(4);
        vertex.mesh.maxVertices = 12;
        vertex.mesh.maxPrimitives = 4;
        vertex.mesh.esgsItemSize = 4;
        vertex.mesh.threadsNum[0] = 64;
        TranslateOptions options;
        options.stage = ShaderStageKind::Mesh;
        options.inputInfo.vertex = &vertex;
        auto program = InstructionTranslator{}.Translate(decoded, cfg, options);
        ValidateProgram(program, false);
        AgcDriver::Graphics::Require(program.Resources().stage == IrShaderStage::Mesh && !program.Blocks().empty(), "RDNA mesh shader translation lost its stage or control flow");
    }
}
