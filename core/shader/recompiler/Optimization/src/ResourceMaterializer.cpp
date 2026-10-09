#include "Optimization/ResourceMaterializer.hpp"
#include "Optimization/SrtWalker/SrtFlatSlotClasses.hpp"
#include "Optimization/ShaderStageInputInfo.hpp"
#include "RdnaDecoder/RdnaDescriptorFormat.hpp"
#include "RdnaDecoder/RdnaImageOpDecoder.hpp"
#include "SpirvBackend/SpirvBufferFormat.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <map>
#include <mutex>
#include <optional>
#include <span>
#include <unordered_map>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace ShaderRecompiler {

namespace {

std::atomic<std::uint64_t> specializationNanoseconds{0};

bool MaterializeProfiled() {
    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    return profile;
}

struct DecodedImage {
    IrTextureNumericClass numericClass = IrTextureNumericClass::Unsupported;
    RdnaImageDimension dimension = RdnaImageDimension::Unknown;
    std::uint32_t mipCount = 1;
    IrBufferFormat conversionFormat = IrBufferFormat::Invalid;
    std::uint32_t shaderSwizzle = ShaderImageIdentitySwizzle;
    bool cube = false;
    bool fmask = false;
    bool depthBits = false;
    bool depthUnorm16 = false;
    IrBufferFormat packedFormat = IrBufferFormat::Invalid;
    bool srgbDecode = false;
};

ShaderBufferResource decodeBufferDescriptor(const DescriptorValue& value) {
    if (value.dwordCount != 4u) {
        throw std::runtime_error("buffer descriptor has an invalid width");
    }
    ShaderBufferResource result;
    for (std::uint32_t i = 0; i < 4u; i++) {
        result.fields[i] = value.dwords[i];
    }
    return result;
}

bool nullImageDescriptor(const DescriptorValue& descriptor) {
    return descriptor.dwords[0] == 0u && (descriptor.dwords[1] & 0xffu) == 0u;
}

ImageType rawImageType(const DescriptorValue& descriptor) {
    return static_cast<ImageType>((descriptor.dwords[3] >> 28u) & 0xfu);
}

IrBufferFormat rawImageFormat(const DescriptorValue& descriptor) {
    return static_cast<IrBufferFormat>((descriptor.dwords[1] >> 20u) & 0x1ffu);
}

std::uint32_t descriptorImageSwizzle(const DescriptorValue& descriptor) {
    return descriptor.dwords[3] & 0xfffu;
}

bool descriptorIsCube(const DescriptorValue& descriptor) {
    return rawImageType(descriptor) == ImageType::Cube;
}

RdnaImageDimension descriptorDimension(const DescriptorValue& descriptor, RdnaImageDimension requested) {
    const bool wantArray = requested == RdnaImageDimension::Dim1DArray || requested == RdnaImageDimension::Dim2DArray || requested == RdnaImageDimension::Dim2DMsaaArray;
    switch (rawImageType(descriptor)) {
        case ImageType::Color1D:
            return RdnaImageDimension::Dim1D;
        case ImageType::Color1DArray:
            return wantArray ? RdnaImageDimension::Dim1DArray : RdnaImageDimension::Dim1D;
        case ImageType::Color3D:
            return RdnaImageDimension::Dim3D;
        case ImageType::Cube:
            return RdnaImageDimension::Dim2DArray;
        case ImageType::Color2DArray:
            return wantArray ? RdnaImageDimension::Dim2DArray : RdnaImageDimension::Dim2D;
        case ImageType::Color2DMsaaArray:
            return wantArray ? RdnaImageDimension::Dim2DMsaaArray : RdnaImageDimension::Dim2DMsaa;
        case ImageType::Color2D:
            return RdnaImageDimension::Dim2D;
        case ImageType::Color2DMsaa:
            return RdnaImageDimension::Dim2DMsaa;
        default:
            throw std::runtime_error("image descriptor has an unsupported type");
    }
}

bool validImageDescriptor(const DescriptorValue& descriptor, bool r128) {
    const auto type = rawImageType(descriptor);
    const auto format = rawImageFormat(descriptor);
    if (type < ImageType::Color1D || format == IrBufferFormat::Invalid) {
        return false;
    }
    if (r128 && type != ImageType::Color1D && type != ImageType::Color2D && type != ImageType::Color2DMsaa) {
        return false;
    }
    if (type == ImageType::Color2DMsaa || type == ImageType::Color2DMsaaArray) {
        const auto baseLevel = (descriptor.dwords[3] >> 12u) & 0xfu;
        const auto fragments = (descriptor.dwords[3] >> 16u) & 0xfu;
        const auto maxMip = (descriptor.dwords[5] >> 4u) & 0xfu;
        return baseLevel == 0u && fragments >= 1u && fragments <= 3u && (r128 || maxMip == fragments);
    }
    return true;
}

std::uint32_t storageMipCount(const ImageResource& base, const DescriptorValue& descriptor) {
    if (base.mipMode != ImageMipMode::DynamicStorage || nullImageDescriptor(descriptor)) {
        return 1u;
    }
    const auto mipBase = (descriptor.dwords[3] >> 12u) & 0xfu;
    const auto mipLast = (descriptor.dwords[3] >> 16u) & 0xfu;
    return mipBase <= mipLast ? mipLast - mipBase + 1u : 0u;
}

DecodedImage decodeImageDescriptor(const DescriptorValue& descriptor, const ImageResource& base, std::uint32_t srgbDecodeFormats) {
    DecodedImage decoded;
    decoded.mipCount = storageMipCount(base, descriptor);
    if (decoded.mipCount == 0u) {
        throw std::runtime_error("storage image descriptor has an invalid mip range");
    }
    if (nullImageDescriptor(descriptor)) {
        decoded.numericClass = base.atomic ? IrTextureNumericClass::Uint : IrTextureNumericClass::Float;
        decoded.dimension = RdnaImageDimension::Dim2D;
        decoded.cube = false;
        return decoded;
    }
    if (base.resourceClass == ImageResourceClass::None || (base.atomic && base.resourceClass != ImageResourceClass::Storage)) {
        throw std::runtime_error("image resource has an invalid class");
    }
    if (!validImageDescriptor(descriptor, base.r128)) {
        throw std::runtime_error("image descriptor is invalid");
    }
    decoded.dimension = descriptorDimension(descriptor, base.dimension);
    decoded.cube = descriptorIsCube(descriptor);
    const auto format = rawImageFormat(descriptor);
    if (base.atomic64 && format != IrBufferFormat::Format32_32UInt && format != IrBufferFormat::Format32_32SInt && format != IrBufferFormat::Format32_32Float) {
        throw std::runtime_error("64-bit atomic image descriptor uses an unsupported format " + std::to_string(static_cast<std::uint32_t>(format)));
    }
    if (base.atomic && !base.atomic64 && format != IrBufferFormat::Format32UInt && format != IrBufferFormat::Format32SInt && format != IrBufferFormat::Format32Float) {
        throw std::runtime_error("atomic image descriptor uses an unsupported format " + std::to_string(static_cast<std::uint32_t>(format)));
    }
    const bool storage = base.resourceClass == ImageResourceClass::Storage;
    decoded.fmask = IsFmaskTextureFormat(format);
    if (decoded.fmask && !base.fmaskCompatible) throw std::runtime_error("FMASK requires a direct 32-bit image load");
    if (decoded.fmask && (storage || base.depthCompare || base.indirectRoot != ImageResource::NoIndirectImage)) {
        throw std::runtime_error("FMASK requires a direct sampled image load");
    }
    if (base.packed) {
        if (base.indirectRoot != ImageResource::NoIndirectImage || (!storage && descriptorImageSwizzle(descriptor) != ShaderImageIdentitySwizzle)) {
            throw std::runtime_error("packed image access requires a direct image, with identity swizzle when sampled");
        }
        decoded.packedFormat = format;
    }
    if (base.byElements != 0u) {
        const bool eightBit = format == IrBufferFormat::Format8UNorm || format == IrBufferFormat::Format8SNorm || format == IrBufferFormat::Format8UInt || format == IrBufferFormat::Format8SInt;
        const bool sixteenBit = format == IrBufferFormat::Format16UNorm || format == IrBufferFormat::Format16SNorm || format == IrBufferFormat::Format16UInt || format == IrBufferFormat::Format16SInt || format == IrBufferFormat::Format16Float;
        const bool eightBitPair = format == IrBufferFormat::Format8_8UNorm || format == IrBufferFormat::Format8_8SNorm || format == IrBufferFormat::Format8_8UInt || format == IrBufferFormat::Format8_8SInt;
        const bool packedEight = format == IrBufferFormat::Format8UNorm || format == IrBufferFormat::Format8UInt || format == IrBufferFormat::Format8SInt;
        const bool packedSixteen = format == IrBufferFormat::Format16UNorm || format == IrBufferFormat::Format16UInt || format == IrBufferFormat::Format16SInt || format == IrBufferFormat::Format8_8UNorm || format == IrBufferFormat::Format8_8UInt || format == IrBufferFormat::Format8_8SInt;
        const bool measured = base.packed ? packedEight || (base.byElements == 2u && packedSixteen)
                                          : base.byElements == 4u ? base.byComponents == 1u && eightBit : base.byElements == 2u && (base.byComponents == 1u ? eightBit || sixteenBit : base.byComponents == 2u && eightBitPair);
        if (!measured || descriptorImageSwizzle(descriptor) != ShaderImageIdentitySwizzle || rawImageType(descriptor) != ImageType::Color2D || base.indirectRoot != ImageResource::NoIndirectImage) {
            throw std::runtime_error(base.packed ? "MIMG PCK2/PCK4 requires a direct, identity-swizzled 2D UNORM, UINT or SINT image whose elements fill one dword: R8, R16 or RG8 for PCK2, R8 for PCK4"
                                                 : "MIMG BY2/BY4 requires a direct, identity-swizzled 2D image whose elements fill one dword: R8, R16 or RG8 for BY2, R8 for BY4");
        }
    }
    decoded.conversionFormat = RemapTextureFormat(format) != format ? format : IrBufferFormat::Invalid;
    if (format == IrBufferFormat::Format11_11_10UNorm || format == IrBufferFormat::Format10_11_11Float) {
        const bool floating = format == IrBufferFormat::Format10_11_11Float;
        if (!base.srgbDecodeCompatible) throw std::runtime_error(floating ? "samples or gathers a converted float image, or queries its LOD, which is not implemented" : "sampling, gathering or querying LOD of a converted unorm image is not implemented");
        if (!base.depthBitsCompatible) throw std::runtime_error(floating ? "reads or writes a converted float image with 16-bit data, which is not implemented" : "reads or writes a converted unorm image with 16-bit data, which is not implemented");
        for (std::uint32_t component = 0; component < 4u; ++component) {
            if (((descriptorImageSwizzle(descriptor) >> (component * 3u)) & 7u) == 7u) throw std::runtime_error("selects a channel the converted image format does not have");
        }
    }
    decoded.srgbDecode = !storage && (srgbDecodeFormats & SrgbDecodeBit(format)) != 0u;
    if (decoded.srgbDecode && !base.srgbDecodeCompatible) {
        throw std::runtime_error("samples or gathers an sRGB image the device cannot sample, which is not implemented");
    }
    if (storage || decoded.conversionFormat != IrBufferFormat::Invalid) {
        decoded.shaderSwizzle = descriptorImageSwizzle(descriptor);
    }
    const bool wideSint = format == IrBufferFormat::Format32SInt || format == IrBufferFormat::Format32_32SInt || format == IrBufferFormat::Format32_32_32_32SInt;
    const bool narrowSint = format == IrBufferFormat::Format16SInt || format == IrBufferFormat::Format8_8SInt || format == IrBufferFormat::Format16_16SInt || format == IrBufferFormat::Format8_8_8_8SInt || format == IrBufferFormat::Format16_16_16_16SInt;
    const bool rawSintStorage = storage && (wideSint || (narrowSint && !base.packed)) && base.written && !base.read && !base.atomic;
    decoded.numericClass = base.atomic ? IrTextureNumericClass::Uint : SampledTextureNumericClass(format);
    if (!storage && !base.depthCompare && IsDepthBitsTexture(descriptor.dwords[1], descriptor.dwords[3])) {
        decoded.depthBits = true;
        if (!base.depthBitsCompatible) throw std::runtime_error("runtime image reads depth bits as unsupported 16-bit results");
        decoded.depthUnorm16 = DepthBitsTextureWidth(descriptor.dwords[1], descriptor.dwords[3]) == 16u;
        decoded.numericClass = IrTextureNumericClass::Float;
        decoded.shaderSwizzle = descriptorImageSwizzle(descriptor);
    }
    if (storage) {
        if ((!rawSintStorage && decoded.numericClass == IrTextureNumericClass::Sint) || decoded.numericClass == IrTextureNumericClass::Unsupported) {
            throw std::runtime_error("storage image descriptor uses an unsupported format");
        }
        if (rawSintStorage) {
            decoded.numericClass = IrTextureNumericClass::Uint;
        }
        if ((rawSintStorage || (base.atomic && format == IrBufferFormat::Format32SInt)) && !base.packed) {
            if (!base.depthBitsCompatible) throw std::runtime_error("stores 16-bit data to an image of a SINT format");
            decoded.conversionFormat = format;
        }
    } else if (decoded.numericClass == IrTextureNumericClass::Unsupported || (base.depthCompare && decoded.numericClass != IrTextureNumericClass::Float)) {
        throw std::runtime_error("sampled image descriptor uses an unsupported format");
    }
    return decoded;
}

constexpr std::uint32_t TableEntryBytes = 32;

std::uint32_t MaterialScanLimit() {
    static const std::uint32_t limit = [] {
        const char* text = std::getenv("APS5_BINDLESS_MATERIAL_SCAN");
        return text != nullptr ? static_cast<std::uint32_t>(std::strtoul(text, nullptr, 0)) : 256u;
    }();
    return limit;
}

bool BindlessTraced() {
    static const bool traced = std::getenv("APS5_TRACE_BINDLESS") != nullptr;
    return traced;
}

struct BindlessCounters {
    std::atomic<std::uint64_t> tablesMaterial{0};
    std::atomic<std::uint64_t> tablesWhole{0};
    std::atomic<std::uint64_t> keys{0};
    std::atomic<std::uint64_t> paddedNull{0};
    std::atomic<std::uint64_t> paddedShape{0};
    std::atomic<std::uint64_t> paddedConversion{0};
    std::atomic<std::uint64_t> outOfRange{0};
    std::array<std::atomic<std::uint64_t>, static_cast<std::size_t>(BindlessRejection::Count)> rejected{};
    std::atomic<long long> lastReport{0};
};

BindlessCounters& bindlessCounters() {
    static BindlessCounters counters;
    return counters;
}

[[noreturn]] void rejectTable(BindlessRejection reason, const std::string& message) {
    ResourceMaterializer::CountBindlessRejection(reason);
    throw std::runtime_error(message);
}

void reportBindless() {
    if (!MaterializeProfiled()) return;
    auto& counters = bindlessCounters();
    const auto now = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    auto last = counters.lastReport.load(std::memory_order_relaxed);
    if (last == 0) {
        counters.lastReport.compare_exchange_strong(last, now, std::memory_order_relaxed);
        return;
    }
    if (now - last < 10'000'000'000ll || !counters.lastReport.compare_exchange_strong(last, now, std::memory_order_relaxed)) return;
    const auto material = counters.tablesMaterial.exchange(0, std::memory_order_relaxed);
    const auto whole = counters.tablesWhole.exchange(0, std::memory_order_relaxed);
    const auto keys = counters.keys.exchange(0, std::memory_order_relaxed);
    std::array<std::uint64_t, static_cast<std::size_t>(BindlessRejection::Count)> rejected{};
    std::uint64_t rejections = 0;
    for (std::size_t i = 0; i < rejected.size(); i++) {
        rejected[i] = counters.rejected[i].exchange(0, std::memory_order_relaxed);
        rejections += rejected[i];
    }
    if (material + whole + rejections == 0) return;
    const auto tables = material + whole;
    std::fprintf(stderr, "[bindless] (10 s): tables bound %llu (mode M %llu, mode T %llu), slots %u, keys avg %.1f, entries unmapped (sample zeros): null/invalid %llu, shape %llu, conversion %llu, out of range %llu; rejected: capacity %llu, material scan %llu, no entry %llu, storage %llu, non-uniform %llu, image slots %llu\n",
        static_cast<unsigned long long>(tables), static_cast<unsigned long long>(material), static_cast<unsigned long long>(whole), ResourceMaterializer::BindlessSlots(), tables != 0 ? static_cast<double>(keys) / static_cast<double>(tables) : 0.0,
        static_cast<unsigned long long>(counters.paddedNull.exchange(0, std::memory_order_relaxed)), static_cast<unsigned long long>(counters.paddedShape.exchange(0, std::memory_order_relaxed)), static_cast<unsigned long long>(counters.paddedConversion.exchange(0, std::memory_order_relaxed)), static_cast<unsigned long long>(counters.outOfRange.exchange(0, std::memory_order_relaxed)),
        static_cast<unsigned long long>(rejected[0]), static_cast<unsigned long long>(rejected[1]), static_cast<unsigned long long>(rejected[2]), static_cast<unsigned long long>(rejected[3]), static_cast<unsigned long long>(rejected[4]), static_cast<unsigned long long>(rejected[5]));
}

struct TableResolution {
    std::vector<DescriptorValue> slots;
    std::vector<std::pair<std::uint32_t, std::uint32_t>> mapping;
};

struct TableTrace {
    bool material = false;
    std::uint32_t entries = 0;
    std::uint32_t materialEntries = 0;
    std::uint32_t keys = 0;

    bool operator==(const TableTrace& other) const = default;
};

void traceTable(const IrResourcePlan& plan, const ImageResource& image, const DescriptorSource::IndirectImage& table, std::uint64_t heapBase, std::uint64_t materialBase, const TableTrace& trace) {
    static std::mutex mutex;
    static std::map<std::pair<std::uint64_t, std::uint32_t>, TableTrace> seen;
    std::lock_guard lock(mutex);
    auto& last = seen[{plan.shaderHash, image.firstUsePc}];
    if (last == trace) return;
    last = trace;
    std::fprintf(stderr, "[bindless] table at pc 0x%x of shader %llx: heap V# base 0x%llx entries %u, material V# base 0x%llx entries %u stride 0x%x offset 0x%x, mode %c, %u keys\n", image.firstUsePc, static_cast<unsigned long long>(plan.shaderHash), static_cast<unsigned long long>(heapBase), trace.entries, static_cast<unsigned long long>(materialBase), trace.materialEntries, table.selectorStride, table.selectorOffset, trace.material ? 'M' : 'T', trace.keys);
}

// The slots of the table image `imageIndex` (see TableResolution). Mode M enumerates the keys the
// dispatch can reach from the material records; mode T binds the whole table when it fits.
void resolveTableImage(const IrResourcePlan& plan, std::uint32_t imageIndex, const DescriptorSource::IndirectImage& table, const SrtRuntime& runtime, SrtWalker& walker, DescriptorValue& resolved, TableResolution& resolution) {
    const auto& image = plan.info.images.at(imageIndex);
    if (runtime.readMemory == nullptr) {
        throw std::runtime_error("bindless image table resolution requires runtime memory access");
    }
    if (image.resourceClass != ImageResourceClass::Sampled) {
        rejectTable(BindlessRejection::Storage, "bindless storage image tables are unsupported");
    }
    if (image.packed || image.byElements != 0u) {
        throw std::runtime_error("bindless packed and BY2/BY4 image tables are unsupported");
    }
    const auto slots = ResourceMaterializer::BindlessSlots();
    DescriptorValue heapValue;
    walker.EvaluateDescriptorSource(plan, table.heapSource, runtime, heapValue);
    const ShaderBufferResource heap = decodeBufferDescriptor(heapValue);
    const std::uint64_t heapSize = heap.GetSize();
    if (heap.Type() != 0u || heap.Base48() > std::numeric_limits<std::uint64_t>::max() - heapSize) throw std::runtime_error("bindless heap buffer is invalid");
    if (heapSize != 0u && (heapSize < table.entryOffset || (heapSize - table.entryOffset) % TableEntryBytes != 0u)) throw std::runtime_error("bindless heap contains a partial descriptor");
    const auto entries = heapSize > table.entryOffset ? static_cast<std::uint32_t>(std::min<std::uint64_t>((heapSize - table.entryOffset) / TableEntryBytes, std::numeric_limits<std::uint32_t>::max())) : 0u;
    const auto readWord = [&](std::uint64_t address, std::uint32_t& word) {
        if (!runtime.readMemory(runtime.userContext, address, &word)) {
            throw std::runtime_error("failed to read a bindless image table from memory");
        }
    };

    auto& counters = bindlessCounters();
    std::vector<std::uint32_t> keys;
    bool materialMode = false;
    std::uint32_t materialEntries = 0;
    std::uint64_t materialBase = 0;
    std::uint32_t outOfRange = 0;
    if (table.hasMaterial) {
        if (table.selectorStride < sizeof(std::uint32_t) || table.selectorOffset > table.selectorStride - sizeof(std::uint32_t)) throw std::runtime_error("bindless material selector is outside its record");
        DescriptorValue materialValue;
        walker.EvaluateDescriptorSource(plan, table.materialSource, runtime, materialValue);
        const ShaderBufferResource material = decodeBufferDescriptor(materialValue);
        materialBase = material.Base48();
        const std::uint64_t materialSize = material.GetSize();
        if (material.Type() != 0u || materialSize % table.selectorStride != 0u) throw std::runtime_error("bindless material buffer has an invalid layout");
        materialEntries = static_cast<std::uint32_t>(std::min<std::uint64_t>(materialSize / table.selectorStride, std::numeric_limits<std::uint32_t>::max()));
        if (materialSize / table.selectorStride > MaterialScanLimit()) rejectTable(BindlessRejection::MaterialScan, "bindless material scan capacity exceeded");
        if (materialEntries <= MaterialScanLimit()) {
            materialMode = true;
            for (std::uint32_t record = 0; record < materialEntries; record++) {
                const std::uint64_t offset = static_cast<std::uint64_t>(record) * table.selectorStride + table.selectorOffset;
                if (offset + sizeof(std::uint32_t) > materialSize) throw std::runtime_error("bindless material selector exceeds its buffer");
                std::uint32_t key = 0;
                readWord(materialBase + offset, key);
                if (key >= entries) {
                    outOfRange++;
                    continue;
                }
                keys.push_back(key);
            }
            std::sort(keys.begin(), keys.end());
            keys.erase(std::unique(keys.begin(), keys.end()), keys.end());
            if (keys.size() > slots) {
                rejectTable(BindlessRejection::Capacity, "bindless image table: " + std::to_string(keys.size()) + " distinct keys exceed the " + std::to_string(slots) + " slots");
            }
        }
    }
    if (!materialMode) {
        if (entries > slots) {
            rejectTable(table.hasMaterial ? BindlessRejection::MaterialScan : BindlessRejection::Capacity, "bindless image table has " + std::to_string(entries) + " entries (" + std::to_string(materialEntries) + " materials), limit " + std::to_string(slots));
        }
        keys.resize(entries);
        for (std::uint32_t key = 0; key < entries; key++) keys[key] = key;
    }
    counters.outOfRange.fetch_add(outOfRange, std::memory_order_relaxed);
    const std::uint64_t heapBase = heap.Base48();
    std::vector<DescriptorValue> candidates(keys.size());
    std::vector<std::uint8_t> valid(keys.size(), 0u);
    std::uint32_t paddedNull = 0u;
    for (std::size_t i = 0u; i < keys.size(); ++i) {
        auto& candidate = candidates[i];
        candidate.dwordCount = 8u;
        const std::uint64_t address = heapBase + table.entryOffset + static_cast<std::uint64_t>(keys[i]) * TableEntryBytes;
        for (std::uint32_t dword = 0u; dword < 8u; ++dword) readWord(address + dword * sizeof(std::uint32_t), candidate.dwords[dword]);
        if (nullImageDescriptor(candidate)) {
            ++paddedNull;
            continue;
        }
        if (!validImageDescriptor(candidate, image.r128)) throw std::runtime_error("bindless image table contains an invalid descriptor");
        const auto decoded = decodeImageDescriptor(candidate, image, plan.srgbDecodeFormats);
        if (decoded.fmask) throw std::runtime_error("bindless FMASK images are unsupported");
        static_cast<void>(ResourceMaterializer::RuntimeImageMode(image, candidate, plan.info.runtimeImageModes.at(imageIndex)));
        valid[i] = 1u;
    }
    resolution.mapping.clear();
    for (std::size_t i = 0u; i < keys.size(); ++i) {
        if (valid[i] != 0u) resolution.mapping.emplace_back(keys[i], static_cast<std::uint32_t>(i));
    }
    DescriptorValue nullDescriptor;
    nullDescriptor.dwordCount = 8u;
    resolution.slots = std::move(candidates);
    resolution.slots.resize(slots, nullDescriptor);
    resolved = resolution.slots[0];

    (materialMode ? counters.tablesMaterial : counters.tablesWhole).fetch_add(1, std::memory_order_relaxed);
    counters.keys.fetch_add(resolution.mapping.size(), std::memory_order_relaxed);
    counters.paddedNull.fetch_add(paddedNull, std::memory_order_relaxed);
    if (BindlessTraced()) traceTable(plan, image, table, heapBase, materialBase, {materialMode, entries, materialEntries, static_cast<std::uint32_t>(resolution.mapping.size())});
}

void materializeSnapshot(const IrResourcePlan& plan, const SrtRuntime& runtime, SrtWalker& walker, ResourceSnapshot& snapshot, std::vector<TableResolution>& tables) {
    snapshot = ResourceSnapshot{};
    if (plan.uniformFill.fill.kind != UniformFillKind::None) {
        const auto words = plan.uniformFill.fill.words;
        if (words == 0u || words > plan.uniformFill.values.size()) {
            throw std::runtime_error("uniform fill plan has an invalid word count");
        }
        std::array<std::uint32_t, 4> stored{};
        walker.EvaluateUniformValues(plan, std::span(plan.uniformFill.values).first(words), runtime, std::span(stored).first(words));
        for (std::uint32_t i = 1; i < words; i++) {
            if (stored[i] != stored[0]) {
                throw std::runtime_error("uniform fill values diverge at runtime");
            }
        }
        snapshot.uniformFill = plan.uniformFill.fill;
        snapshot.uniformFill.value = stored[0];
    }
    if (runtime.userData.size() < plan.userDataCount) {
        throw std::runtime_error("runtime user data is smaller than the shader user data count");
    }
    snapshot.userData.assign(runtime.userData.begin(), runtime.userData.begin() + plan.userDataCount);

    std::vector<DescriptorValue> values;
    std::vector<std::uint8_t> activeSources;
    walker.EvaluateRuntimeSources(plan, plan.materializationSources, runtime, values, snapshot.flattenedSrt, plan.cleanFlatSlots, activeSources, &snapshot.srtPoison);

    std::size_t cursor = 0;
    if (values.size() < plan.info.buffers.size()) {
        throw std::runtime_error("materialization sources are missing buffer descriptors");
    }
    snapshot.buffers.assign(values.begin(), values.begin() + plan.info.buffers.size());
    cursor += plan.info.buffers.size();

    snapshot.images.resize(plan.info.images.size());
    tables.assign(plan.info.images.size(), {});
    std::uint32_t tableCount = 0;
    for (std::uint32_t i = 0; i < plan.info.images.size(); i++) {
        const auto& image = plan.info.images[i];
        if (image.source >= plan.descriptorSources.size()) {
            throw std::runtime_error("image resource references an unknown descriptor source");
        }
        if (plan.descriptorSources[image.source].indirectImage.has_value()) tableCount++;
    }
    const auto tableSlots = ResourceMaterializer::BindlessSlots();
    if (tableCount != 0u && plan.info.images.size() + static_cast<std::size_t>(tableSlots - 1u) * tableCount > ShaderInfo::MaxImages) {
        rejectTable(BindlessRejection::ImageSlots, "bindless image tables need " + std::to_string(plan.info.images.size() + static_cast<std::size_t>(tableSlots - 1u) * tableCount) + " image slots, limit " + std::to_string(ShaderInfo::MaxImages));
    }
    for (std::uint32_t i = 0; i < plan.info.images.size(); i++) {
        const auto& image = plan.info.images[i];
        const auto& source = plan.descriptorSources[image.source];
        if (source.indirectImage.has_value()) {
            if (image.source < activeSources.size() && activeSources[image.source] == 0u) {
                snapshot.images[i].dwordCount = 8u;
                tables[i].slots.assign(tableSlots, snapshot.images[i]);
                continue;
            }
            resolveTableImage(plan, i, *source.indirectImage, runtime, walker, snapshot.images[i], tables[i]);
            continue;
        }
        if (cursor >= values.size()) {
            throw std::runtime_error("materialization sources are missing image descriptors");
        }
        auto descriptor = values[cursor];
        cursor++;
        if (descriptor.dwordCount != 8u) {
            throw std::runtime_error("image descriptor has an invalid width");
        }
        if (!validImageDescriptor(descriptor, image.r128) && !nullImageDescriptor(descriptor)) {
            throw std::runtime_error("runtime image descriptor is invalid");
        }
        snapshot.images[i] = descriptor;
    }

    if (values.size() < cursor + plan.info.samplers.size()) {
        throw std::runtime_error("materialization sources are missing sampler descriptors");
    }
    snapshot.samplers.assign(values.begin() + cursor, values.begin() + cursor + plan.info.samplers.size());
}

std::uint32_t colorCompareReference(IrBufferFormat format) {
    switch (format) {
    case IrBufferFormat::Format8UNorm: case IrBufferFormat::Format8_8UNorm: case IrBufferFormat::Format16_16UNorm:
    case IrBufferFormat::Format11_11_10UNorm: case IrBufferFormat::Format10_11_11UNorm: case IrBufferFormat::Format2_10_10_10UNorm:
    case IrBufferFormat::Format10_10_10_2UNorm: case IrBufferFormat::Format8_8_8_8UNorm: case IrBufferFormat::Format16_16_16_16UNorm:
        return EmulatedCompare::ReferenceUnorm;
    case IrBufferFormat::Format8SNorm: case IrBufferFormat::Format16SNorm: case IrBufferFormat::Format8_8SNorm: case IrBufferFormat::Format16_16SNorm:
    case IrBufferFormat::Format11_11_10SNorm: case IrBufferFormat::Format10_11_11SNorm: case IrBufferFormat::Format2_10_10_10SNorm:
    case IrBufferFormat::Format10_10_10_2SNorm: case IrBufferFormat::Format8_8_8_8SNorm: case IrBufferFormat::Format16_16_16_16SNorm:
        return EmulatedCompare::ReferenceSnorm;
    case IrBufferFormat::Format16Float: case IrBufferFormat::Format16_16Float: case IrBufferFormat::Format11_11_10Float:
    case IrBufferFormat::Format10_11_11Float: case IrBufferFormat::Format32_32Float: case IrBufferFormat::Format16_16_16_16Float:
        return EmulatedCompare::ReferenceFloat;
    default:
        throw std::runtime_error("comparison sampling of a color texture is implemented only for float, unorm and snorm formats (format " + std::to_string(static_cast<std::uint32_t>(format)) + ")");
    }
}

std::uint32_t emulatedCompareState(const ShaderInfo& info, const ResourceSnapshot& snapshot, std::uint32_t index) {
    const auto& image = info.images[index];
    const auto& descriptor = snapshot.images[index];
    if (!image.depthCompare || descriptor.dwordCount != 8u || nullImageDescriptor(descriptor)) return 0u;
    const auto format = rawImageFormat(descriptor);
    if (format == IrBufferFormat::Format32Float || format == IrBufferFormat::Format16UNorm || IsDepthBitsTexture(descriptor.dwords[1], descriptor.dwords[3])) return 0u;
    if (image.indirectRoot != ImageResource::NoIndirectImage || (image.emulatedCompare & EmulatedCompare::Unsupported) != 0u) throw std::runtime_error("unsupported color comparison image instructions");
    if ((image.emulatedCompare & EmulatedCompare::RequiresSingleLevel) != 0u && ((descriptor.dwords[3] >> 12u) & 0xfu) != ((descriptor.dwords[3] >> 16u) & 0xfu)) throw std::runtime_error("color comparison requires a single mip level");
    const auto reference = colorCompareReference(format);
    const auto type = rawImageType(descriptor);
    if (type != ImageType::Color2D && type != ImageType::Color2DArray && type != ImageType::Cube) throw std::runtime_error("comparison sampling of a color texture is implemented only for 2D, 2D array and cube views");
    if ((descriptorImageSwizzle(descriptor) & 0x7u) != 4u) throw std::runtime_error("comparison sampling of a color texture is implemented only when the view's X channel is red");
    std::optional<std::uint32_t> samplerState;
    for (const auto& pair : info.sampledPairs) {
        if (pair.image != index) continue;
        if (pair.sampler >= snapshot.samplers.size() || snapshot.samplers[pair.sampler].dwordCount != 4u) throw std::runtime_error("comparison sampling of a color texture has no sampler descriptor");
        const auto& words = snapshot.samplers[pair.sampler].dwords;
        const auto clampX = words[0] & 0x7u;
        const auto clampY = (words[0] >> 3u) & 0x7u;
        const auto function = (words[0] >> 12u) & 0x7u;
        const bool unnormalized = ((words[0] >> 15u) & 0x1u) != 0u;
        if (((words[0] >> 29u) & 0x3u) != 0u) throw std::runtime_error("comparison sampling of a color texture through a min or max reduction sampler is not implemented");
        const auto magFilter = (words[2] >> 20u) & 0x3u;
        const auto minFilter = (words[2] >> 22u) & 0x3u;
        if (type == ImageType::Cube && magFilter == 1u && ((words[0] >> 28u) & 1u) == 0u) {
            throw std::runtime_error("bilinear cube comparison requires DISABLE_CUBE_WRAP to avoid seamless face filtering");
        }
        const auto addressMode = [](std::uint32_t clamp) {
            if (clamp == 0u) return EmulatedCompare::AddressWrap;
            if (clamp == 2u) return EmulatedCompare::AddressEdge;
            if (clamp == 6u) return EmulatedCompare::AddressBorder;
            throw std::runtime_error("comparison sampling of a color texture is implemented only with wrap, clamp-to-edge or clamp-to-border addressing");
        };
        const auto addressX = addressMode(clampX);
        const auto addressY = addressMode(clampY);
        const auto borderType = (words[3] >> 30u) & 0x3u;
        const bool border = addressX == EmulatedCompare::AddressBorder || addressY == EmulatedCompare::AddressBorder;
        if (border && borderType == 3u) throw std::runtime_error("comparison sampling of a color texture with a border color table is not implemented");
        if (magFilter != minFilter || magFilter > 1u) throw std::runtime_error("comparison sampling of a color texture is implemented only with equal point or bilinear minification and magnification filters");
        if (unnormalized) throw std::runtime_error("comparison sampling of a color texture does not implement unnormalized coordinates");
        const auto state = EmulatedCompare::Enabled | (function << EmulatedCompare::FunctionShift) | (magFilter == 1u ? EmulatedCompare::Linear : 0u)
            | (addressX << EmulatedCompare::ClampXShift) | (addressY << EmulatedCompare::ClampYShift) | (border && borderType == 2u ? EmulatedCompare::BorderWhite : 0u);
        if (samplerState.has_value() && *samplerState != state) throw std::runtime_error("comparison sampling of a color texture through samplers that disagree is not implemented");
        samplerState = state;
    }
    if (!samplerState.has_value()) throw std::runtime_error("comparison sampling of a color texture has no paired sampler");
    const bool singleLevel = ((descriptor.dwords[3] >> 12u) & 0xfu) == ((descriptor.dwords[3] >> 16u) & 0xfu);
    return *samplerState | (reference << EmulatedCompare::ReferenceShift) | (singleLevel ? EmulatedCompare::SingleLevel : 0u);
}

void materializeTables(const IrResourcePlan& plan, ResourceSnapshot& snapshot, const std::vector<TableResolution>& tables) {
    for (std::uint32_t i = 0u; i < plan.info.buffers.size(); ++i) {
        const auto decoded = decodeBufferDescriptor(snapshot.buffers.at(i));
        if (decoded.Type() != 0u) throw std::runtime_error("buffer descriptor uses an unsupported type");
        if (plan.stage != IrShaderStage::Compute && decoded.AddTid()) throw std::runtime_error("buffer ADD_TID is only valid for compute shaders");
    }
    if (snapshot.flattenedSrt.size() != plan.srtReads.size()) throw std::runtime_error("runtime SRT size differs from the static interface");
    for (std::uint32_t i = 0u; i < plan.info.images.size(); ++i) {
        const auto& image = plan.info.images[i];
        static_cast<void>(ResourceMaterializer::RuntimeImageMode(image, snapshot.images.at(i), plan.info.runtimeImageModes.at(i)));
        const auto& table = tables.at(i);
        if (!plan.descriptorSources.at(image.source).indirectImage.has_value()) {
            if (!table.slots.empty()) throw std::runtime_error("direct image has runtime table slots");
            continue;
        }
        const auto slots = ResourceMaterializer::BindlessSlots();
        if (table.slots.size() != slots) throw std::runtime_error("runtime table size differs from the static interface");
        const auto mappingOffset = snapshot.flattenedSrt.size();
        snapshot.flattenedSrt.push_back(static_cast<std::uint32_t>(table.mapping.size()));
        for (const auto& [key, slot] : table.mapping) {
            if (slot >= slots) throw std::runtime_error("runtime table slot exceeds its static capacity");
            snapshot.flattenedSrt.push_back(key);
            snapshot.flattenedSrt.push_back(slot == 0u ? i : static_cast<std::uint32_t>(snapshot.images.size()) + slot - 1u);
        }
        snapshot.flattenedSrt.resize(mappingOffset + 1u + 2u * slots, 0u);
        snapshot.images.insert(snapshot.images.end(), table.slots.begin() + 1u, table.slots.end());
    }
}

void materializeSrtGuards(const IrResourcePlan& plan, ResourceSnapshot& snapshot) {
    const auto& guarded = plan.guardedSrtSlots;
    const auto flags = snapshot.flattenedSrt.size();
    snapshot.flattenedSrt.resize(flags + guarded.size(), 0u);
    for (std::uint32_t record = 0u; record < snapshot.srtPoison.size(); ++record) {
        const auto& poison = snapshot.srtPoison[record];
        const auto found = std::lower_bound(guarded.begin(), guarded.end(), poison.slot);
        if (found == guarded.end() || *found != poison.slot) throw std::runtime_error("SRT read at flat offset " + std::to_string(poison.slot) + " reads inaccessible memory but has no guard");
        snapshot.flattenedSrt[flags + static_cast<std::size_t>(found - guarded.begin())] = record + 1u;
        snapshot.flattenedSrt.insert(snapshot.flattenedSrt.end(), {poison.pc, static_cast<std::uint32_t>(poison.address), static_cast<std::uint32_t>(poison.address >> 32u)});
    }
}

}

std::vector<ImageResource> ResourceMaterializer::RuntimeImageModes(const ImageResource& image) {
    std::vector<ImageResource> modes;
    const bool storage = image.resourceClass == ImageResourceClass::Storage;
    const auto append = [&](IrTextureNumericClass numeric, IrBufferFormat conversion, IrBufferFormat packed, bool depth, bool unorm16) {
        auto mode = image;
        mode.numericClass = numeric;
        mode.conversionFormat = conversion;
        mode.packedFormat = packed;
        mode.depthBits = depth;
        mode.depthUnorm16 = unorm16;
        mode.cube = false;
        mode.mipCount = mode.mipMode == ImageMipMode::DynamicStorage ? RuntimeAbi::StorageMipSlots : 1u;
        mode.shaderSwizzle = ShaderImageIdentitySwizzle;
        if (conversion == IrBufferFormat::Format11_11_10UNorm || conversion == IrBufferFormat::Format10_11_11Float) mode.shaderSwizzle = 0x2acu;
        modes.push_back(mode);
        if (image.dimension == RdnaImageDimension::Dim2D && image.fmaskCompatible && !depth && packed == IrBufferFormat::Invalid && image.byElements == 0u) {
            auto volume = mode;
            volume.dimension = RdnaImageDimension::Dim3D;
            modes.push_back(volume);
        }
        if (image.dimension == RdnaImageDimension::Dim3D && image.flatVolumeCompatible && !storage && !depth && conversion == IrBufferFormat::Invalid && packed == IrBufferFormat::Invalid) {
            auto plane = mode;
            plane.dimension = RdnaImageDimension::Dim2D;
            modes.push_back(plane);
        }
        if (image.dimension == RdnaImageDimension::Dim2D && storage && image.written && !image.read && !image.atomic && image.mipMode != ImageMipMode::DynamicStorage && !depth && packed == IrBufferFormat::Invalid && image.byElements == 0u) {
            auto line = mode;
            line.dimension = RdnaImageDimension::Dim1D;
            modes.push_back(line);
        }
        if (image.dimension == RdnaImageDimension::Dim2D && image.flatLineCompatible && !storage && !depth && conversion == IrBufferFormat::Invalid && packed == IrBufferFormat::Invalid) {
            auto line = mode;
            line.dimension = RdnaImageDimension::Dim1D;
            modes.push_back(line);
        }
        if (image.dimension == RdnaImageDimension::Dim1DArray || image.dimension == RdnaImageDimension::Dim2DArray || image.dimension == RdnaImageDimension::Dim2DMsaaArray) {
            auto plain = mode;
            plain.dimension = image.dimension == RdnaImageDimension::Dim1DArray ? RdnaImageDimension::Dim1D : image.dimension == RdnaImageDimension::Dim2DArray ? RdnaImageDimension::Dim2D : RdnaImageDimension::Dim2DMsaa;
            modes.push_back(plain);
        }
        if (image.dimension == RdnaImageDimension::Dim2DArray) {
            mode.cube = true;
            modes.push_back(mode);
        }
    };
    if (image.atomic) {
        append(IrTextureNumericClass::Uint, IrBufferFormat::Invalid, IrBufferFormat::Invalid, false, false);
    } else if (image.packed) {
        for (std::uint32_t value = 1u; value <= 77u; ++value) {
            const auto format = static_cast<IrBufferFormat>(value);
            const auto info = GetFormatInfo(format);
            if (info.packedBitfield || info.componentCount == 0u || (storage && info.byteSize == 12u)) continue;
            if (image.byElements != 0u && info.byteSize * 8u * image.byElements > 32u) continue;
            const auto numeric = storage && (format == IrBufferFormat::Format32SInt || format == IrBufferFormat::Format32_32SInt || format == IrBufferFormat::Format32_32_32_32SInt) ? IrTextureNumericClass::Uint : SampledTextureNumericClass(format);
            bool supported = numeric != IrTextureNumericClass::Unsupported && (!storage || numeric != IrTextureNumericClass::Sint);
            for (std::uint32_t component = 0u; component < info.componentCount; ++component) {
                const auto bits = info.componentBits[component];
                const bool exactRead = info.type == SpirvFormatComponentType::Uint || info.type == SpirvFormatComponentType::Sint || (info.type == SpirvFormatComponentType::Unorm && bits <= 16u) || (info.type == SpirvFormatComponentType::Float && bits == 32u);
                const bool exactWrite = info.type == SpirvFormatComponentType::Uint || (bits == 32u && (info.type == SpirvFormatComponentType::Sint || info.type == SpirvFormatComponentType::Float));
                supported &= storage ? exactWrite : exactRead;
            }
            if (supported) append(numeric, IrBufferFormat::Invalid, format, false, false);
        }
    } else {
        if ((image.emulatedCompare & EmulatedCompare::NativeOffsetUnsupported) == 0u) append(IrTextureNumericClass::Float, IrBufferFormat::Invalid, IrBufferFormat::Invalid, false, false);
        if (!image.depthCompare) {
            append(IrTextureNumericClass::Uint, IrBufferFormat::Invalid, IrBufferFormat::Invalid, false, false);
            if (!storage) append(IrTextureNumericClass::Sint, IrBufferFormat::Invalid, IrBufferFormat::Invalid, false, false);
            append(IrTextureNumericClass::Uint, IrBufferFormat::Format11_11_10UInt, IrBufferFormat::Invalid, false, false);
            if (image.srgbDecodeCompatible && image.depthBitsCompatible) {
                append(IrTextureNumericClass::Uint, IrBufferFormat::Format11_11_10UNorm, IrBufferFormat::Invalid, false, false);
                append(IrTextureNumericClass::Uint, IrBufferFormat::Format10_11_11Float, IrBufferFormat::Invalid, false, false);
            }
            if (!storage) {
                if (image.depthBitsCompatible) {
                    append(IrTextureNumericClass::Float, IrBufferFormat::Invalid, IrBufferFormat::Invalid, true, false);
                    append(IrTextureNumericClass::Float, IrBufferFormat::Invalid, IrBufferFormat::Invalid, true, true);
                }
                if (image.fmaskCompatible && image.indirectRoot == ImageResource::NoIndirectImage) append(IrTextureNumericClass::Float, IrBufferFormat::Invalid, IrBufferFormat::Fmask8_S2_F1, false, false);
            }
        }
    }
    if (storage && image.depthBitsCompatible && !image.packed && !image.atomic64 && ((image.written && !image.read) || image.atomic)) {
        constexpr std::array formats{IrBufferFormat::Format32SInt, IrBufferFormat::Format32_32SInt, IrBufferFormat::Format32_32_32_32SInt, IrBufferFormat::Format16SInt, IrBufferFormat::Format8_8SInt, IrBufferFormat::Format16_16SInt, IrBufferFormat::Format8_8_8_8SInt, IrBufferFormat::Format16_16_16_16SInt};
        for (const auto format : formats) {
            if (image.atomic && format != IrBufferFormat::Format32SInt) continue;
            append(IrTextureNumericClass::Uint, format, IrBufferFormat::Invalid, false, false);
        }
    }
    if (image.depthCompare && image.indirectRoot == ImageResource::NoIndirectImage && (image.emulatedCompare & EmulatedCompare::Unsupported) == 0u && (image.dimension == RdnaImageDimension::Dim2D || image.dimension == RdnaImageDimension::Dim2DArray)) {
        auto mode = image;
        mode.numericClass = IrTextureNumericClass::Float;
        mode.depthCompare = false;
        mode.cube = false;
        mode.emulatedCompare |= EmulatedCompare::Enabled;
        mode.conversionFormat = IrBufferFormat::Invalid;
        mode.packedFormat = IrBufferFormat::Invalid;
        mode.shaderSwizzle = ShaderImageIdentitySwizzle;
        modes.push_back(mode);
        if (image.dimension == RdnaImageDimension::Dim2DArray) {
            mode.cube = true;
            modes.push_back(mode);
        }
    }
    if (image.srgbDecodeFormats != 0u && image.srgbDecodeCompatible && !storage && !image.depthCompare && !image.packed) {
        const auto count = modes.size();
        for (std::size_t index = 0; index < count; ++index) {
            const auto& base = modes[index];
            if (base.numericClass != IrTextureNumericClass::Float || base.conversionFormat != IrBufferFormat::Invalid || base.packedFormat != IrBufferFormat::Invalid || base.depthBits) continue;
            auto mode = base;
            mode.srgbDecode = true;
            modes.push_back(mode);
        }
    }
    if (image.constantSwizzleCompatible && !storage && !image.depthCompare && !image.packed && image.indirectRoot == ImageResource::NoIndirectImage) {
        for (const auto numeric : {IrTextureNumericClass::Float, IrTextureNumericClass::Uint, IrTextureNumericClass::Sint}) {
            auto mode = image;
            mode.numericClass = numeric;
            mode.conversionFormat = IrBufferFormat::Invalid;
            mode.packedFormat = IrBufferFormat::Invalid;
            mode.depthBits = false;
            mode.depthUnorm16 = false;
            mode.cube = false;
            mode.mipCount = 1u;
            mode.shaderSwizzle = ShaderImageIdentitySwizzle;
            mode.constantSwizzle = true;
            modes.push_back(mode);
        }
    }
    if (modes.empty()) throw std::runtime_error("image instruction has no supported runtime modes");
    return modes;
}

std::uint32_t ResourceMaterializer::RuntimeImageMode(const ImageResource& image, const DescriptorValue& descriptor, std::span<const ImageResource> modes) {
    if (descriptor.dwordCount != 8u) throw std::runtime_error("runtime image descriptor must contain eight dwords");
    if (modes.empty()) throw std::runtime_error("prepared runtime image modes are missing");
    if (nullImageDescriptor(descriptor)) return 0u;
    const auto decoded = decodeImageDescriptor(descriptor, image, image.srgbDecodeFormats);
    const auto format = rawImageFormat(descriptor);
    const bool emulated = image.depthCompare && format != IrBufferFormat::Format32Float && format != IrBufferFormat::Format16UNorm && !IsDepthBitsTexture(descriptor.dwords[1], descriptor.dwords[3]);
    if (!emulated && (image.emulatedCompare & EmulatedCompare::NativeOffsetUnsupported) != 0u) throw std::runtime_error("native comparison with a nonconstant texel offset requires VK_KHR_maintenance8 and shaderImageGatherExtended");
    if (image.packed && decoded.packedFormat != IrBufferFormat::Invalid) {
        const auto format = GetFormatInfo(decoded.packedFormat);
        if (format.packedBitfield) throw std::runtime_error("runtime packed image accesses a bitfield format");
        const bool storage = image.resourceClass == ImageResourceClass::Storage;
        if (storage && format.byteSize == 12u) throw std::runtime_error("runtime packed image uses a format the hardware does not write");
        for (std::uint32_t component = 0u; component < format.componentCount; ++component) {
            const auto bits = format.componentBits[component];
            const bool exact = storage ? format.type == SpirvFormatComponentType::Uint || (bits == 32u && (format.type == SpirvFormatComponentType::Sint || format.type == SpirvFormatComponentType::Float)) : format.type == SpirvFormatComponentType::Uint || format.type == SpirvFormatComponentType::Sint || (format.type == SpirvFormatComponentType::Unorm && bits <= 16u) || (format.type == SpirvFormatComponentType::Float && bits == 32u);
            if (!exact) throw std::runtime_error(storage ? "runtime packed image bits are not reproducible through the view" : "runtime packed image bits are not recoverable from the view");
        }
    }
    if (decoded.mipCount > (image.mipMode == ImageMipMode::DynamicStorage ? RuntimeAbi::StorageMipSlots : 1u)) throw std::runtime_error("runtime storage image mip capacity exceeded");
    if ((descriptorImageSwizzle(descriptor) & 06666u) == 0u && !decoded.fmask && !decoded.depthBits && decoded.conversionFormat == IrBufferFormat::Invalid && !decoded.srgbDecode) {
        for (std::uint32_t index = 0u; index < modes.size(); ++index) {
            if (modes[index].constantSwizzle && modes[index].numericClass == decoded.numericClass) return index;
        }
    }
    for (std::uint32_t index = 0u; index < modes.size(); ++index) {
        const auto& mode = modes[index];
        if (((mode.emulatedCompare & EmulatedCompare::Enabled) != 0u) != emulated) continue;
        if (decoded.fmask) {
            if (mode.packedFormat == IrBufferFormat::Fmask8_S2_F1) return index;
            continue;
        }
        if (mode.numericClass == decoded.numericClass && mode.dimension == decoded.dimension && mode.conversionFormat == decoded.conversionFormat && mode.packedFormat == decoded.packedFormat && mode.cube == decoded.cube && mode.depthBits == decoded.depthBits && mode.depthUnorm16 == decoded.depthUnorm16 && mode.srgbDecode == decoded.srgbDecode) return index;
    }
    if (image.dimension == RdnaImageDimension::Dim1D && decoded.dimension != RdnaImageDimension::Dim1D) throw std::runtime_error("image address has too few coordinate components");
    const auto describe = [](IrTextureNumericClass numeric, RdnaImageDimension dimension, IrBufferFormat conversion, IrBufferFormat packed, bool cube, bool depthBits, bool depthUnorm16, bool srgb) {
        return "{class " + std::to_string(static_cast<int>(numeric)) + ", dim " + std::to_string(static_cast<int>(dimension)) + ", conversion " + std::to_string(static_cast<int>(conversion)) + ", packed " + std::to_string(static_cast<int>(packed)) + ", cube " + std::to_string(cube) + ", depthBits " + std::to_string(depthBits) + ", unorm16 " + std::to_string(depthUnorm16) + ", srgb " + std::to_string(srgb) + "}";
    };
    std::string message = "image descriptor is incompatible with the static runtime image interface: descriptor format " + std::to_string(static_cast<int>(format)) + " decodes to " + describe(decoded.numericClass, decoded.dimension, decoded.conversionFormat, decoded.packedFormat, decoded.cube, decoded.depthBits, decoded.depthUnorm16, decoded.srgbDecode) + (emulated ? " (emulated compare)" : "") + "; modes:";
    for (const auto& mode : modes) message += " " + describe(mode.numericClass, mode.dimension, mode.conversionFormat, mode.packedFormat, mode.cube, mode.depthBits, mode.depthUnorm16, mode.srgbDecode);
    char words[96];
    std::snprintf(words, sizeof(words), "; T# %08x %08x %08x %08x %08x %08x %08x %08x", descriptor.dwords[0], descriptor.dwords[1], descriptor.dwords[2], descriptor.dwords[3], descriptor.dwords[4], descriptor.dwords[5], descriptor.dwords[6], descriptor.dwords[7]);
    throw std::runtime_error(message + words);
}

std::uint32_t ResourceMaterializer::EmulatedCompareState(const ShaderInfo& info, const ResourceSnapshot& snapshot, std::uint32_t index) {
    return emulatedCompareState(info, snapshot, index);
}

void ResourceMaterializer::ApplyStaticInterface(IrProgram& program, bool nativeSampleOffsets) const {
    auto& resources = program.Resources();
    if (!resources.resourceTrackingComplete || !resources.srtPlanComplete) throw std::runtime_error("static resource interface requires a completed resource plan");
    auto images = resources.info.images;
    if (!nativeSampleOffsets) {
        for (const auto& block : program.Blocks()) {
            for (const auto* inst : block->Instructions()) {
                if (inst->Opcode() != IrOpcode::ImageSampleRaw) continue;
                const auto& memory = resources.memoryInfo.at(inst->Flags<MemoryFlags>().index);
                if ((memory.imageSampleFlags & (RdnaImageSampleFlagCompare | RdnaImageSampleFlagOffset)) != (RdnaImageSampleFlagCompare | RdnaImageSampleFlagOffset)) continue;
                const auto* address = inst->Argument(2)->Resolve();
                const auto component = GetRdnaImageAddressComponentLayout(memory.imageSampleFlags, 0u);
                const auto argument = component.bitOffset / 32u;
                if (component.bitWidth == 32u && argument < address->ArgumentCount() && address->Argument(argument)->Resolve()->HasImmediate()) continue;
                images.at(memory.resource).emulatedCompare |= EmulatedCompare::NativeOffsetUnsupported;
            }
        }
    }
    const auto directCount = static_cast<std::uint32_t>(images.size());
    auto mappingOffset = static_cast<std::uint32_t>(resources.srtReads.size());
    const auto slots = BindlessSlots();
    for (std::uint32_t index = 0u; index < directCount; ++index) {
        auto image = images[index];
        image.srgbDecodeFormats = resources.srgbDecodeFormats;
        if (image.indirectRoot != ImageResource::NoIndirectImage) throw std::runtime_error("static image interface was already expanded");
        image.numericClass = image.atomic ? IrTextureNumericClass::Uint : IrTextureNumericClass::Float;
        image.mipCount = image.mipMode == ImageMipMode::DynamicStorage ? RuntimeAbi::StorageMipSlots : 1u;
        if (resources.descriptorSources.at(image.source).indirectImage.has_value()) {
            if (images.size() + slots - 1u > ShaderInfo::MaxImages) throw std::runtime_error("static bindless image capacity exceeded");
            image.indirectRoot = index;
            image.indirectResources.push_back(index);
            for (std::uint32_t slot = 1u; slot < slots; ++slot) {
                auto entry = image;
                entry.indirectResources.clear();
                image.indirectResources.push_back(static_cast<std::uint32_t>(images.size()));
                images.push_back(std::move(entry));
            }
            image.indirectMappingOffset = mappingOffset;
            image.indirectSearchIterations = static_cast<std::uint32_t>(std::bit_width(slots));
            mappingOffset += 1u + 2u * slots;
        }
        images[index] = std::move(image);
    }
    for (const auto& pair : resources.info.sampledPairs) {
        if (pair.image >= images.size() || pair.sampler >= resources.info.samplers.size()) throw std::runtime_error("static sampled pair is out of range");
        auto& sampler = resources.info.samplers[pair.sampler];
        sampler.depthCompare = sampler.depthCompare || images[pair.image].depthCompare;
    }
    resources.info.images = std::move(images);
    resources.guardedSrtSlots = Detail::ComputeGuardedFlatSlots(resources);
    resources.srtGuardOffset = mappingOffset;
    if (!resources.guardedSrtSlots.empty()) resources.info.usesFaultBuffer = true;
    PrepareImageModes(resources.info);
}

void ResourceMaterializer::PrepareImageModes(ShaderInfo& info) {
    info.runtimeImageModes.clear();
    info.runtimeImageModes.reserve(info.images.size());
    for (const auto& image : info.images) info.runtimeImageModes.push_back(RuntimeImageModes(image));
}

namespace {

void ownPlanValues(IrResourcePlan& plan) {
    std::vector<IrValue**> roots;
    for (auto& source : plan.descriptorSources) {
        for (auto& dword : source.dwords) roots.push_back(&dword);
    }
    for (auto& read : plan.srtReads) roots.push_back(&read.value);
    for (auto& block : plan.controlFlow) roots.push_back(&block.condition);
    for (auto& value : plan.uniformFill.values) roots.push_back(&value);

    std::unordered_map<const IrValue*, IrValue*> clones;
    std::vector<const IrValue*> order;
    std::vector<const IrValue*> pending;
    for (const auto* root : roots) {
        if (*root != nullptr) pending.push_back(*root);
    }
    while (!pending.empty()) {
        const auto* value = pending.back();
        pending.pop_back();
        if (!clones.emplace(value, nullptr).second) continue;
        order.push_back(value);
        for (const auto* argument : value->Arguments()) {
            if (argument != nullptr) pending.push_back(argument);
        }
    }
    for (const auto* value : order) {
        auto clone = std::make_unique<IrValue>(value->Opcode(), value->Type(), value->Id());
        clone->SetFlags(value->Flags<std::uint64_t>());
        if (value->HasImmediate()) clone->SetImmediateU64(value->ImmediateU64());
        clone->SetRegister(value->Register());
        clones[value] = clone.get();
        plan.valueStorage.push_back(std::move(clone));
    }
    std::unordered_map<const IrBlock*, IrBlock*> blocks;
    const auto blockFor = [&](const IrBlock* block) {
        auto& clone = blocks[block];
        if (clone == nullptr) {
            plan.blockStorage.push_back(std::make_unique<IrBlock>(block->Id()));
            clone = plan.blockStorage.back().get();
        }
        return clone;
    };
    for (const auto* value : order) {
        auto* clone = clones.at(value);
        for (std::size_t index = 0; index < value->ArgumentCount(); index++) {
            const auto* argument = value->Argument(index);
            auto* mapped = argument == nullptr ? nullptr : clones.at(argument);
            if (value->IsPhi()) {
                clone->AddPhiOperand(blockFor(value->PhiBlock(index)), mapped);
            } else {
                clone->AddArgument(mapped);
            }
        }
    }
    for (auto* root : roots) {
        if (*root != nullptr) *root = clones.at(*root);
    }
}

}

IrResourcePlan ResourceMaterializer::ExtractPlan(const IrProgram& program) const {
    const IrResourcePlan& source = program.Resources();
    if (!source.resourceTrackingComplete || !source.srtPlanComplete) {
        throw std::runtime_error("ResourceMaterializer::ExtractPlan requires a completed resource and SRT plan");
    }
    IrResourcePlan plan;
    plan.stage = source.stage;
    plan.shaderHash = source.shaderHash;
    plan.userDataBase = source.userDataBase;
    plan.userDataCount = source.userDataCount;
    plan.srgbDecodeFormats = source.srgbDecodeFormats;
    plan.memoryInfo = source.memoryInfo;
    plan.descriptorSources = source.descriptorSources;
    plan.controlFlow = source.controlFlow;
    plan.srtReads = source.srtReads;
    plan.cleanFlatSlots = source.cleanFlatSlots;
    plan.requiresSpecializationMemory = source.requiresSpecializationMemory;
    plan.srtPlanComplete = source.srtPlanComplete;
    plan.resourceTrackingComplete = source.resourceTrackingComplete;
    plan.info = source.info;
    PrepareImageModes(plan.info);
    plan.uniformFill = source.uniformFill;
    ownPlanValues(plan);
    const auto addSource = [&plan](std::uint32_t index) {
        if (index >= plan.descriptorSources.size()) {
            throw std::runtime_error("ResourceMaterializer::ExtractPlan resource references an unknown descriptor source");
        }
        plan.materializationSources.push_back(index);
    };
    for (const auto& buffer : plan.info.buffers) addSource(buffer.source);
    for (const auto& image : plan.info.images) {
        if (image.source >= plan.descriptorSources.size()) {
            throw std::runtime_error("ResourceMaterializer::ExtractPlan image references an unknown descriptor source");
        }
        if (plan.descriptorSources[image.source].indirectImage.has_value()) {
            plan.requiresSpecializationMemory = true;
        } else {
            addSource(image.source);
        }
    }
    for (const auto& sampler : plan.info.samplers) addSource(sampler.source);
    plan.pureFlatSlots = Detail::ComputePureFlatSlots(plan);
    plan.guardedSrtSlots = Detail::ComputeGuardedFlatSlots(plan);
    return plan;
}

void ResourceMaterializer::Materialize(const IrResourcePlan& program, const SrtRuntime& runtime, ResourceSnapshot& snapshot) const {
    const IrResourcePlan& plan = program;
    if (!plan.resourceTrackingComplete) {
        throw std::runtime_error("ResourceMaterializer::Materialize requires a completed resource plan");
    }
    if (plan.requiresSpecializationMemory && runtime.readMemory == nullptr) {
        throw std::runtime_error("ResourceMaterializer::Materialize requires runtime memory access for indirect images");
    }
    SrtWalker walker;
    ResourceSnapshot nextSnapshot;
    std::vector<TableResolution> tables;
    try {
        materializeSnapshot(plan, runtime, walker, nextSnapshot, tables);
    } catch (...) {
        reportBindless();
        throw;
    }
    const auto started = MaterializeProfiled() ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
    materializeTables(plan, nextSnapshot, tables);
    materializeSrtGuards(plan, nextSnapshot);
    if (MaterializeProfiled()) specializationNanoseconds.fetch_add(static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - started).count()), std::memory_order_relaxed);
    snapshot = std::move(nextSnapshot);
    reportBindless();
}

std::uint64_t ResourceMaterializer::SpecializationNanoseconds() {
    return specializationNanoseconds.load(std::memory_order_relaxed);
}

std::uint32_t ResourceMaterializer::BindlessSlots() {
    return RuntimeAbi::BindlessTableSlots;
}

void ResourceMaterializer::CountBindlessRejection(BindlessRejection reason) {
    if (reason < BindlessRejection::Count) bindlessCounters().rejected[static_cast<std::size_t>(reason)].fetch_add(1, std::memory_order_relaxed);
}

}
