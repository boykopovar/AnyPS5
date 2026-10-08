#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DRIVER_DRAWCACHE_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DRIVER_DRAWCACHE_HPP

#include "prx/libSceAgcDriver/Execution/include/Driver/Dispatch/DispatchCache.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/DrawPlan.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderInputState.hpp"
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <list>
#include <memory>
#include <unordered_map>
#include <vector>

namespace AgcDriver::DriverDetail {

struct DrawRecipeRecord {
    std::vector<std::weak_ptr<const DispatchVariant>> stages;
    std::shared_ptr<const DrawRecipe> recipe;
    bool Matches(std::span<const std::shared_ptr<DispatchVariant>> variants) const;
    bool Expired() const;
};

struct DrawEntry {

    std::shared_ptr<const DrawPlan> plan;
    std::vector<DrawProgram> programs;

    std::vector<std::vector<std::shared_ptr<DispatchVariant>>> stages;

    std::atomic<std::shared_ptr<const std::vector<DrawRecipeRecord>>> recipes;
};

struct DrawCacheSlot {
    std::shared_ptr<DrawEntry> entry;
    std::list<std::uint64_t>::iterator order;
    std::uint64_t touched = 0;
};

class DrawPlanCache {
public:
    explicit DrawPlanCache(std::size_t capacity);
    DrawPlanCache(const DrawPlanCache&) = delete;
    DrawPlanCache& operator=(const DrawPlanCache&) = delete;
    std::shared_ptr<DrawEntry> Find(std::uint64_t shape);
    void Keep(std::uint64_t shape, std::shared_ptr<DrawEntry> entry);
    bool Admit(std::uint64_t key);
    std::size_t Size() const { return entries.size(); }

private:
    struct Entry {
        std::shared_ptr<DrawEntry> draw;
        std::list<std::uint64_t>::iterator order;
    };
    struct Seen {
        std::uint64_t key = 0;
        bool occupied = false;
    };
    std::unordered_map<std::uint64_t, Entry> entries;
    std::list<std::uint64_t> order;
    std::vector<Seen> seen;
};

enum class DrawMiss : std::size_t { FrontDiffering, FragmentDiffering, OtherDiffering, Layout, Gate, Stages, Count };

struct DrawEntryCounters {
    std::uint64_t lookups = 0, absent = 0, hits = 0, stageValidations = 0, stageEqual = 0, variantsCompared = 0, inserts = 0, variantsInserted = 0, variantsEvicted = 0, present = 0, unstable = 0, touches = 0, verifyHits = 0, verifyMismatches = 0;
    std::array<std::uint64_t, static_cast<std::size_t>(DrawMiss::Count)> misses{};
    std::array<std::uint64_t, MaxDispatchVariants> variantHitsByRank{};
    double validateUs = 0;

    std::uint64_t registerKeyLookups = 0, registerKeyHits = 0, decodeSkipped = 0, decodePartial = 0, facadeMismatches = 0, verifyDecodes = 0, verifyDecodeMismatches = 0;
    double keyUs = 0;
    std::uint64_t absentNewRegisters = 0, absentUserWords = 0, absentEvicted = 0, absentNeverInserted = 0, evictions = 0;
    std::uint64_t differingSameRuns = 0, differingWords = 0, differingRunsChanged = 0;
    std::uint64_t dataHits = 0, dataStagesReused = 0, dataStagesCompiled = 0, dataVerified = 0;
    std::uint64_t admissionsSkipped = 0;
    std::chrono::steady_clock::time_point lastReport = std::chrono::steady_clock::now();
};

enum class DrawVerdict { Drawn, Nothing, Rejected };

struct StageCapture {
    std::shared_ptr<const ShaderRecompiler::RecompileResult> compiled;
    std::vector<ShaderRecompiler::MemoryRegion> regions;
    std::uint64_t forgetSerial = 0;
    std::uint32_t pushOffset = 0;
};

struct DrawStage {
    std::optional<ShaderRecompiler::ShaderVertexStageInfo> vertexInfo;
    std::vector<Graphics::DecodeRead> decodeReads;
    StageCapture capture;
    std::shared_ptr<DispatchVariant> matched;
    std::vector<ShaderRecompiler::MemoryRegion> matchedRegions;
    std::shared_ptr<DispatchVariant> fresh;
    std::vector<std::pair<std::shared_ptr<DispatchVariant>, std::vector<ShaderRecompiler::MemoryRegion>>> candidates;
    const ShaderRecompiler::RecompileResult* result = nullptr;
    std::uint32_t pushOffset = 0;
    std::size_t compiledIndex = 0;
    bool recompiled = false;
    bool reused = false;
};

}

#endif
