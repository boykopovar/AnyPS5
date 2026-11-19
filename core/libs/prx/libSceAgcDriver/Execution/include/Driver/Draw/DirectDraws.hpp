#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DRIVER_DRAW_DIRECTDRAWS_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DRIVER_DRAW_DIRECTDRAWS_HPP

#include "Optimization/ResourceProgram.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/DirectDrawReport.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/DrawCache.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/WalkRanges.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/RuntimeSwitches.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderInputState.hpp"
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <utility>
#include <vector>

namespace AgcDriver::DriverDetail {

struct DirectStage {
    std::uint64_t variant = 0;
    std::uint64_t key = 0;
    std::shared_ptr<const ShaderRecompiler::RecompileResult> donor;
    std::shared_ptr<const ShaderRecompiler::MemoPatch> patch;
    bool addressed = false;
};

class DirectStageStore {
public:
    static constexpr std::size_t Bound = 16384;
    static constexpr std::size_t FrontSlots = 1024;

    std::shared_ptr<const DirectStage> Find(std::uint64_t variant, std::uint64_t key) const;
    std::shared_ptr<const DirectStage> Install(std::shared_ptr<const DirectStage> stage, std::uint64_t tag, bool& installed);
    void Clear();
    std::size_t Size() const;

private:
    struct KeyHash {
        std::size_t operator()(const std::pair<std::uint64_t, std::uint64_t>& key) const;
    };

    mutable std::mutex mutex;
    std::unordered_map<std::pair<std::uint64_t, std::uint64_t>, std::shared_ptr<const DirectStage>, KeyHash> stages;
    std::atomic<std::uint64_t> clears{1};
};

struct DirectStages;
struct DrawRecord;
struct LeanDrawJob;

struct DirectClassKey {
    static constexpr std::size_t MaxStages = 4;

    std::uint64_t shape = 0;
    std::uint64_t layout = 0;
    std::uint64_t indexAddress = 0;
    std::uint32_t indexCount = 0;
    std::uint32_t indexSize = 0;
    std::uint32_t instanceCount = 0;
    std::uint32_t flags = 0;
    std::uint32_t firstVertex = 0;
    std::uint32_t firstInstance = 0;
    std::array<const DirectStage*, MaxStages> stages{};
    std::uint32_t count = 0;
    bool indexed = false;

    static DirectClassKey For(const LeanDrawJob& job, const DirectStages& resolved);
    std::uint64_t Probe() const;
    bool operator==(const DirectClassKey&) const = default;
};

struct DirectClass {
    DirectClassKey key;
    std::uint64_t probe = 0;
    std::array<std::shared_ptr<const DirectStage>, DirectClassKey::MaxStages> stages;
    std::shared_ptr<const DrawRecord> record;
    DirectDraws::Counter refusal = DirectDraws::Counter::Count;

    std::size_t Bytes() const;
};

class DirectClassStore {
public:
    static constexpr std::size_t Bound = 65536;
    static constexpr std::size_t FrontSlots = 1024;

    std::shared_ptr<const DirectClass> Find(const DirectClassKey& key, std::uint64_t probe) const;
    std::shared_ptr<const DirectClass> Install(std::shared_ptr<const DirectClass> entry, std::uint64_t tag, bool& installed);
    bool Refuse(const DirectClass& found, DirectDraws::Counter reason);
    void Evict();
    void Clear();
    std::size_t Size() const;

private:
    struct KeyHash {
        std::size_t operator()(const DirectClassKey& key) const { return static_cast<std::size_t>(key.Probe()); }
    };

    void note() const;

    mutable std::mutex mutex;
    std::unordered_map<DirectClassKey, std::shared_ptr<const DirectClass>, KeyHash> classes;
    std::size_t kept = 0;
    std::size_t bytes = 0;
    std::atomic<std::uint64_t> clears{1};
};

void RefuseDirectClass(const std::shared_ptr<const void>& owner);

class DirectStores {
public:
    static DirectStores& Get();
    std::uint64_t Follow(std::uint64_t deviceSerial);
    bool Current(std::uint64_t tag) const { return tag == epoch.load(std::memory_order_acquire); }

    DirectStageStore stages;
    DirectClassStore classes;

private:
    std::mutex mutex;
    std::atomic<std::uint64_t> epoch{1};
    std::uint64_t deviceSerial = 0;
    Graphics::SwitchSnapshot switches;
    bool tagged = false;
};

struct DirectResolvedStage {
    std::size_t program = 0;
    ShaderRecompiler::ShaderStage stage = ShaderRecompiler::ShaderStage::Vertex;
    std::uint32_t requestOffset = 0;
    std::uint32_t pushOffset = 0;
    std::shared_ptr<const DirectStage> shared;
    std::shared_ptr<const ShaderRecompiler::ResourceCapture> capture;
    ShaderRecompiler::StageSideWords side;
    AddressRanges ranges;
    std::vector<std::uint32_t> values;
};

struct DirectStages {
    std::shared_ptr<const DrawDecode> decode;
    std::vector<std::optional<ShaderRecompiler::ShaderVertexStageInfo>> vertexInfos;
    std::vector<std::vector<Graphics::DecodeRead>> decodeReads;
    std::vector<ShaderRecompiler::MemoryRegion> memory;
    std::vector<ShaderRecompiler::LinkedProgram> linked;
    std::vector<DirectResolvedStage> stages;
    std::vector<ShaderRecompiler::MemoryRegion> regions;
    std::vector<std::uint32_t> vertexLayout;
    std::vector<std::uint32_t> vertexInputs;
    std::shared_ptr<const Graphics::Recorder::WriteRanges> before;
    std::uint64_t generation = 0;
    std::uint64_t tag = 0;
};

class DirectStepClock {
public:
    void Start() { mark = std::chrono::steady_clock::now(); }
    void Lap(DirectDraws::Step step);
    void Report() const;

private:
    std::chrono::steady_clock::time_point mark{};
    std::array<std::uint64_t, static_cast<std::size_t>(DirectDraws::Step::Count)> nanoseconds{};
    std::array<bool, static_cast<std::size_t>(DirectDraws::Step::Count)> reached{};
};

}

#endif
