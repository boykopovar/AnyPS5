#ifndef CORE_SHADER_RECOMPILER_RUNTIMEABI_HPP
#define CORE_SHADER_RECOMPILER_RUNTIMEABI_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <stdexcept>

namespace ShaderRecompiler::RuntimeAbi {

inline constexpr std::uint32_t Version = 14u;
inline constexpr std::uint32_t DescriptorSet = 0u;
inline constexpr std::uint32_t FragmentDescriptorSet = 1u;
inline constexpr std::uint32_t StageCount = 4u;
inline constexpr std::uint32_t PushConstantDwords = 32u;
inline constexpr std::uint32_t FirstImageBinding = 1u;
inline constexpr std::uint32_t FirstComparisonImageBinding = 22u;
inline constexpr std::uint32_t FirstStorageImageBinding = 29u;
inline constexpr std::uint32_t ImageBindingCount = 56u;
inline constexpr bool GpuHeapStorageKind(std::uint32_t kind) { return kind >= FirstStorageImageBinding && kind < FirstImageBinding + ImageBindingCount; }
inline constexpr std::uint32_t StorageKindStride = 7u;
inline constexpr std::uint32_t FirstUintStorageImageBinding = FirstStorageImageBinding + StorageKindStride;
inline constexpr std::uint32_t FirstAtomicImageBinding = FirstStorageImageBinding + 2u * StorageKindStride;
inline constexpr std::uint32_t FirstAtomic64ImageBinding = FirstStorageImageBinding + 3u * StorageKindStride;
inline constexpr std::uint32_t SamplerSlotWords = 2u;
inline constexpr std::uint32_t BufferAddressWords = 4u;
inline constexpr bool AtomicImageKind(std::uint32_t kind) { return kind >= FirstAtomicImageBinding && kind < FirstAtomic64ImageBinding; }
inline constexpr bool Atomic64ImageKind(std::uint32_t kind) { return kind >= FirstAtomic64ImageBinding && kind < FirstImageBinding + ImageBindingCount; }
inline constexpr bool UintStorageImageKind(std::uint32_t kind) { return kind >= FirstUintStorageImageBinding && kind < FirstAtomicImageBinding; }

enum class HeapPart : std::uint32_t { Sampled, Storage, Sampler, Atomic, Atomic64, UintStorage };

inline constexpr HeapPart StorageHeapPart(std::uint32_t kind) {
    if (AtomicImageKind(kind)) return HeapPart::Atomic;
    if (Atomic64ImageKind(kind)) return HeapPart::Atomic64;
    if (UintStorageImageKind(kind)) return HeapPart::UintStorage;
    return HeapPart::Storage;
}

inline constexpr std::uint32_t HeapPartFirstBinding(HeapPart part) {
    switch (part) {
    case HeapPart::Sampled: return FirstImageBinding;
    case HeapPart::Storage: return FirstStorageImageBinding;
    case HeapPart::Atomic: return FirstAtomicImageBinding;
    case HeapPart::Atomic64: return FirstAtomic64ImageBinding;
    case HeapPart::UintStorage: return FirstUintStorageImageBinding;
    case HeapPart::Sampler: break;
    }
    return 0u;
}

enum class Binding : std::uint32_t {
    Buffers = 0u,
    Samplers = 57u,
    Gds = 58u,
    BdaPagetable = 59u,
    FaultBuffer = 60u,
    FlattenedSrt = 61u,
    ShaderData = 62u,
    SamplerTable = 63u,
    ImageTableMap = 64u,
    Count = 65u,
    ImageTable = 256u,
    ImageStorageTable = 257u
};

enum class Stage : std::uint32_t { Main, Fragment, TessellationControl, TessellationEvaluation };

inline constexpr Stage StageGroup(bool fragment, bool tessellationControl, bool tessellationEvaluation) {
    if (fragment) return Stage::Fragment;
    if (tessellationControl) return Stage::TessellationControl;
    if (tessellationEvaluation) return Stage::TessellationEvaluation;
    return Stage::Main;
}

inline constexpr std::uint32_t UserDataCapacity = 128u;
inline constexpr std::uint32_t BufferCapacity = 128u;
inline constexpr std::uint32_t ImageCapacity = 256u;
inline constexpr std::uint32_t SampledHeapCapacity = 64u;
inline constexpr std::uint32_t BindlessTableSlots = 16u;
inline constexpr std::uint32_t StorageMipSlots = 16u;
inline constexpr std::uint32_t StorageHeapCapacity = 64u;
inline constexpr std::uint32_t SamplerHeapCapacity = 64u;
inline constexpr std::uint32_t GpuImageHeapSlots = static_cast<std::uint32_t>(Binding::Samplers) * 4096u;
inline constexpr std::uint32_t GpuSamplerHeapSlots = 2048u;
inline constexpr std::uint32_t GpuDataSet = 2u;
inline constexpr std::uint32_t GpuGdsSet = 4u;
inline constexpr std::uint32_t GdsBytes = 0x10000u;
inline constexpr std::uint32_t PushDataDwords = 64u;
inline constexpr std::uint32_t RootDword = 38u;
inline constexpr std::uint32_t RootRingAddress = 0u;
inline constexpr std::uint32_t RootPagetableAddress = 2u;
inline constexpr std::uint32_t RootFaultAddress = 4u;
inline constexpr std::uint32_t RootGdsAddress = 6u;
inline constexpr std::uint32_t RootPagetableDwords = 8u;
inline constexpr std::uint32_t RootStencilReference = 9u;
inline constexpr std::uint32_t RootStageRecords = 10u;
enum class RootStageField : std::uint32_t { ShaderData, FlattenedSrt, SrtDwords, TableMap, Count };
inline constexpr std::uint32_t RootStageDword(Stage stage, RootStageField field) { return RootStageRecords + static_cast<std::uint32_t>(stage) * static_cast<std::uint32_t>(RootStageField::Count) + static_cast<std::uint32_t>(field); }
inline constexpr std::uint32_t RootDwords = RootStageRecords + StageCount * static_cast<std::uint32_t>(RootStageField::Count);
static_assert(RootDword * 4u % 8u == 0u && RootDword + RootDwords == PushDataDwords);
struct HeapSlotWord {
    std::uint32_t dword = 0;
    std::uint32_t kind = 0;
    std::uint32_t element = 0;

    bool operator==(const HeapSlotWord& other) const = default;
};
inline constexpr std::uint32_t StateExportMappings = 0u;
inline constexpr std::uint32_t StateDwords = 2u;
inline constexpr std::uint32_t StateVertexSelectors = 0u;
inline constexpr std::uint32_t VertexSelectorsPerDword = 2u;
inline constexpr std::uint32_t ImageModeMask = 0xffu;
inline constexpr std::uint32_t CompareStateShift = 9u;
inline constexpr std::uint32_t CompareStateMask = 0x1fffu;

struct ResourceMetadata {
    std::uint32_t binding;
    std::uint32_t firstElement;
    std::uint32_t elementCount;
    std::uint32_t flags;
    std::array<std::uint32_t, 8> descriptor;
};

struct ShaderData {
    std::uint32_t version;
    std::uint32_t imageCount;
    std::uint32_t samplerCount;
    std::uint32_t reserved;
    std::array<std::uint32_t, UserDataCapacity> userData;
    std::array<std::uint32_t, BufferCapacity / 4u> bufferOffsets;
    std::array<std::uint32_t, 4> dispatchThreadLimit;
    std::array<ResourceMetadata, ImageCapacity> images;
    std::array<ResourceMetadata, SamplerHeapCapacity> samplers;
    std::array<std::uint32_t, 8> exportMappings;
};

inline constexpr std::uint32_t UserDataDword = offsetof(ShaderData, userData) / sizeof(std::uint32_t);
inline constexpr std::uint32_t BufferOffsetsDword = offsetof(ShaderData, bufferOffsets) / sizeof(std::uint32_t);
inline constexpr std::uint32_t DispatchThreadLimitDword = offsetof(ShaderData, dispatchThreadLimit) / sizeof(std::uint32_t);
inline constexpr std::uint32_t ExportMappingsDword = offsetof(ShaderData, exportMappings) / sizeof(std::uint32_t);
inline constexpr std::uint32_t ShaderDataDwords = sizeof(ShaderData) / sizeof(std::uint32_t);

inline std::uint32_t HeapCapacity(Binding binding) {
    const auto value = static_cast<std::uint32_t>(binding);
    if (value >= FirstImageBinding && value < FirstStorageImageBinding) return SampledHeapCapacity;
    if (value >= FirstStorageImageBinding && value < static_cast<std::uint32_t>(Binding::Samplers)) return StorageHeapCapacity;
    if (binding == Binding::Samplers) return SamplerHeapCapacity;
    throw std::runtime_error("Shader runtime ABI: binding is not a typed heap");
}

static_assert(std::is_standard_layout_v<ResourceMetadata> && std::is_trivially_copyable_v<ResourceMetadata> && sizeof(ResourceMetadata) == 48u);
static_assert(std::is_standard_layout_v<ShaderData> && std::is_trivially_copyable_v<ShaderData> && sizeof(ShaderData) == 16064u);
static_assert(UserDataDword == 4u && BufferOffsetsDword == 132u && DispatchThreadLimitDword == 164u);
static_assert(ExportMappingsDword == 4008u);
static_assert(offsetof(ResourceMetadata, descriptor) == 16u && offsetof(ShaderData, images) == 672u && offsetof(ShaderData, samplers) == 12960u);

inline void RequireVersion(std::uint32_t version) {
    if (version != Version) throw std::runtime_error("Shader runtime ABI: incompatible version");
}

inline constexpr std::uint32_t ImageStorageTableBinding = StageCount * static_cast<std::uint32_t>(Binding::Count);
inline constexpr std::uint32_t ImageTableBinding = ImageStorageTableBinding + 1u;

inline std::uint32_t BindingNumber(Stage stage, Binding binding) {
    if (binding == Binding::ImageTable) return ImageTableBinding;
    if (binding == Binding::ImageStorageTable) return ImageStorageTableBinding;
    const auto group = static_cast<std::uint32_t>(stage);
    const auto index = static_cast<std::uint32_t>(binding);
    const auto count = static_cast<std::uint32_t>(Binding::Count);
    if (group >= StageCount || index >= count) throw std::runtime_error("Shader runtime ABI: invalid stage or binding");
    return group * count + index;
}

static_assert(FirstImageBinding + ImageBindingCount == static_cast<std::uint32_t>(Binding::Samplers));
static_assert(static_cast<std::uint32_t>(Binding::ImageTable) > static_cast<std::uint32_t>(Binding::Count) && ImageStorageTableBinding == 260u && ImageTableBinding == 261u);

}

#endif
