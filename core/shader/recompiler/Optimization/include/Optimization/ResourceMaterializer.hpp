#ifndef CORE_SHADER_RECOMPILIER_OPTIMIZATION_INCLUDE_OPTIMIZATION_RESOURCEMATERIALIZER_HPP
#define CORE_SHADER_RECOMPILIER_OPTIMIZATION_INCLUDE_OPTIMIZATION_RESOURCEMATERIALIZER_HPP

#include "IntermediateRepresentation/IrProgram.hpp"
#include "Optimization/SrtWalker.hpp"
#include <cstdint>
#include <vector>

namespace ShaderRecompiler {

// Why a bindless image table (a T# loaded from a table buffer at a runtime key) was not bound;
// counted on the [bindless] line (APS5_PROFILE_DRAW).
enum class BindlessRejection { Capacity, MaterialScan, NoEntry, Storage, NonUniform, ImageSlots, Count };

class ResourceMaterializer {
public:
    static std::uint32_t EmulatedCompareState(const ShaderInfo& info, const ResourceSnapshot& snapshot, std::uint32_t index);
    void ApplyStaticInterface(IrProgram& program, bool nativeSampleOffsets = true) const;
    static std::vector<ImageResource> RuntimeImageModes(const ImageResource& image);
    static std::uint32_t RuntimeImageMode(const ImageResource& image, const DescriptorValue& descriptor, std::span<const ImageResource> modes);
    static void PrepareImageModes(ShaderInfo& info);
    static const char* UnnormalizedSampleMismatch(const ImageResource& image);
    [[nodiscard]] IrResourcePlan ExtractPlan(const IrProgram& program) const;
    void Materialize(const IrResourcePlan& program, const SrtRuntime& runtime, ResourceSnapshot& snapshot) const;
    static std::uint64_t SpecializationNanoseconds();
    static std::uint32_t BindlessSlots();
    static void CountBindlessRejection(BindlessRejection reason);
};

}

#endif
