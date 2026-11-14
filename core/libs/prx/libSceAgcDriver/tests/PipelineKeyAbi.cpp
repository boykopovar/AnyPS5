#include "PipelineKeyAbi.hpp"
#include <cstdio>
#include <limits>

const char* PipelineKeyAbiPlatform() {
#ifdef _WIN32
    return "Windows x86-64 native Win64";
#else
    return "Linux x86-64 native SysV";
#endif
}

unsigned PipelineKeyAbiDifferences(const PipelineKeyAbiObservation& observation) {
    constexpr std::array names{"rbx", "rbp", "r12", "r13", "r14", "r15", "rdi", "rsi"};
#ifdef _WIN32
    constexpr unsigned generalCount = 8;
#else
    constexpr unsigned generalCount = 6;
#endif
    unsigned changed = 0;
    for (unsigned i = 0; i < generalCount; ++i) {
        if (observation.before.general[i] == observation.after.general[i]) continue;
        ++changed;
        std::printf("%s before=%016llx after=%016llx\n", names[i], static_cast<unsigned long long>(observation.before.general[i]), static_cast<unsigned long long>(observation.after.general[i]));
    }
#ifdef _WIN32
    for (unsigned i = 0; i < observation.before.vector.size(); ++i) {
        const auto& before = observation.before.vector[i];
        const auto& after = observation.after.vector[i];
        if (before == after) continue;
        ++changed;
        std::printf("xmm%u before=%016llx%016llx after=%016llx%016llx\n", i + 6, static_cast<unsigned long long>(before[1]), static_cast<unsigned long long>(before[0]), static_cast<unsigned long long>(after[1]), static_cast<unsigned long long>(after[0]));
    }
#endif
    return changed;
}

int PipelineKeyAbiSynthetic(bool clobbering) {
    PipelineKeyAbiObservation inner;
    PipelineKeyAbiObservation outer;
    PipelineKeyAbiNestedCall nested{clobbering ? PipelineKeyAbiClobbering : PipelineKeyAbiConforming, &inner};
    VkPipelineBinaryKeyKHR key{VK_STRUCTURE_TYPE_PIPELINE_BINARY_KEY_KHR};
    PipelineKeyAbiProbe(PipelineKeyAbiNested, reinterpret_cast<VkDevice>(1), reinterpret_cast<const VkPipelineCreateInfoKHR*>(&nested), &key, &outer, 0xa5a5a5a5a5a5a5a5ull);
    const auto changed = PipelineKeyAbiDifferences(inner);
    const auto callerChanged = PipelineKeyAbiDifferences(outer);
#ifdef _WIN32
    constexpr unsigned registerCount = 18;
#else
    constexpr unsigned registerCount = 6;
#endif
    bool seeded = true;
    bool distinct = true;
    for (unsigned i = 0; i < (registerCount == 18 ? 8u : 6u); ++i) {
        const auto value = inner.before.general[i];
        distinct = distinct && value != outer.before.general[i];
        seeded = seeded && (value >> 32u) != 0 && static_cast<std::uint32_t>(value) != 0;
        for (unsigned j = 0; j < i; ++j) seeded = seeded && value != inner.before.general[j];
        if (clobbering) seeded = seeded && inner.after.general[i] == (value & 0xffffffff00000000ull);
    }
#ifdef _WIN32
    for (unsigned i = 0; i < inner.before.vector.size(); ++i) {
        for (unsigned half = 0; half < 2; ++half) {
            const auto value = inner.before.vector[i][half];
            distinct = distinct && value != outer.before.vector[i][half];
            seeded = seeded && (value >> 32u) != 0 && static_cast<std::uint32_t>(value) != 0;
            if (clobbering) seeded = seeded && inner.after.vector[i][half] == std::numeric_limits<std::uint64_t>::max();
        }
    }
#endif
    const bool passed = seeded && distinct && inner.result == VK_SUCCESS && outer.result == VK_SUCCESS && key.keySize == 1 && key.key[0] == 0x5a && changed == (clobbering ? registerCount : 0) && callerChanged == 0;
    std::printf("%s; compiler=%s; synthetic=%s; distinct_seeds=%u; changed=%u; caller_changed=%u; %s\n", PipelineKeyAbiPlatform(), __VERSION__, clobbering ? "clobbering" : "conforming", static_cast<unsigned>(distinct), changed, callerChanged, passed ? "PASS" : "FAIL");
    return passed ? 0 : 1;
}
