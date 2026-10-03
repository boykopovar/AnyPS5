#include "Optimization/ResourceMaterializer.hpp"
#include "Optimization/SrtWalker/SrtFlatSlotClasses.hpp"
#include "IntermediateRepresentation/IrBuilder.hpp"
#include "Optimization/ShaderStageInputInfo.hpp"
#include "RdnaDecoder/RdnaDescriptorFormat.hpp"
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
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace ShaderRecompiler {

namespace {

constexpr std::uint32_t NoRemap = std::numeric_limits<std::uint32_t>::max();

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
    bool empty = false;
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

std::uint32_t imageBaseLevel(const DescriptorValue& descriptor) {
    return (descriptor.dwords[3] >> 12u) & 0xfu;
}

std::uint32_t imageLastLevel(const DescriptorValue& descriptor) {
    return (descriptor.dwords[3] >> 16u) & 0xfu;
}

std::uint32_t imageMaxMip(const DescriptorValue& descriptor) {
    return (descriptor.dwords[5] >> 4u) & 0xfu;
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
        const auto baseLevel = imageBaseLevel(descriptor);
        const auto fragments = imageLastLevel(descriptor);
        const auto maxMip = imageMaxMip(descriptor);
        return baseLevel == 0u && fragments >= 1u && fragments <= 3u && (r128 || maxMip == fragments);
    }
    return true;
}

std::uint32_t storageMipCount(const ImageResource& base, const DescriptorValue& descriptor) {
    if (base.mipMode != ImageMipMode::DynamicStorage || nullImageDescriptor(descriptor)) {
        return 1u;
    }
    const auto mipBase = imageBaseLevel(descriptor);
    const auto mipLast = imageLastLevel(descriptor);
    return mipBase <= mipLast ? mipLast - mipBase + 1u : 0u;
}

DecodedImage decodeImageDescriptor(const DescriptorValue& descriptor, const ImageResource& base) {
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
    if (descriptor.dwordCount == 8u && base.mipMode != ImageMipMode::DynamicStorage) {
        decoded.empty = imageBaseLevel(descriptor) > imageMaxMip(descriptor);
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
    if (base.atomic && format != IrBufferFormat::Format32UInt) {
        throw std::runtime_error("atomic image descriptor uses an unsupported format");
    }
    const bool storage = base.resourceClass == ImageResourceClass::Storage;
    decoded.fmask = IsFmaskTextureFormat(format);
    if (decoded.fmask && (storage || base.depthCompare || base.indirectRoot != ImageResource::NoIndirectImage)) {
        throw std::runtime_error("FMASK requires a direct sampled image load");
    }
    decoded.conversionFormat = RemapTextureFormat(format) != format ? format : IrBufferFormat::Invalid;
    if (storage || decoded.conversionFormat != IrBufferFormat::Invalid) {
        decoded.shaderSwizzle = descriptorImageSwizzle(descriptor);
    }
    const bool rawSintStorage = storage && format == IrBufferFormat::Format32SInt && base.written && !base.read && !base.atomic;
    decoded.numericClass = SampledTextureNumericClass(format);
    if (!storage && !base.depthCompare && IsDepthBitsTexture(descriptor.dwords[1], descriptor.dwords[3])) {
        decoded.depthBits = true;
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
    } else if (decoded.numericClass == IrTextureNumericClass::Unsupported || (base.depthCompare && decoded.numericClass != IrTextureNumericClass::Float)) {
        throw std::runtime_error("sampled image descriptor uses an unsupported format");
    }
    return decoded;
}

bool requiresPointSampler(const ResourceSpecialization::Image& image) {
    return image.numericClass == IrTextureNumericClass::Sint || image.conversionFormat != IrBufferFormat::Invalid || image.depthBits;
}

constexpr std::uint32_t TableEntryBytes = 32;

// Bindless image tables: the most material records mode M reads to enumerate a table's keys
// (APS5_BINDLESS_MATERIAL_SCAN, default 256); beyond it the whole table is bound instead.
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

// A bindless image table's bound slots and its (key, slot) mapping, keys ascending. Slots the
// mapping does not name (past the keys, or an entry that is null, invalid or of another shape)
// hold a copy of the first usable entry: a key the mapping lacks samples zeros in the shader, as
// a null T# does on hardware. Empty when the image is no table root.
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
    const auto slots = ResourceMaterializer::BindlessSlots();
    DescriptorValue heapValue;
    walker.EvaluateDescriptorSource(plan, table.heapSource, runtime, heapValue);
    const ShaderBufferResource heap = decodeBufferDescriptor(heapValue);
    const std::uint64_t heapSize = heap.GetSize();
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
    if (table.hasMaterial && table.selectorStride != 0u) {
        DescriptorValue materialValue;
        walker.EvaluateDescriptorSource(plan, table.materialSource, runtime, materialValue);
        const ShaderBufferResource material = decodeBufferDescriptor(materialValue);
        materialBase = material.Base48();
        const std::uint64_t materialSize = material.GetSize();
        materialEntries = static_cast<std::uint32_t>(std::min<std::uint64_t>(materialSize / table.selectorStride, std::numeric_limits<std::uint32_t>::max()));
        if (materialEntries <= MaterialScanLimit()) {
            materialMode = true;
            for (std::uint32_t record = 0; record < materialEntries; record++) {
                const std::uint64_t offset = static_cast<std::uint64_t>(record) * table.selectorStride + table.selectorOffset;
                if (offset + sizeof(std::uint32_t) > materialSize) break;
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
    if (keys.empty()) {
        rejectTable(BindlessRejection::NoEntry, "bindless image table selects no entry");
    }

    const std::uint64_t heapBase = heap.Base48();
    std::vector<DescriptorValue> candidates(keys.size());
    std::vector<std::uint8_t> valid(keys.size(), 0u);
    std::optional<DecodedImage> shape;
    std::uint32_t paddedNull = 0;
    std::uint32_t paddedShape = 0;
    std::uint32_t paddedConversion = 0;
    for (std::size_t i = 0; i < keys.size(); i++) {
        auto& candidate = candidates[i];
        candidate.dwordCount = 8u;
        const std::uint64_t address = heapBase + table.entryOffset + static_cast<std::uint64_t>(keys[i]) * TableEntryBytes;
        for (std::uint32_t dword = 0; dword < 8u; dword++) {
            readWord(address + dword * sizeof(std::uint32_t), candidate.dwords[dword]);
        }
        DecodedImage decoded;
        bool usable = !nullImageDescriptor(candidate) && validImageDescriptor(candidate, image.r128);
        if (usable) {
            try {
                decoded = decodeImageDescriptor(candidate, image);
            } catch (const std::exception&) {
                usable = false;
            }
        }
        if (!usable) {
            paddedNull++;
            continue;
        }
        if (decoded.conversionFormat != IrBufferFormat::Invalid || decoded.fmask) {
            paddedConversion++;
            continue;
        }
        if (shape.has_value() && (decoded.numericClass != shape->numericClass || decoded.dimension != shape->dimension || decoded.cube != shape->cube)) {
            paddedShape++;
            continue;
        }
        if (!shape.has_value()) shape = decoded;
        valid[i] = 1u;
    }
    if (!shape.has_value()) {
        rejectTable(BindlessRejection::NoEntry, "bindless image table has no valid entry");
    }
    const auto pad = candidates[static_cast<std::size_t>(std::find(valid.begin(), valid.end(), std::uint8_t{1}) - valid.begin())];
    resolution.mapping.clear();
    for (std::size_t i = 0; i < keys.size(); i++) {
        if (valid[i] == 0u) {
            candidates[i] = pad;
            continue;
        }
        resolution.mapping.emplace_back(keys[i], static_cast<std::uint32_t>(i));
    }
    resolution.slots = std::move(candidates);
    resolution.slots.resize(slots, pad);
    resolved = resolution.slots[0];

    (materialMode ? counters.tablesMaterial : counters.tablesWhole).fetch_add(1, std::memory_order_relaxed);
    counters.keys.fetch_add(resolution.mapping.size(), std::memory_order_relaxed);
    counters.paddedNull.fetch_add(paddedNull, std::memory_order_relaxed);
    counters.paddedShape.fetch_add(paddedShape, std::memory_order_relaxed);
    counters.paddedConversion.fetch_add(paddedConversion, std::memory_order_relaxed);
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
    walker.EvaluateRuntimeSources(plan, plan.materializationSources, runtime, values, snapshot.flattenedSrt, plan.cleanFlatSlots, activeSources);

    std::size_t cursor = 0;
    if (values.size() < plan.info.buffers.size()) {
        throw std::runtime_error("materialization sources are missing buffer descriptors");
    }
    snapshot.buffers.assign(values.begin(), values.begin() + plan.info.buffers.size());
    cursor += plan.info.buffers.size();

    snapshot.images.resize(plan.info.images.size());
    tables.assign(plan.info.images.size(), {});
    std::uint32_t activeTables = 0;
    for (std::uint32_t i = 0; i < plan.info.images.size(); i++) {
        const auto& image = plan.info.images[i];
        if (image.source >= plan.descriptorSources.size()) {
            throw std::runtime_error("image resource references an unknown descriptor source");
        }
        const bool active = image.source >= activeSources.size() || activeSources[image.source] != 0u;
        if (plan.descriptorSources[image.source].indirectImage.has_value() && active) activeTables++;
    }
    const auto tableSlots = ResourceMaterializer::BindlessSlots();
    if (activeTables != 0u && plan.info.images.size() + static_cast<std::size_t>(tableSlots - 1u) * activeTables > ShaderInfo::MaxImages) {
        rejectTable(BindlessRejection::ImageSlots, "bindless image tables need " + std::to_string(plan.info.images.size() + static_cast<std::size_t>(tableSlots - 1u) * activeTables) + " image slots, limit " + std::to_string(ShaderInfo::MaxImages));
    }
    for (std::uint32_t i = 0; i < plan.info.images.size(); i++) {
        const auto& image = plan.info.images[i];
        const auto& source = plan.descriptorSources[image.source];
        if (source.indirectImage.has_value()) {
            if (image.source < activeSources.size() && activeSources[image.source] == 0u) {
                snapshot.images[i].dwordCount = 8u;
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
            descriptor.dwords.fill(0u);
        }
        snapshot.images[i] = descriptor;
    }

    if (values.size() < cursor + plan.info.samplers.size()) {
        throw std::runtime_error("materialization sources are missing sampler descriptors");
    }
    snapshot.samplers.assign(values.begin() + cursor, values.begin() + cursor + plan.info.samplers.size());
}

void buildResourceSpecialization(const IrResourcePlan& plan, ResourceSnapshot& snapshot, const std::vector<TableResolution>& tables, ResourceSpecialization& specialization) {
    ResourceSpecialization result;
    result.buffers.reserve(plan.info.buffers.size());
    for (std::uint32_t i = 0; i < plan.info.buffers.size(); i++) {
        const ShaderBufferResource decoded = decodeBufferDescriptor(snapshot.buffers[i]);
        if (decoded.Type() != 0u) {
            throw std::runtime_error("buffer descriptor uses an unsupported type");
        }
        auto packedStride = decoded.PackedStride();
        const auto stride = packedStride & 0x3fffu;
        const bool swizzleActive = stride != 0u && ((packedStride >> 14u) & 1u) != 0u;
        if (stride == 0u) {
            packedStride &= ~((1u << 14u) | (3u << 16u));
        } else if (!swizzleActive) {
            packedStride &= ~(3u << 16u);
        }
        const auto& buffer = plan.info.buffers[i];
        ResourceSpecialization::Buffer entry;
        entry.packedStride = packedStride;
        entry.descriptorFormat = buffer.formatted ? decoded.Format() : IrBufferFormat::Invalid;
        entry.descriptorSwizzle = buffer.formatted ? decoded.DstSelXYZW() : DstSel(4, 5, 6, 7);
        entry.empty = decoded.GetSize() == 0u || decoded.Base48() == 0u;
        result.buffers.push_back(entry);
    }

    result.images.reserve(plan.info.images.size());
    for (std::uint32_t i = 0; i < plan.info.images.size(); i++) {
        const auto& image = plan.info.images[i];
        const DecodedImage decoded = decodeImageDescriptor(snapshot.images[i], image);
        if (decoded.fmask && std::any_of(plan.info.sampledPairs.begin(), plan.info.sampledPairs.end(), [i](const SampledResourcePair& pair) { return pair.image == i; })) {
            throw std::runtime_error("FMASK requires a direct image load");
        }
        ResourceSpecialization::Image entry;
        entry.numericClass = decoded.numericClass;
        entry.dimension = decoded.dimension;
        entry.mipCount = decoded.mipCount;
        entry.conversionFormat = decoded.conversionFormat;
        entry.shaderSwizzle = decoded.shaderSwizzle;
        entry.indirectRoot = ImageResource::NoIndirectImage;
        entry.indirectMappingOffset = 0u;
        entry.indirectSearchIterations = 0u;
        entry.cube = decoded.cube;
        entry.fmask = decoded.fmask;
        entry.depthBits = decoded.depthBits;
        entry.depthUnorm16 = decoded.depthUnorm16;
        entry.empty = decoded.empty;
        result.images.push_back(entry);
    }

    // A table root is followed by its slots 1..C-1 as extra images of the root's shape; the
    // (key, slot) mapping the SPIR-V selector searches is appended to the flattened SRT in a
    // block of fixed size, so the offsets (part of the specialization) never depend on the keys.
    for (std::uint32_t i = 0; i < plan.info.images.size(); i++) {
        const auto& table = tables[i];
        if (table.slots.empty()) {
            continue;
        }
        const auto mappingOffset = static_cast<std::uint32_t>(snapshot.flattenedSrt.size());
        snapshot.flattenedSrt.push_back(static_cast<std::uint32_t>(table.mapping.size()));
        for (const auto& [key, slot] : table.mapping) {
            snapshot.flattenedSrt.push_back(key);
            snapshot.flattenedSrt.push_back(slot);
        }
        snapshot.flattenedSrt.resize(mappingOffset + 1u + 2u * table.slots.size(), 0u);
        auto root = result.images[i];
        root.indirectRoot = i;
        root.indirectMappingOffset = mappingOffset;
        root.indirectSearchIterations = static_cast<std::uint32_t>(std::bit_width(table.slots.size()));
        result.images[i] = root;
        root.indirectMappingOffset = 0u;
        root.indirectSearchIterations = 0u;
        for (std::uint32_t slot = 1; slot < table.slots.size(); slot++) {
            result.images.push_back(root);
            snapshot.images.push_back(table.slots[slot]);
        }
    }

    result.boundDescriptors.clear();
    result.boundDescriptors.reserve(result.buffers.size() + result.images.size());
    for (std::uint32_t index = 0; index < result.buffers.size(); index++) {
        result.boundDescriptors.push_back(index);
    }
    for (std::uint32_t index = 0; index < result.images.size(); index++) {
        result.boundDescriptors.push_back(index);
    }
    specialization = std::move(result);
}

}

void ResourceMaterializer::Apply(IrProgram& program, const ResourceSpecialization& specialization) const {
    IrResourcePlan& resources = program.Resources();
    if (!resources.resourceTrackingComplete) {
        throw std::runtime_error("ResourceMaterializer::Apply requires a completed resource plan");
    }
    if (resources.info.buffers.size() != specialization.buffers.size()) {
        throw std::runtime_error("ResourceMaterializer::Apply buffer count mismatch");
    }
    if (resources.info.images.size() > specialization.images.size()) {
        throw std::runtime_error("ResourceMaterializer::Apply image count mismatch");
    }

    auto buffers = resources.info.buffers;
    for (std::uint32_t i = 0; i < buffers.size(); i++) {
        buffers[i].packedStride = specialization.buffers[i].packedStride;
        buffers[i].descriptorFormat = specialization.buffers[i].descriptorFormat;
        buffers[i].descriptorSwizzle = specialization.buffers[i].descriptorSwizzle;
        buffers[i].empty = specialization.buffers[i].empty;
    }

    auto images = resources.info.images;
    images.reserve(specialization.images.size());
    for (std::uint32_t index = 0; index < specialization.images.size(); index++) {
        const auto& source = specialization.images[index];
        if (index >= images.size()) {
            if (source.indirectRoot >= resources.info.images.size()) {
                throw std::runtime_error("ResourceMaterializer::Apply indirect image root is out of range");
            }
            images.push_back(resources.info.images[source.indirectRoot]);
        }
        auto& image = images[index];
        image.numericClass = source.numericClass;
        image.dimension = source.dimension;
        image.mipCount = source.mipCount;
        image.conversionFormat = source.conversionFormat;
        image.shaderSwizzle = source.shaderSwizzle;
        image.indirectRoot = source.indirectRoot;
        image.indirectMappingOffset = source.indirectMappingOffset;
        image.indirectSearchIterations = source.indirectSearchIterations;
        image.cube = source.cube;
        image.depthBits = source.depthBits;
        image.depthUnorm16 = source.depthUnorm16;
        image.empty = source.empty;
        image.indirectResources.clear();
    }
    for (std::uint32_t index = 0; index < images.size(); index++) {
        const auto root = images[index].indirectRoot;
        if (root != ImageResource::NoIndirectImage) {
            if (root >= images.size()) {
                throw std::runtime_error("ResourceMaterializer::Apply indirect image root is out of range");
            }
            images[root].indirectResources.push_back(index);
        }
    }

    std::vector<std::uint32_t> pointSampler(resources.info.samplers.size(), NoRemap);
    std::uint32_t samplerCount = static_cast<std::uint32_t>(resources.info.samplers.size());
    std::vector<std::uint8_t> samplerUsage(resources.info.samplers.size(), 0u);
    for (const auto& pair : resources.info.sampledPairs) {
        if (pair.image >= images.size() || pair.sampler >= resources.info.samplers.size()) {
            throw std::runtime_error("ResourceMaterializer::Apply sampled pair is out of range");
        }
        samplerUsage[pair.sampler] |= requiresPointSampler(specialization.images[pair.image]) ? 2u : 1u;
    }
    for (std::uint32_t index = 0; index < resources.info.samplers.size(); index++) {
        if ((samplerUsage[index] & 2u) == 0u) {
            continue;
        }
        if ((samplerUsage[index] & 1u) == 0u) {
            pointSampler[index] = index;
        } else {
            if (samplerCount >= ShaderInfo::MaxSamplers) {
                throw std::runtime_error("ResourceMaterializer::Apply exceeds the sampler resource limit");
            }
            pointSampler[index] = samplerCount;
            samplerCount++;
        }
    }
    auto samplers = resources.info.samplers;
    auto sampledPairs = resources.info.sampledPairs;
    samplers.reserve(samplerCount);
    for (std::uint32_t index = 0; index < resources.info.samplers.size(); index++) {
        const auto target = pointSampler[index];
        if (target == NoRemap) {
            continue;
        }
        if (target == index) {
            samplers[index].forcePointFiltering = true;
        } else {
            if (target != samplers.size()) {
                throw std::runtime_error("ResourceMaterializer::Apply sampler plan is inconsistent");
            }
            auto sampler = samplers[index];
            sampler.forcePointFiltering = true;
            samplers.push_back(sampler);
        }
    }
    for (auto& pair : sampledPairs) {
        if (requiresPointSampler(specialization.images[pair.image])) {
            if (pointSampler[pair.sampler] == NoRemap) {
                throw std::runtime_error("ResourceMaterializer::Apply missing point sampler for pair");
            }
            pair.sampler = pointSampler[pair.sampler];
        }
        samplers[pair.sampler].depthCompare = samplers[pair.sampler].depthCompare || images[pair.image].depthCompare;
    }

    auto memoryInfo = resources.memoryInfo;
    std::vector<std::uint32_t> imageRemap(specialization.images.size());
    std::uint32_t remapCount = 0;
    for (std::uint32_t i = 0; i < specialization.images.size(); i++) {
        imageRemap[i] = specialization.images[i].fmask ? NoRemap : remapCount;
        if (!specialization.images[i].fmask) {
            remapCount++;
        }
    }

    IrBuilder builder(program);
    for (auto& block : program.Blocks()) {
        builder.SetInsertionPoint(*block);
        for (auto* value : block->Instructions()) {
            const auto imageOpcode = ImageOpcodeInfoOf(value->Opcode());
            if (imageOpcode.access == ImageAccess::None) {
                continue;
            }
            const auto flags = value->Flags<MemoryFlags>();
            if (flags.index >= memoryInfo.size()) {
                throw std::runtime_error("ResourceMaterializer::Apply memory info index is out of range");
            }
            auto& memory = memoryInfo[flags.index];
            if (memory.resource >= images.size()) {
                throw std::runtime_error("ResourceMaterializer::Apply memory resource index is out of range");
            }
            if (specialization.images[memory.resource].fmask) {
                if (value->Opcode() != IrOpcode::ImageRead || memory.dataBits != 32u) {
                    throw std::runtime_error("ResourceMaterializer::Apply found an unsupported FMASK access");
                }
                constexpr std::array<std::uint32_t, 2> fragmentIndices{0x76543210u, 0xfedcba98u};
                std::array<IrValue*, 2> fragments{};
                for (std::uint32_t component = 0; component < fragments.size(); component++) {
                    IrValue& selected = program.CreateValue(IrOpcode::SelectU32, IrType::U32);
                    selected.AddArgument(value->Argument(2));
                    selected.AddArgument(&builder.Constant(fragmentIndices[component]));
                    selected.AddArgument(&builder.Constant(0u));
                    block->InsertInstructionBefore(value, &selected);
                    fragments[component] = &selected;
                }
                IrValue& result = program.CreateValue(IrOpcode::CompositeConstructU32x4, IrType::U32x4);
                result.AddArgument(fragments[0]);
                result.AddArgument(fragments[1]);
                result.AddArgument(&builder.Constant(0u));
                result.AddArgument(&builder.Constant(0u));
                block->InsertInstructionBefore(value, &result);
                value->ReplaceAllUsesWith(&result);
                continue;
            }
            if (imageOpcode.needsSampler && requiresPointSampler(specialization.images[memory.resource]) && memory.sampler < resources.info.samplers.size()) {
                if (pointSampler[memory.sampler] == NoRemap) {
                    throw std::runtime_error("ResourceMaterializer::Apply missing point sampler for image access");
                }
                memory.sampler = pointSampler[memory.sampler];
            }
        }
    }

    for (auto& memory : memoryInfo) {
        if (memory.kind == ResourceKind::Image && !memory.planningOnly) {
            if (memory.resource >= imageRemap.size() || imageRemap[memory.resource] == NoRemap) {
                throw std::runtime_error("ResourceMaterializer::Apply cannot remap an image memory reference");
            }
            memory.resource = imageRemap[memory.resource];
        }
    }
    for (auto& buffer : buffers) {
        if (buffer.imageAlias != BufferResource::NoImageAlias) {
            if (buffer.imageAlias >= imageRemap.size() || imageRemap[buffer.imageAlias] == NoRemap) {
                throw std::runtime_error("ResourceMaterializer::Apply cannot remap a buffer image alias");
            }
            buffer.imageAlias = imageRemap[buffer.imageAlias];
        }
    }
    for (auto& pair : sampledPairs) {
        if (pair.image >= imageRemap.size() || imageRemap[pair.image] == NoRemap) {
            throw std::runtime_error("ResourceMaterializer::Apply cannot remap a sampled pair image");
        }
        pair.image = imageRemap[pair.image];
    }
    for (auto& image : images) {
        if (image.indirectRoot != ImageResource::NoIndirectImage) {
            if (image.indirectRoot >= imageRemap.size() || imageRemap[image.indirectRoot] == NoRemap) {
                throw std::runtime_error("ResourceMaterializer::Apply cannot remap an indirect image root");
            }
            image.indirectRoot = imageRemap[image.indirectRoot];
        }
        for (auto& resource : image.indirectResources) {
            if (resource >= imageRemap.size() || imageRemap[resource] == NoRemap) {
                throw std::runtime_error("ResourceMaterializer::Apply cannot remap an indirect image resource");
            }
            resource = imageRemap[resource];
        }
    }
    if (images.size() != imageRemap.size()) {
        throw std::runtime_error("ResourceMaterializer::Apply image remap size mismatch");
    }
    for (std::uint32_t index = 0; index < images.size(); index++) {
        if (imageRemap[index] != NoRemap && imageRemap[index] != index) {
            images[imageRemap[index]] = std::move(images[index]);
        }
    }
    images.resize(remapCount);

    resources.info.buffers = std::move(buffers);
    resources.info.images = std::move(images);
    resources.info.samplers = std::move(samplers);
    resources.info.sampledPairs = std::move(sampledPairs);
    resources.memoryInfo = std::move(memoryInfo);
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
    plan.memoryInfo = source.memoryInfo;
    plan.descriptorSources = source.descriptorSources;
    plan.controlFlow = source.controlFlow;
    plan.srtReads = source.srtReads;
    plan.cleanFlatSlots = source.cleanFlatSlots;
    plan.requiresSpecializationMemory = source.requiresSpecializationMemory;
    plan.srtPlanComplete = source.srtPlanComplete;
    plan.resourceTrackingComplete = source.resourceTrackingComplete;
    plan.info = source.info;
    plan.uniformFill = source.uniformFill;
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
    return plan;
}

void ResourceMaterializer::Materialize(const IrResourcePlan& program, const SrtRuntime& runtime, ResourceSnapshot& snapshot, ResourceSpecialization& specialization) const {
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
    ResourceSpecialization nextSpecialization;
    const auto started = MaterializeProfiled() ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
    buildResourceSpecialization(plan, nextSnapshot, tables, nextSpecialization);
    if (MaterializeProfiled()) specializationNanoseconds.fetch_add(static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - started).count()), std::memory_order_relaxed);
    snapshot = std::move(nextSnapshot);
    specialization = std::move(nextSpecialization);
    reportBindless();
}

std::uint64_t ResourceMaterializer::SpecializationNanoseconds() {
    return specializationNanoseconds.load(std::memory_order_relaxed);
}

std::uint32_t ResourceMaterializer::BindlessSlots() {
    static const std::uint32_t slots = [] {
        const char* text = std::getenv("APS5_BINDLESS_SLOTS");
        const auto value = text != nullptr ? std::strtoul(text, nullptr, 0) : 16ul;
        return static_cast<std::uint32_t>(std::clamp<unsigned long>(value, 1ul, 48ul));
    }();
    return slots;
}

void ResourceMaterializer::CountBindlessRejection(BindlessRejection reason) {
    if (reason < BindlessRejection::Count) bindlessCounters().rejected[static_cast<std::size_t>(reason)].fetch_add(1, std::memory_order_relaxed);
}

bool ResourceSpecialization::Buffer::operator==(const Buffer& other) const {
    return packedStride == other.packedStride && descriptorFormat == other.descriptorFormat && descriptorSwizzle == other.descriptorSwizzle && empty == other.empty;
}

bool ResourceSpecialization::Image::operator==(const Image& other) const {
    return numericClass == other.numericClass && dimension == other.dimension && mipCount == other.mipCount && conversionFormat == other.conversionFormat && shaderSwizzle == other.shaderSwizzle && indirectRoot == other.indirectRoot && indirectMappingOffset == other.indirectMappingOffset && indirectSearchIterations == other.indirectSearchIterations && cube == other.cube && fmask == other.fmask && depthBits == other.depthBits && depthUnorm16 == other.depthUnorm16 && empty == other.empty;
}

bool ResourceSpecialization::operator==(const ResourceSpecialization& other) const {
    return buffers == other.buffers && images == other.images;
}

}
