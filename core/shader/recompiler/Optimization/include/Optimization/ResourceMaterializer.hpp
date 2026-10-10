#ifndef CORE_SHADER_RECOMPILIER_OPTIMIZATION_INCLUDE_OPTIMIZATION_RESOURCEMATERIALIZER_HPP
#define CORE_SHADER_RECOMPILIER_OPTIMIZATION_INCLUDE_OPTIMIZATION_RESOURCEMATERIALIZER_HPP

#include "IntermediateRepresentation/IrProgram.hpp"
#include "Optimization/SrtWalker.hpp"
#include "ImageTableAbi.hpp"
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace ShaderRecompiler {

struct ImageTableElement {
    std::array<std::uint32_t, 8> words{};
    RdnaImageDimension dimension = RdnaImageDimension::Unknown;
    bool depthCompare = false;
    DescriptorBindingKind kind = DescriptorBindingKind::Buffers;
    std::uint32_t samplers = 0;
};

struct ImageTableSamplerElement {
    std::array<std::uint32_t, 4> words{};
    bool compare = false;
    bool point = false;
};

struct ImageTablePoisonRecord {
    std::array<std::uint32_t, 8> words{};
    std::uint32_t dwordCount = 0;
    std::uint32_t resource = 0;
    ImageTableAbi::PoisonReason reason = ImageTableAbi::PoisonReason::OutsideSnapshot;
};

struct ResolvedImageTables {
    std::vector<std::uint32_t> map;
    std::vector<ImageTableElement> elements;
    std::vector<ImageTableSamplerElement> samplers;
    std::vector<ImageTablePoisonRecord> poison;
    std::uint32_t faults = 0;
};

struct RuntimeImageModeMatch {
    std::optional<std::uint32_t> mode;
    ImageTableAbi::PoisonReason reason = ImageTableAbi::PoisonReason::InvalidFormat;
    std::string detail;
};

class ResourceMaterializer {
public:
    static std::uint32_t EmulatedCompareState(const ShaderInfo& info, const ResourceSnapshot& snapshot, std::uint32_t index);
    void ApplyStaticInterface(IrProgram& program, bool nativeSampleOffsets = true) const;
    static std::vector<ImageResource> RuntimeImageModes(const ImageResource& image);
    static RuntimeImageModeMatch TryRuntimeImageMode(const ImageResource& image, const DescriptorValue& descriptor, std::span<const ImageResource> modes);
    static std::uint32_t RuntimeImageMode(const ImageResource& image, const DescriptorValue& descriptor, std::span<const ImageResource> modes);
    static void PrepareImageModes(ShaderInfo& info);
    static ResolvedImageTables ResolveImageTables(const ShaderInfo& info, const ResourceSnapshot& snapshot, std::uint64_t memoKey = 0);
    [[nodiscard]] IrResourcePlan ExtractPlan(const IrProgram& program) const;
    void Materialize(const IrResourcePlan& program, const SrtRuntime& runtime, ResourceSnapshot& snapshot) const;
    static std::uint64_t SpecializationNanoseconds();
};

}

#endif
