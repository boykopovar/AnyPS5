#ifndef CORE_SHADER_RECOMPILER_OPTIMIZATION_RESOURCEPROGRAM_HPP
#define CORE_SHADER_RECOMPILER_OPTIMIZATION_RESOURCEPROGRAM_HPP

#include "Recompiler.hpp"
#include "IntermediateRepresentation/IrProgram.hpp"
#include "Optimization/SrtWalker.hpp"
#include <cstdint>
#include <memory>

namespace ShaderRecompiler {

[[nodiscard]] IrProgram PrepareResourceProgram(const RecompileRequest& request);
[[nodiscard]] std::shared_ptr<const IrResourcePlan> GetResourcePlan(const RecompileRequest& request);

struct SourceEntry;
struct ResourceCapture {
    std::shared_ptr<const IrResourcePlan> plan;
    ResourceSnapshot snapshot;
    // The cache entry the plan belongs to; null when the request bypasses the cache.
    std::shared_ptr<SourceEntry> source;
    // The walk's read addresses when the plan has pure flat slots (the leaf of each pure slot,
    // and every other read sorted and deduplicated); empty otherwise.
    SrtReadTrace readTrace;
    // APS5_PROFILE_DRAW: what CaptureResources spent resolving the source (the stage inputs, the
    // key over the code, the plan) before the walk.
    std::uint64_t sourceNanoseconds = 0;
};
[[nodiscard]] std::shared_ptr<const ResourceCapture> CaptureResources(const RecompileRequest& request, const SrtRuntime& runtime);

// The resolved source of a request (its cache entry with the plan built), for a driver that
// memoizes it per registered shader: ResolveSource is what CaptureResources does before the walk
// (the stage input validation, the key over the code, the lookup, the first-sight plan build), and
// the overload below captures over a handle without repeating it. A handle stays valid for every
// request with the same code and the same cache key fields (RecompileCacheKey::ContextHash plus
// the target); the vertex stages' input validation is repeated per capture because it reads V#
// fields the key does not cover. Null for a request that bypasses the cache (useCache false).
struct CompiledVariant;
struct SourceHandle {
    std::shared_ptr<SourceEntry> source;
    std::shared_ptr<const CompiledVariant> artifact;
    std::vector<std::uint64_t> staticKey;
};
class PreparedShaderInvocation {
public:
    static std::optional<PreparedShaderInvocation> TryCreate(const RecompileRequest& request, const std::shared_ptr<const SourceHandle>& handle);
    static std::optional<PreparedShaderInvocation> TryCreate(const RecompileRequest& request, const std::shared_ptr<const SourceHandle>& handle, std::span<const std::uint64_t> key);
    const RecompileRequest& Request() const { return request; }
    std::shared_ptr<const ResourceCapture> Capture(const SrtRuntime& runtime) const;
    std::shared_ptr<const RecompileResult> Materialize(const ResourceCapture& capture) const;

private:
    PreparedShaderInvocation(const RecompileRequest& request, const std::shared_ptr<const SourceHandle>& handle);
    RecompileRequest request;
    std::shared_ptr<const SourceHandle> handle;
};
[[nodiscard]] std::span<const std::uint32_t> GetPreparedCode(const SourceHandle& handle);
[[nodiscard]] std::span<const CapturedShaderCall> GetPreparedCalls(const SourceHandle& handle);
[[nodiscard]] std::shared_ptr<const SourceHandle> PrepareShader(const RecompileRequest& request);
[[nodiscard]] bool MatchesPreparedShader(const RecompileRequest& request, const SourceHandle& handle);
void BuildPreparedShaderKey(const RecompileRequest& request, std::vector<std::uint64_t>& key);
[[nodiscard]] bool MatchesPreparedShader(const RecompileRequest& request, const SourceHandle& handle, std::span<const std::uint64_t> key);
[[nodiscard]] const CompiledShaderArtifact& GetPreparedArtifact(const SourceHandle& handle);
[[nodiscard]] std::shared_ptr<const RecompileResult> MaterializeShader(const RecompileRequest& request, const ResourceCapture& capture, const SourceHandle& handle);
[[nodiscard]] std::shared_ptr<const SourceHandle> ResolveSource(const RecompileRequest& request);
[[nodiscard]] std::shared_ptr<const ResourceCapture> CaptureResources(const RecompileRequest& request, const SrtRuntime& runtime, const SourceHandle& handle);

}

#endif
