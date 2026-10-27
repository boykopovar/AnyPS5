#ifndef CORE_SHADER_RECOMPILER_SHADERDISKCACHE_HPP
#define CORE_SHADER_RECOMPILER_SHADERDISKCACHE_HPP

#include "CompiledVariant.hpp"
#include "IntermediateRepresentation/IrMetadata/ResourcePlan.hpp"
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace ShaderRecompiler::ShaderDiskCache {

inline constexpr std::uint32_t FormatVersion = 48;

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

void EncodeResult(const RecompileResult& result, std::vector<std::byte>& out);
[[nodiscard]] bool DecodeResult(std::span<const std::byte> bytes, RecompileResult& result, std::shared_ptr<const void> owner = nullptr);

[[nodiscard]] std::vector<std::byte> EncodeEntry(std::span<const std::byte> key, const CompiledVariant& variant);
[[nodiscard]] LoadStatus DecodeEntry(std::span<const std::byte> file, std::span<const std::byte> key, CompiledVariant& variant, std::shared_ptr<const void> owner = nullptr);

void BuildPlanKey(const RecompileRequest& request, std::uint32_t hostSubgroupSize, std::vector<std::byte>& key);
[[nodiscard]] bool EncodePlan(const IrResourcePlan& plan, std::vector<std::byte>& out);
[[nodiscard]] bool DecodePlan(std::span<const std::byte> bytes, IrResourcePlan& plan);
[[nodiscard]] std::vector<std::byte> EncodePlanEntry(std::span<const std::byte> key, const IrResourcePlan& plan);
[[nodiscard]] LoadStatus DecodePlanEntry(std::span<const std::byte> file, std::span<const std::byte> key, IrResourcePlan& plan);

[[nodiscard]] bool Enabled();
[[nodiscard]] std::filesystem::path EntryDirectory();

[[nodiscard]] bool Load(std::span<const std::byte> key, CompiledVariant& variant);
void Store(std::vector<std::byte> key, std::shared_ptr<const CompiledVariant> variant);
[[nodiscard]] std::shared_ptr<const CompiledVariant> Persist(std::span<const std::byte> key, const CompiledVariant& variant);
[[nodiscard]] bool LoadPlan(std::span<const std::byte> key, IrResourcePlan& plan);
[[nodiscard]] std::shared_ptr<const IrResourcePlan> LoadSharedPlan(std::span<const std::byte> key, std::uint64_t shaderHash);
void StorePlan(std::vector<std::byte> key, std::shared_ptr<const IrResourcePlan> plan);
void Flush();
void Shutdown();
[[nodiscard]] unsigned Writers();
[[nodiscard]] Counters Totals();

}

#endif
