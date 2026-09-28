#ifndef CORE_SHADER_RECOMPILER_OPTIMIZATION_RESOURCEPROGRAM_HPP
#define CORE_SHADER_RECOMPILER_OPTIMIZATION_RESOURCEPROGRAM_HPP

#include "Recompiler.hpp"
#include "IntermediateRepresentation/IrProgram.hpp"
#include "Optimization/ResourceMaterializer.hpp"
#include <cstdint>
#include <memory>

namespace ShaderRecompiler {

[[nodiscard]] IrProgram PrepareResourceProgram(const RecompileRequest& request);
[[nodiscard]] std::shared_ptr<const IrResourcePlan> GetResourcePlan(const RecompileRequest& request);

// What a driver's capture of a request produces: the plan and the materialization of the words the
// runtime read through it. Recompile(request, capture) compiles from these without a second walk.
struct SourceEntry;
struct ResourceCapture {
    std::shared_ptr<const IrResourcePlan> plan;
    ResourceSnapshot snapshot;
    ResourceSpecialization specialization;
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
struct SourceHandle {
    std::shared_ptr<SourceEntry> source;
};
[[nodiscard]] std::shared_ptr<const SourceHandle> ResolveSource(const RecompileRequest& request);
[[nodiscard]] std::shared_ptr<const ResourceCapture> CaptureResources(const RecompileRequest& request, const SrtRuntime& runtime, const SourceHandle& handle);

}

#endif
