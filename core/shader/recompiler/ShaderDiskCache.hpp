#ifndef CORE_SHADER_RECOMPILER_SHADERDISKCACHE_HPP
#define CORE_SHADER_RECOMPILER_SHADERDISKCACHE_HPP

#include "CompiledVariant.hpp"
#include "IntermediateRepresentation/IrMetadata/ResourcePlan.hpp"
#include "Optimization/include/Optimization/DescriptorBindingBuilder.hpp"
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace ShaderRecompiler::ShaderDiskCache {

inline constexpr std::uint32_t FormatVersion = 37;

inline constexpr std::size_t ArtifactIdentityBytes = 16;

enum class LoadStatus {
    Loaded,
    Absent,
    KeyMismatch,
    Rejected,
};

struct Counters {
    std::uint64_t hits = 0;
    std::uint64_t misses = 0;
    std::uint64_t writes = 0;
    std::uint64_t loadFailures = 0;
    std::uint64_t writeFailures = 0;
    std::uint64_t bytesRead = 0;
    std::uint64_t bytesWritten = 0;
    std::uint64_t planHits = 0;
    std::uint64_t planMisses = 0;
    std::uint64_t planWrites = 0;
    std::uint64_t planLoadFailures = 0;
    std::uint64_t specializedHits = 0;
    std::uint64_t specializedMisses = 0;
    std::uint64_t specializedWrites = 0;
    std::uint64_t specializedLoadFailures = 0;
    std::uint64_t specializedEvictions = 0;
    std::uint64_t bindingPlanHits = 0;
    std::uint64_t bindingPlanMisses = 0;
    std::uint64_t bindingPlanWrites = 0;
    std::uint64_t bindingPlanLoadFailures = 0;
    std::uint64_t prebuiltHits = 0;
    std::uint64_t prebuiltMisses = 0;
    std::uint64_t prebuiltRejected = 0;
    std::uint64_t lookupNanoseconds = 0;
    std::uint64_t readNanoseconds = 0;
    std::uint64_t decodeNanoseconds = 0;
    std::uint64_t loadNanoseconds = 0;
    std::uint64_t maxLoadNanoseconds = 0;
    std::uint64_t prebuiltBytesRead = 0;
    std::uint64_t prebuiltLoadNanoseconds = 0;
};

[[nodiscard]] std::uint64_t SourceVersion();

void BuildKey(const RecompileRequest& request, std::uint32_t hostSubgroupSize, std::vector<std::byte>& key);

[[nodiscard]] std::string EntryName(std::span<const std::byte> key);

[[nodiscard]] std::string SpecializedEntryName(std::span<const std::byte> key);

void EncodeResult(const RecompileResult& result, std::vector<std::byte>& out);
[[nodiscard]] bool DecodeResult(std::span<const std::byte> bytes, RecompileResult& result, std::shared_ptr<const void> owner = nullptr);

[[nodiscard]] std::vector<std::byte> EncodeEntry(std::span<const std::byte> key, const CompiledVariant& variant);
[[nodiscard]] LoadStatus DecodeEntry(std::span<const std::byte> file, std::span<const std::byte> key, CompiledVariant& variant, std::shared_ptr<const void> owner = nullptr);

void BuildPlanKey(const RecompileRequest& request, std::uint32_t hostSubgroupSize, std::vector<std::byte>& key);
[[nodiscard]] bool EncodePlan(const IrResourcePlan& plan, std::vector<std::byte>& out);
[[nodiscard]] bool DecodePlan(std::span<const std::byte> bytes, IrResourcePlan& plan);
[[nodiscard]] std::vector<std::byte> EncodePlanEntry(std::span<const std::byte> key, const IrResourcePlan& plan);
[[nodiscard]] LoadStatus DecodePlanEntry(std::span<const std::byte> file, std::span<const std::byte> key, IrResourcePlan& plan);

struct SpecializedModule {
    SharedSpirv spirv;
    std::vector<std::uint32_t> bindings;
    bool pushData = false;
};

void BuildArtifactIdentity(std::span<const std::byte> artifactKey, std::vector<std::byte>& identity);

void BuildSpecializedKey(std::span<const std::byte> artifactIdentity, std::span<const std::uint64_t> planIdentity, std::span<const std::uint32_t> moduleKey, std::span<const std::uint32_t> vertexClasses, std::span<const PipelineSpecializationConstant> constants, bool registrationDefaults, std::vector<std::byte>& key);

void BuildBindingPlanKey(std::span<const std::byte> artifactIdentity, std::span<const std::uint64_t> planIdentity, std::vector<std::byte>& key);

[[nodiscard]] bool EncodeSpecialized(const SpecializedModule& module, std::vector<std::byte>& out);
[[nodiscard]] bool DecodeSpecialized(std::span<const std::byte> bytes, SpecializedModule& module, std::shared_ptr<const void> owner = nullptr);
[[nodiscard]] std::vector<std::byte> EncodeSpecializedEntry(std::span<const std::byte> key, const SpecializedModule& module);
[[nodiscard]] LoadStatus DecodeSpecializedEntry(std::span<const std::byte> file, std::span<const std::byte> key, SpecializedModule& module, std::shared_ptr<const void> owner = nullptr);

[[nodiscard]] bool EncodeBindingPlan(const DescriptorBindingPlan& plan, std::vector<std::byte>& out);
[[nodiscard]] bool DecodeBindingPlan(std::span<const std::byte> bytes, DescriptorBindingPlan& plan);
[[nodiscard]] std::vector<std::byte> EncodeBindingPlanEntry(std::span<const std::byte> key, const DescriptorBindingPlan& plan);
[[nodiscard]] LoadStatus DecodeBindingPlanEntry(std::span<const std::byte> file, std::span<const std::byte> key, DescriptorBindingPlan& plan);

[[nodiscard]] bool Enabled();
[[nodiscard]] bool SpecializedEnabled();
[[nodiscard]] std::filesystem::path EntryDirectory();

[[nodiscard]] bool Load(std::span<const std::byte> key, CompiledVariant& variant);
void Store(std::vector<std::byte> key, std::shared_ptr<const CompiledVariant> variant);
[[nodiscard]] std::shared_ptr<const CompiledVariant> Persist(std::span<const std::byte> key, const CompiledVariant& variant);
[[nodiscard]] bool LoadPlan(std::span<const std::byte> key, IrResourcePlan& plan);
void StorePlan(std::vector<std::byte> key, std::shared_ptr<const IrResourcePlan> plan);
[[nodiscard]] bool LoadSpecialized(std::span<const std::byte> key, SpecializedModule& module);
void StoreSpecialized(std::vector<std::byte> key, std::shared_ptr<const SpecializedModule> module);
[[nodiscard]] bool LoadBindingPlan(std::span<const std::byte> key, DescriptorBindingPlan& plan);
void StoreBindingPlan(std::vector<std::byte> key, std::shared_ptr<const DescriptorBindingPlan> plan);
void Flush();
void Shutdown();
[[nodiscard]] unsigned Writers();
[[nodiscard]] Counters Totals();

}

#endif
