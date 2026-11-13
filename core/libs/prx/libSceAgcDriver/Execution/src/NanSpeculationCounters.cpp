#include "prx/libSceAgcDriver/Execution/include/NanSpeculationCounters.hpp"
#include "prx/libSceAgcDriver/Execution/include/Pm4.hpp"
#include "Recompiler.hpp"
#include <cstdio>

namespace AgcDriver {

namespace {

NanSpeculationCount ReadSlot(std::uint32_t slot) {
    const auto base = Pm4::GdsAddress() + ShaderRecompiler::NanSpeculationCounterBase + slot * 8u;
    const auto* words = reinterpret_cast<const volatile std::uint32_t*>(base);
    return {words[0], words[1]};
}

}

std::optional<NanSpeculationCount> ReadNanSpeculationCount(std::uint64_t address, std::uint32_t stage) {
    for (const auto& site : ShaderRecompiler::NanSpeculationCounterSites()) {
        if (site.address == address && site.stage == stage) return ReadSlot(site.slot);
    }
    return std::nullopt;
}

void ReportNanSpeculationCounters(const char* label) {
    if (!ShaderRecompiler::NanSpeculation().count) return;
    std::uint64_t entered = 0;
    std::uint64_t exact = 0;
    for (const auto& site : ShaderRecompiler::NanSpeculationCounterSites()) {
        const auto count = ReadSlot(site.slot);
        if (count.entered == 0u && count.exact == 0u) continue;
        entered += count.entered;
        exact += count.exact;
        std::fprintf(stderr, "[nan-spec-count] %s 0x%llx stage=%u entered=%u exact=%u (%.4f%%)\n", label, static_cast<unsigned long long>(site.address), site.stage, count.entered, count.exact, count.entered != 0u ? 100.0 * count.exact / count.entered : 0.0);
    }
    std::fprintf(stderr, "[nan-spec-count] %s total entered=%llu exact=%llu (%.4f%%)\n", label, static_cast<unsigned long long>(entered), static_cast<unsigned long long>(exact), entered != 0u ? 100.0 * static_cast<double>(exact) / static_cast<double>(entered) : 0.0);
}

}
