#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DRIVER_DRAW_DRAWPROGRAMTRACE_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DRIVER_DRAW_DRAWPROGRAMTRACE_HPP

#include "Recompiler.hpp"
#include <cstdint>
#include <span>
#include <string>

namespace AgcDriver::DriverDetail {

struct DrawProgramTraceEntry {
    ShaderRecompiler::ShaderStage stage;
    ShaderRecompiler::ProgramRole role;
    std::uint64_t codeAddress;
    std::uint64_t entryAddress;
};

std::string FormatDrawProgramTrace(std::uint64_t submission, std::uint32_t queue, std::span<const DrawProgramTraceEntry> programs);

}

#endif
