#include "prx/libSceAgcDriver/Execution/include/NanSpeculationCounters.hpp"
#include "Recompiler.hpp"
#include <cstdio>

namespace AgcDriver {

void ReportNanSpeculationCounters(const char* label) {
    if (!ShaderRecompiler::NanSpeculation().enabled) return;
    static constexpr const char* Names[ShaderRecompiler::NanSpeculationStageCount] = {"compute", "vertex", "tess_control", "tess_eval", "geometry", "fragment", "local", "mesh"};
    for (std::uint32_t stage = 0; stage < ShaderRecompiler::NanSpeculationStageCount; ++stage) {
        const auto stats = ShaderRecompiler::NanSpeculationStats(stage);
        if (stats.programs == 0u && stats.fellBack == 0u) continue;
        std::fprintf(stderr, "[nan-spec-compile] %s %s programs=%u speculated=%u declined=%u fell_back=%u\n", label, Names[stage], stats.programs, stats.speculated, stats.declined, stats.fellBack);
    }
}

}
