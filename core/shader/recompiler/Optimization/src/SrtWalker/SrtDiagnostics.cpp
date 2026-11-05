#include "Optimization/SrtWalker/SrtDiagnostics.hpp"

#include <cstdio>
#include <stdexcept>

namespace ShaderRecompiler::Detail {

const char* StageName(IrShaderStage stage) {
    switch (stage) {
        case IrShaderStage::Vertex: return "vertex";
        case IrShaderStage::Pixel: return "pixel";
        case IrShaderStage::Fetch: return "fetch";
        case IrShaderStage::Compute: return "compute";
        default: return "unknown";
    }
}

std::string Diagnostic(const IrResourcePlan& program, std::uint32_t pc, const std::string& message) {
    char hashBuffer[32];
    std::snprintf(hashBuffer, sizeof(hashBuffer), "0x%016llx", static_cast<unsigned long long>(program.shaderHash));
    char pcBuffer[16];
    std::snprintf(pcBuffer, sizeof(pcBuffer), "0x%08x", pc);
    return "shader SRT: hash=" + std::string(hashBuffer) + " stage=" + StageName(program.stage) + " pc=" + std::string(pcBuffer) + " " + message;
}

[[noreturn]] void Fail(const IrResourcePlan& program, std::uint32_t pc, const std::string& message) {
    throw std::runtime_error(Diagnostic(program, pc, message));
}

}
