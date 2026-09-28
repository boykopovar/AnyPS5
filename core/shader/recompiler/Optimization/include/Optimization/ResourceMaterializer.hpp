#ifndef CORE_SHADER_RECOMPILIER_OPTIMIZATION_INCLUDE_OPTIMIZATION_RESOURCEMATERIALIZER_HPP
#define CORE_SHADER_RECOMPILIER_OPTIMIZATION_INCLUDE_OPTIMIZATION_RESOURCEMATERIALIZER_HPP

#include "IntermediateRepresentation/IrProgram.hpp"
#include "Optimization/SrtWalker.hpp"
#include <cstdint>
#include <vector>

namespace ShaderRecompiler {

struct ResourceSpecialization {
    struct Buffer {
        std::uint32_t packedStride = 0;
        IrBufferFormat descriptorFormat = IrBufferFormat::Invalid;
        std::uint32_t descriptorSwizzle = ShaderImageIdentitySwizzle;

        bool operator==(const Buffer& other) const;
    };

    struct Image {
        IrTextureNumericClass numericClass = IrTextureNumericClass::Unsupported;
        RdnaImageDimension dimension = RdnaImageDimension::Unknown;
        std::uint32_t mipCount = 1;
        IrBufferFormat conversionFormat = IrBufferFormat::Invalid;
        std::uint32_t shaderSwizzle = ShaderImageIdentitySwizzle;
        std::uint32_t indirectRoot = ImageResource::NoIndirectImage;
        std::uint32_t indirectMappingOffset = 0;
        std::uint32_t indirectSearchIterations = 0;
        bool cube = false;
        bool fmask = false;

        bool operator==(const Image& other) const;
    };

    std::vector<Buffer> buffers;
    std::vector<Image> images;

    bool operator==(const ResourceSpecialization& other) const;

    std::vector<std::uint32_t> boundDescriptors;
};

// Why a bindless image table (a T# loaded from a table buffer at a runtime key) was not bound;
// counted on the [bindless] line (APS5_PROFILE_DRAW).
enum class BindlessRejection { Capacity, MaterialScan, NoEntry, Storage, NonUniform, ImageSlots, Count };

class ResourceMaterializer {
public:
    void Apply(IrProgram& program, const ResourceSpecialization& specialization) const;
    [[nodiscard]] IrResourcePlan ExtractPlan(const IrProgram& program) const;
    void Materialize(const IrResourcePlan& program, const SrtRuntime& runtime, ResourceSnapshot& snapshot, ResourceSpecialization& specialization) const;
    // APS5_PROFILE_DRAW: the time Materialize spent building specializations, over every call.
    static std::uint64_t SpecializationNanoseconds();
    // The slots every bindless image table binds (APS5_BINDLESS_SLOTS, default 16, 1..48).
    static std::uint32_t BindlessSlots();
    static void CountBindlessRejection(BindlessRejection reason);
};

}

#endif
