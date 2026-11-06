#include <spirv-tools/libspirv.hpp>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace {

std::array<std::vector<std::uint32_t>, 3> modules;

bool Validate(const std::vector<std::uint32_t>& words) {
    spvtools::SpirvTools tools(SPV_ENV_VULKAN_1_2);
    return tools.Validate(words);
}

bool Check(const char* phase) {
    const bool function = Validate(modules[0]);
    const bool phi = Validate(modules[1]);
    const bool invalidRejected = !Validate(modules[2]);
    std::fprintf(stderr, "%s: function=%d phi=%d invalidRejected=%d\n", phase, function, phi, invalidRejected);
    return function && phi && invalidRejected;
}

void ValidateAtExit() {
    if (!Check("exit validation")) std::_Exit(EXIT_FAILURE);
}

}

int main() {
    if (std::atexit(ValidateAtExit) != 0) return EXIT_FAILURE;
    const std::string prefix = R"(OpCapability Shader
OpMemoryModel Logical GLSL450
OpEntryPoint GLCompute %main "main"
OpExecutionMode %main LocalSize 1 1 1
%void = OpTypeVoid
%fn = OpTypeFunction %void
%bool = OpTypeBool
%true = OpConstantTrue %bool
%uint = OpTypeInt 32 0
%one = OpConstant %uint 1
%two = OpConstant %uint 2
)";
    const std::string function = "%main = OpFunction %void None %fn\n%entry = OpLabel\n";
    const std::string tail = "OpReturn\nOpFunctionEnd\n";
    const std::string phi = R"(OpSelectionMerge %merge None
OpBranchConditional %true %left %right
%left = OpLabel
OpBranch %merge
%right = OpLabel
OpBranch %merge
%merge = OpLabel
%value = OpPhi %uint %one %left %two %right
)";
    spvtools::SpirvTools tools(SPV_ENV_VULKAN_1_2);
    if (!tools.Assemble(prefix + function + tail, &modules[0]) ||
        !tools.Assemble(prefix + function + phi + tail, &modules[1]) ||
        !tools.Assemble(prefix + "%main = OpFunction %void None %void\n%entry = OpLabel\n" + tail, &modules[2])) return EXIT_FAILURE;
    return Check("warm validation") ? EXIT_SUCCESS : EXIT_FAILURE;
}
