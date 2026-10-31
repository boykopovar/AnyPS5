#ifndef CORE_SHADER_RECOMPILIER_OPTIMIZATION_INCLUDE_OPTIMIZATION_RESOURCEMATERIALIZER_HPP
#define CORE_SHADER_RECOMPILIER_OPTIMIZATION_INCLUDE_OPTIMIZATION_RESOURCEMATERIALIZER_HPP

#include "IntermediateRepresentation/IrProgram.hpp"
#include "Optimization/SrtWalker.hpp"
#include "DeviceImageTableAbi.hpp"
#include "ImageTableAbi.hpp"
#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace ShaderRecompiler {

struct ImageTableElement {
    std::array<std::uint32_t, 8> words{};
    RdnaImageDimension dimension = RdnaImageDimension::Unknown;
    bool depthCompare = false;
    DescriptorBindingKind kind = DescriptorBindingKind::Buffers;
    std::uint32_t samplers = 0;
    bool atomic = false;
    bool atomic64 = false;
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

struct ImageTableReuseCounts {
    std::uint64_t unwritten = 0;
    std::uint64_t compared = 0;
    std::uint64_t wordsRead = 0;
    std::uint64_t shared = 0;
    std::uint64_t columns = 0;
    std::uint64_t resolutions = 0;
    std::uint64_t resolutionsShared = 0;
};

struct RuntimeImageModeMatch {
    std::optional<std::uint32_t> mode;
    ImageTableAbi::PoisonReason reason = ImageTableAbi::PoisonReason::InvalidFormat;
    std::string detail;
};

struct DeviceImageInterface {
    std::uint64_t id = 0;
    ImageResource image;
    std::vector<ImageResource> modes;
};

class ConvertedSampleRequired : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class ResourceMaterializer {
public:
    static std::uint32_t EmulatedCompareState(const ShaderInfo& info, const ResourceSnapshot& snapshot, std::uint32_t index);
    void ApplyStaticInterface(IrProgram& program, bool nativeSampleOffsets = true) const;
    static std::vector<ImageResource> RuntimeImageModes(const ImageResource& image);
    static RuntimeImageModeMatch TryRuntimeImageMode(const ImageResource& image, const DescriptorValue& descriptor, std::span<const ImageResource> modes);
    static std::uint32_t RuntimeImageMode(const ImageResource& image, const DescriptorValue& descriptor, std::span<const ImageResource> modes);
    static void PrepareImageModes(ShaderInfo& info);
    static const char* UnnormalizedSampleMismatch(const ImageResource& image);
    static std::shared_ptr<const ResolvedImageTables> ResolveImageTablesShared(const ShaderInfo& info, const ResourceSnapshot& snapshot, std::uint64_t memoKey);
    static bool NullImageDescriptor(const ImageResource& image, const DescriptorValue& descriptor);
    static ResolvedImageTables ResolveImageTables(const ShaderInfo& info, const ResourceSnapshot& snapshot, std::uint64_t memoKey = 0);
    static std::uint64_t DeviceImageInterfaceId(const ImageResource& image, std::span<const ImageResource> modes);
    static std::uint64_t RegisterDeviceImageInterface(const ImageResource& image, std::span<const ImageResource> modes);
    static void RegisterDeviceImageInterfaces(const ShaderInfo& info);
    static std::shared_ptr<const DeviceImageInterface> FindDeviceImageInterface(std::uint64_t id);
    static constexpr std::size_t MaxDeviceImageCandidates = 16;
    static std::vector<DeviceImageTableAbi::Expectation> ExpectedDeviceImages(const IrResourcePlan& plan, const SrtRuntime& runtime, bool& complete);
    [[nodiscard]] IrResourcePlan ExtractPlan(const IrProgram& program) const;
    static void CompactPlan(IrResourcePlan& plan);
    void Materialize(const IrResourcePlan& program, const SrtRuntime& runtime, ResourceSnapshot& snapshot) const;
    static std::uint64_t SpecializationNanoseconds();
    static ImageTableReuseCounts TableReuse();
    static std::uint64_t EvaluateNanoseconds();
    static std::uint64_t MaterializeNanoseconds();
};

}

#endif
