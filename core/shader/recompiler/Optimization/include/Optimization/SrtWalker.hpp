#ifndef CORE_SHADER_RECOMPILIER_OPTIMIZATION_INCLUDE_OPTIMIZATION_SRTWALKER_HPP
#define CORE_SHADER_RECOMPILIER_OPTIMIZATION_INCLUDE_OPTIMIZATION_SRTWALKER_HPP

#include "IntermediateRepresentation/IrProgram.hpp"
#include "UserDataUse.hpp"

#include <atomic>
#include <cstdint>
#include <limits>
#include <memory>
#include <span>
#include <utility>
#include <vector>

namespace ShaderRecompiler {

using SrtMemoryReader = bool (*)(void* userData, std::uint64_t address, std::uint32_t* value);
using SrtMemoryProbe = bool (*)(void* userData, std::uint64_t address, std::uint64_t bytes);
using SrtMemoryBulkReader = bool (*)(void* userData, std::uint64_t address, std::uint32_t* values, std::uint64_t count);

struct SrtWriteStamp {
    std::uint64_t generation = 0;
    std::uint64_t layout = 0;
};

struct SrtStampCell {
    std::atomic<std::uint64_t> sequence{0};
    std::atomic<std::uint64_t> generation{0};
    std::atomic<std::uint64_t> layout{0};
};

struct SrtGuestWords {
    std::vector<std::pair<std::uint64_t, std::uint32_t>> runs;
    std::vector<std::uint32_t> words;
    std::shared_ptr<SrtStampCell> stamp;
    std::uint64_t low = 0;
    std::uint64_t high = 0;
    bool ordered = false;
};

inline SrtWriteStamp LoadStamp(const SrtGuestWords& words) {
    if (words.stamp == nullptr) return {};
    const auto& cell = *words.stamp;
    const auto sequence = cell.sequence.load(std::memory_order_acquire);
    if ((sequence & 1u) != 0u) return {};
    const SrtWriteStamp stamp{cell.generation.load(std::memory_order_relaxed), cell.layout.load(std::memory_order_relaxed)};
    std::atomic_thread_fence(std::memory_order_acquire);
    return cell.sequence.load(std::memory_order_relaxed) == sequence ? stamp : SrtWriteStamp{};
}

inline void StoreStamp(const SrtGuestWords& words, const SrtWriteStamp& stamp) {
    if (words.stamp == nullptr || stamp.generation == 0u) return;
    auto& cell = *words.stamp;
    auto sequence = cell.sequence.load(std::memory_order_relaxed);
    if ((sequence & 1u) != 0u || !cell.sequence.compare_exchange_strong(sequence, sequence + 1u, std::memory_order_relaxed)) return;
    std::atomic_thread_fence(std::memory_order_release);
    cell.generation.store(stamp.generation, std::memory_order_relaxed);
    cell.layout.store(stamp.layout, std::memory_order_relaxed);
    cell.sequence.store(sequence + 2u, std::memory_order_release);
}

using SrtMemoryStamper = SrtWriteStamp (*)(void* userData, std::uint64_t address, std::uint64_t bytes);
using SrtMemoryUnchanged = bool (*)(void* userData, std::uint64_t address, std::uint64_t bytes, const SrtWriteStamp& stamp);
using SrtMemorySupplier = bool (*)(void* userData, const std::shared_ptr<const SrtGuestWords>& words);
inline constexpr std::size_t SrtDescriptorForceSync = std::numeric_limits<std::size_t>::max();
inline constexpr std::size_t SrtDescriptorEndForceSync = std::numeric_limits<std::size_t>::max() - 1u;
using SrtDescriptorScope = std::uint64_t (*)(void* userData, std::size_t bytes);

// The guest addresses a walk dereferenced (Detail::Evaluator::EvaluateRawRead): the leaf read of
// each pure flat slot (IrResourcePlan::pureFlatSlots) as (flat offset, address), set by the
// flattened loop through `leaf`/`leafSlot` around that slot's evaluation, and every other raw read
// address.
struct SrtReadTrace {
    const IrValue* leaf = nullptr;
    std::uint32_t leafSlot = 0;
    std::vector<std::pair<std::uint32_t, std::uint64_t>> leaves;
    std::vector<std::uint64_t> otherReads;
};

struct SrtRuntime {
    std::span<const std::uint32_t> userData;
    std::uint64_t shaderBase = 0;
    SrtMemoryReader readMemory = nullptr;
    void* userContext = nullptr;
    SrtMemoryReader readSpecializationMemory = nullptr;
    SrtReadTrace* readTrace = nullptr;
    SrtMemoryProbe accessible = nullptr;
    SrtMemoryProbe pendingWrite = nullptr;
    SrtMemoryBulkReader readWords = nullptr;
    SrtMemoryStamper stampWrites = nullptr;
    SrtMemoryUnchanged unchangedSince = nullptr;
    SrtMemorySupplier supplyWords = nullptr;
    SrtUserDataUse* userDataUse = nullptr;
    SrtDescriptorScope descriptorScope = nullptr;
};

struct SrtForceSyncScope {
    const SrtRuntime& runtime;
    bool active = false;
    SrtForceSyncScope(const SrtRuntime& runtime, bool enable) : runtime(runtime), active(enable && runtime.descriptorScope != nullptr) {
        if (active) static_cast<void>(runtime.descriptorScope(runtime.userContext, SrtDescriptorForceSync));
    }
    ~SrtForceSyncScope() {
        if (active) static_cast<void>(runtime.descriptorScope(runtime.userContext, SrtDescriptorEndForceSync));
    }
    SrtForceSyncScope(const SrtForceSyncScope&) = delete;
    SrtForceSyncScope& operator=(const SrtForceSyncScope&) = delete;
};

enum class RuntimeValueType {
    Any,
    Integer
};

class SrtWalker {
public:
    void BuildPlan(IrProgram& program) const;
    [[nodiscard]] bool ValidateRuntimeValue(const IrResourcePlan& program, const IrValue* value, RuntimeValueType type = RuntimeValueType::Any) const;
    void EvaluateUniformValues(const IrResourcePlan& program, std::span<IrValue* const> values, const SrtRuntime& runtime, std::span<std::uint32_t> results) const;
    void EvaluateDescriptorSource(const IrResourcePlan& program, std::uint32_t source, const SrtRuntime& runtime, DescriptorValue& result) const;
    void EvaluateDescriptorSources(const IrResourcePlan& program, std::span<const std::uint32_t> sources, const SrtRuntime& runtime, std::vector<DescriptorValue>& results) const;
    void EvaluateRuntimeSources(const IrResourcePlan& program, std::span<const std::uint32_t> sources, const SrtRuntime& runtime, std::vector<DescriptorValue>& results, std::vector<std::uint32_t>& flat, std::span<const std::uint8_t> cleanFlatSlots, std::vector<std::uint8_t>& activeSources, std::vector<SrtReadPoison>* poison = nullptr, std::uint32_t* nullRootReads = nullptr) const;
    void Walk(const IrResourcePlan& program, const SrtRuntime& runtime, std::vector<std::uint32_t>& flat) const;

};

}

#endif
