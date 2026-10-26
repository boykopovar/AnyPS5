#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/DrawProgramTrace.hpp"
#include <cstdio>
#include <stdexcept>

namespace AgcDriver::DriverDetail {
namespace {

const char* StageName(ShaderRecompiler::ShaderStage stage) {
    using ShaderRecompiler::ShaderStage;
    switch (stage) {
    case ShaderStage::Compute: return "compute";
    case ShaderStage::Vertex: return "vertex";
    case ShaderStage::TessellationControl: return "tess-control";
    case ShaderStage::TessellationEvaluation: return "tess-evaluation";
    case ShaderStage::Geometry: return "geometry";
    case ShaderStage::Fragment: return "fragment";
    case ShaderStage::Local: return "local";
    case ShaderStage::Mesh: return "mesh";
    }
    throw std::invalid_argument("unknown shader stage in draw trace");
}

const char* RoleName(ShaderRecompiler::ProgramRole role) {
    using ShaderRecompiler::ProgramRole;
    switch (role) {
    case ProgramRole::Main: return "main";
    case ProgramRole::GeometryBack: return "geometry-back";
    case ProgramRole::Local: return "local";
    case ProgramRole::Hull: return "hull";
    case ProgramRole::Domain: return "domain";
    case ProgramRole::Fragment: return "fragment";
    }
    throw std::invalid_argument("unknown program role in draw trace");
}

}

std::string FormatDrawProgramTrace(std::uint64_t submission, std::uint32_t queue, std::span<const DrawProgramTraceEntry> programs) {
    char prefix[128];
    const auto size = std::snprintf(prefix, sizeof(prefix), "[draw-programs] submission=%llu queue=0x%x count=%zu",
        static_cast<unsigned long long>(submission), queue, programs.size());
    if (size < 0 || static_cast<std::size_t>(size) >= sizeof(prefix)) throw std::runtime_error("draw program trace prefix overflow");
    std::string result(prefix, static_cast<std::size_t>(size));
    char item[160];
    for (const auto& program : programs) {
        const auto itemSize = std::snprintf(item, sizeof(item), " %s/%s=0x%llx(entry=0x%llx)",
            StageName(program.stage), RoleName(program.role),
            static_cast<unsigned long long>(program.codeAddress), static_cast<unsigned long long>(program.entryAddress));
        if (itemSize < 0 || static_cast<std::size_t>(itemSize) >= sizeof(item)) throw std::runtime_error("draw program trace entry overflow");
        result.append(item, static_cast<std::size_t>(itemSize));
    }
    return result;
}

}
