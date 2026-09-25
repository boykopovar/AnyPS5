#ifndef CORE_SHADER_RECOMPILER_OPTIMIZATION_RESOURCEPROGRAM_HPP
#define CORE_SHADER_RECOMPILER_OPTIMIZATION_RESOURCEPROGRAM_HPP

#include "Recompiler.hpp"
#include "IntermediateRepresentation/IrProgram.hpp"
#include "Optimization/ResourceMaterializer.hpp"
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
};
[[nodiscard]] std::shared_ptr<const ResourceCapture> CaptureResources(const RecompileRequest& request, const SrtRuntime& runtime);

}

#endif
