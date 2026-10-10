#include "Optimization/ResourceMaterializer.hpp"
#include "Optimization/SrtWalker/SrtFlatSlotClasses.hpp"
#include "Optimization/ShaderStageInputInfo.hpp"
#include "RdnaDecoder/RdnaDescriptorFormat.hpp"
#include "RdnaDecoder/RdnaImageOpDecoder.hpp"
#include "SpirvBackend/SpirvBufferFormat.hpp"
#include "ImageTableAbi.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <span>
#include <unordered_map>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace ShaderRecompiler {

namespace {

using ImageTableAbi::PoisonReason;

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

struct ImageDecode {
    DecodedImage decoded;
    std::optional<PoisonReason> reason;
    std::string detail;
};

ImageDecode rejectImage(PoisonReason reason, std::string detail) {
    ImageDecode result;
    result.reason = reason;
    result.detail = std::move(detail);
    return result;
}

PoisonReason invalidImageReason(const DescriptorValue& descriptor) {
    const auto type = rawImageType(descriptor);
    if (type < ImageType::Color1D) {
        return PoisonReason::NotImage;
    }
    if (rawImageFormat(descriptor) == IrBufferFormat::Invalid) {
        return PoisonReason::InvalidFormat;
    }
    if (type == ImageType::Color2DMsaa || type == ImageType::Color2DMsaaArray) {
        return PoisonReason::Multisampled;
    }
    return PoisonReason::Dimension;
}

ImageDecode inspectImageDescriptor(const DescriptorValue& descriptor, const ImageResource& base, std::uint32_t srgbDecodeFormats) {
    ImageDecode result;
    auto& decoded = result.decoded;
    decoded.mipCount = storageMipCount(base, descriptor);
    if (decoded.mipCount == 0u) {
        return rejectImage(PoisonReason::InvalidFormat, "storage image descriptor has an invalid mip range");
    }
    if (nullImageDescriptor(descriptor)) {
        decoded.numericClass = base.atomic ? IrTextureNumericClass::Uint : IrTextureNumericClass::Float;
        decoded.dimension = RdnaImageDimension::Dim2D;
        decoded.cube = false;
        return result;
    }
    if (base.resourceClass == ImageResourceClass::None || (base.atomic && base.resourceClass != ImageResourceClass::Storage)) {
        throw std::runtime_error("image resource has an invalid class");
    }
    if (!validImageDescriptor(descriptor, base.r128)) {
        return rejectImage(invalidImageReason(descriptor), "image descriptor is invalid");
    }
    decoded.dimension = descriptorDimension(descriptor, base.dimension);
    decoded.cube = descriptorIsCube(descriptor);
    const auto format = rawImageFormat(descriptor);
    if (base.atomic64 && format != IrBufferFormat::Format32_32UInt && format != IrBufferFormat::Format32_32SInt && format != IrBufferFormat::Format32_32Float) {
        return rejectImage(PoisonReason::InvalidFormat, "64-bit atomic image descriptor uses an unsupported format " + std::to_string(static_cast<std::uint32_t>(format)));
    }
    if (base.atomic && !base.atomic64 && format != IrBufferFormat::Format32UInt && format != IrBufferFormat::Format32SInt && format != IrBufferFormat::Format32Float) {
        return rejectImage(PoisonReason::InvalidFormat, "atomic image descriptor uses an unsupported format " + std::to_string(static_cast<std::uint32_t>(format)));
    }
    const bool storage = base.resourceClass == ImageResourceClass::Storage;
    const bool table = base.table != NoTable;
    decoded.fmask = IsFmaskTextureFormat(format);
    if (decoded.fmask && !base.fmaskCompatible) return rejectImage(PoisonReason::Fmask, "FMASK requires a direct 32-bit image load");
    if (decoded.fmask && (storage || base.depthCompare || table)) {
        return rejectImage(PoisonReason::Fmask, "FMASK requires a direct sampled image load");
    }
    if (base.packed) {
        if (table || (!storage && descriptorImageSwizzle(descriptor) != ShaderImageIdentitySwizzle)) {
            return rejectImage(PoisonReason::InvalidFormat, "packed image access requires a direct image, with identity swizzle when sampled");
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
        if (!measured || descriptorImageSwizzle(descriptor) != ShaderImageIdentitySwizzle || rawImageType(descriptor) != ImageType::Color2D || table) {
            return rejectImage(PoisonReason::InvalidFormat, base.packed ? "MIMG PCK2/PCK4 requires a direct, identity-swizzled 2D UNORM, UINT or SINT image whose elements fill one dword: R8, R16 or RG8 for PCK2, R8 for PCK4"
                                                                     : "MIMG BY2/BY4 requires a direct, identity-swizzled 2D image whose elements fill one dword: R8, R16 or RG8 for BY2, R8 for BY4");
        }
    }
    decoded.conversionFormat = RemapTextureFormat(format) != format ? format : IrBufferFormat::Invalid;
    if (format == IrBufferFormat::Format11_11_10UNorm || format == IrBufferFormat::Format10_11_11Float) {
        const bool floating = format == IrBufferFormat::Format10_11_11Float;
        if (!base.srgbDecodeCompatible) return rejectImage(PoisonReason::Conversion, floating ? "samples or gathers a converted float image, or queries its LOD, which is not implemented" : "sampling, gathering or querying LOD of a converted unorm image is not implemented");
        if (!base.depthBitsCompatible) return rejectImage(PoisonReason::Conversion, floating ? "reads or writes a converted float image with 16-bit data, which is not implemented" : "reads or writes a converted unorm image with 16-bit data, which is not implemented");
        for (std::uint32_t component = 0; component < 4u; ++component) {
            if (((descriptorImageSwizzle(descriptor) >> (component * 3u)) & 7u) == 7u) return rejectImage(PoisonReason::Conversion, "selects a channel the converted image format does not have");
        }
    }
    decoded.srgbDecode = !storage && (srgbDecodeFormats & SrgbDecodeBit(format)) != 0u;
    if (decoded.srgbDecode && !base.srgbDecodeCompatible) {
        return rejectImage(PoisonReason::SrgbUnsupported, "samples or gathers an sRGB image the device cannot sample, which is not implemented");
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
        if (!base.depthBitsCompatible) return rejectImage(PoisonReason::DepthBits16, "runtime image reads depth bits as unsupported 16-bit results");
        decoded.depthUnorm16 = DepthBitsTextureWidth(descriptor.dwords[1], descriptor.dwords[3]) == 16u;
        decoded.numericClass = IrTextureNumericClass::Float;
        decoded.shaderSwizzle = descriptorImageSwizzle(descriptor);
    }
    if (storage) {
        if ((!rawSintStorage && decoded.numericClass == IrTextureNumericClass::Sint) || decoded.numericClass == IrTextureNumericClass::Unsupported) {
            return rejectImage(PoisonReason::NumericClass, "storage image descriptor uses an unsupported format");
        }
        if (rawSintStorage) {
            decoded.numericClass = IrTextureNumericClass::Uint;
        }
        if ((rawSintStorage || (base.atomic && format == IrBufferFormat::Format32SInt)) && !base.packed) {
            if (!base.depthBitsCompatible) return rejectImage(PoisonReason::InvalidFormat, "stores 16-bit data to an image of a SINT format");
            decoded.conversionFormat = format;
        }
    } else if (decoded.numericClass == IrTextureNumericClass::Unsupported || (base.depthCompare && decoded.numericClass != IrTextureNumericClass::Float)) {
        return rejectImage(PoisonReason::NumericClass, "sampled image descriptor uses an unsupported format");
    }
    return result;
}

DecodedImage decodeImageDescriptor(const DescriptorValue& descriptor, const ImageResource& base, std::uint32_t srgbDecodeFormats) {
    auto result = inspectImageDescriptor(descriptor, base, srgbDecodeFormats);
    if (result.reason.has_value()) {
        throw std::runtime_error(result.detail);
    }
    return result.decoded;
}

bool requiresPointSampler(const ImageResource& mode) {
    return mode.numericClass == IrTextureNumericClass::Uint || mode.numericClass == IrTextureNumericClass::Sint || mode.conversionFormat != IrBufferFormat::Invalid || mode.depthBits;
}

bool reducesBetweenTexels(std::uint32_t word0, std::uint32_t filter) {
    return ((word0 >> 29u) & 3u) != 0u && (((filter >> 20u) & 0xfu) != 0u || ((filter >> 26u) & 3u) == 2u);
}

std::string hexText(std::uint64_t value) {
    char text[24];
    std::snprintf(text, sizeof(text), "%llx", static_cast<unsigned long long>(value));
    return text;
}

std::uint32_t TableKeyScanLimit() {
    static const std::uint32_t limit = [] {
        const char* text = std::getenv("APS5_IMAGE_TABLE_KEY_SCAN");
        return text != nullptr ? static_cast<std::uint32_t>(std::strtoul(text, nullptr, 0)) : 256u;
    }();
    return limit;
}

bool TableStrict() {
    static const bool strict = [] {
        const char* text = std::getenv("APS5_IMAGE_TABLE_STRICT");
        return text != nullptr && std::strcmp(text, "0") != 0;
    }();
    return strict;
}

bool TableTraced() {
    static const bool traced = std::getenv("APS5_TRACE_IMAGE_TABLE") != nullptr;
    return traced;
}

constexpr std::size_t PoisonReasonCount = static_cast<std::size_t>(PoisonReason::Count);

struct TableCounters {
    std::atomic<std::uint64_t> snapshots{0};
    std::atomic<std::uint64_t> tables{0};
    std::atomic<std::uint64_t> keys{0};
    std::array<std::atomic<std::uint64_t>, PoisonReasonCount> poison{};
    std::atomic<std::uint64_t> narrowed{0};
    std::atomic<std::uint64_t> narrowSkipped{0};
    std::atomic<std::uint64_t> nanoseconds{0};
    std::atomic<long long> lastReport{0};
};

TableCounters& tableCounters() {
    static TableCounters counters;
    return counters;
}

void reportTables() {
    if (!MaterializeProfiled()) return;
    auto& counters = tableCounters();
    const auto now = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    auto last = counters.lastReport.load(std::memory_order_relaxed);
    if (last == 0) {
        counters.lastReport.compare_exchange_strong(last, now, std::memory_order_relaxed);
        return;
    }
    if (now - last < 10'000'000'000ll || !counters.lastReport.compare_exchange_strong(last, now, std::memory_order_relaxed)) return;
    const auto snapshots = counters.snapshots.exchange(0, std::memory_order_relaxed);
    if (snapshots == 0) return;
    std::string poison;
    for (std::size_t reason = 0; reason < PoisonReasonCount; reason++) {
        const auto count = counters.poison[reason].exchange(0, std::memory_order_relaxed);
        if (count == 0) continue;
        poison += " " + std::string(ImageTableAbi::PoisonReasonName(static_cast<PoisonReason>(reason))) + "=" + std::to_string(count);
    }
    std::fprintf(stderr, "[image-table] (10 s): %llu snapshots, %llu tables, %llu keys, structural poison:%s, narrowing %llu applied / %llu skipped, %.1f ms\n",
        static_cast<unsigned long long>(snapshots), static_cast<unsigned long long>(counters.tables.exchange(0, std::memory_order_relaxed)), static_cast<unsigned long long>(counters.keys.exchange(0, std::memory_order_relaxed)), poison.empty() ? " none" : poison.c_str(),
        static_cast<unsigned long long>(counters.narrowed.exchange(0, std::memory_order_relaxed)), static_cast<unsigned long long>(counters.narrowSkipped.exchange(0, std::memory_order_relaxed)), static_cast<double>(counters.nanoseconds.exchange(0, std::memory_order_relaxed)) / 1e6);
}

enum class RecordState : std::uint8_t { OutsideDomain, Read, Unmapped };

struct ColumnRecords {
    std::uint64_t base = 0;
    std::uint64_t size = 0;
    std::uint32_t records = 0;
    std::uint32_t keys = 0;
    std::uint32_t fault = 0;
    bool narrowed = false;
    bool outside = false;
    std::vector<RecordState> states;
    std::vector<std::uint32_t> words;
};

struct TableTrace {
    std::uint64_t base = 0;
    std::uint64_t size = 0;
    std::uint32_t records = 0;
    std::uint32_t keys = 0;
    std::uint32_t poison = 0;
    bool narrowed = false;

    bool operator==(const TableTrace& other) const = default;
};

void traceTable(const IrResourcePlan& plan, std::uint32_t table, const TableColumn& column, const TableTrace& trace) {
    static std::mutex mutex;
    static std::map<std::pair<std::uint64_t, std::uint32_t>, TableTrace> seen;
    std::lock_guard lock(mutex);
    auto& last = seen[{plan.shaderHash, table}];
    if (last == trace) return;
    last = trace;
    std::fprintf(stderr, "[image-table] shader 0x%llx table %u (%s): base 0x%llx size 0x%llx stride 0x%x addend 0x%x offset 0x%x dwords %u, %u records, %u keys%s, %u structural poison\n", static_cast<unsigned long long>(plan.shaderHash), table, column.sampler ? "sampler" : "image", static_cast<unsigned long long>(trace.base), static_cast<unsigned long long>(trace.size), column.stride, column.addend, column.offset, column.dwordCount, trace.records, trace.keys, trace.narrowed ? " (narrowed)" : "", trace.poison);
}

class TableSnapshotter {
public:
    TableSnapshotter(const IrResourcePlan& plan, const SrtRuntime& runtime, SrtWalker& walker, const std::vector<std::uint8_t>& activeSources, ImageTableSnapshot& tables) : plan(plan), runtime(runtime), walker(walker), activeSources(activeSources), tables(tables) {}

    void Run() {
        tables = ImageTableSnapshot{};
        std::uint32_t count = 0;
        for (const auto& image : plan.info.images) {
            if (image.table != NoTable) count = std::max(count, image.table + 1u);
        }
        for (const auto& sampler : plan.info.samplers) {
            if (sampler.table != NoTable) count = std::max(count, sampler.table + 1u);
        }
        if (count == 0u) {
            return;
        }
        if (runtime.readMemory == nullptr) {
            throw std::runtime_error("image table snapshot requires runtime memory access");
        }
        const auto started = std::chrono::steady_clock::now();
        tables.shader = plan.shaderHash;
        tables.tables.resize(count);
        for (const auto& image : plan.info.images) {
            if (image.table != NoTable) Snapshot(image.source, image.table);
        }
        for (const auto& sampler : plan.info.samplers) {
            if (sampler.table != NoTable) Snapshot(sampler.source, sampler.table);
        }
        auto& counters = tableCounters();
        counters.snapshots.fetch_add(1, std::memory_order_relaxed);
        counters.tables.fetch_add(count, std::memory_order_relaxed);
        counters.nanoseconds.fetch_add(static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - started).count()), std::memory_order_relaxed);
        if (TableStrict()) {
            for (std::uint32_t table = 0; table < tables.tables.size(); table++) {
                const auto& column = tables.tables[table];
                const auto poisoned = std::find_if(column.codes.begin(), column.codes.end(), [](std::uint32_t code) { return (code & ImageTableAbi::PoisonFlag) != 0u && (code & ~ImageTableAbi::PoisonFlag) != static_cast<std::uint32_t>(PoisonReason::OutsideDomain); });
                if (column.fault != 0u || poisoned != column.codes.end()) {
                    const auto reason = static_cast<PoisonReason>((column.fault != 0u ? column.fault : *poisoned) & ~ImageTableAbi::PoisonFlag);
                    throw std::runtime_error("image table " + std::to_string(table) + " at 0x" + hexText(column.base) + " holds a record that cannot be read (" + std::string(ImageTableAbi::PoisonReasonName(reason)) + "), and APS5_IMAGE_TABLE_STRICT is set");
                }
            }
        }
    }

private:
    static constexpr std::uint64_t PageBytes = 4096u;

    bool Accessible(std::uint64_t address, std::uint64_t bytes) const {
        return runtime.accessible == nullptr || runtime.accessible(runtime.userContext, address, bytes);
    }

    bool PageAccessible(std::uint64_t address, std::uint64_t begin, std::uint64_t end) {
        const auto page = address & ~(PageBytes - 1u);
        if (const auto found = pages.find(page); found != pages.end()) return found->second;
        const auto first = std::max(page, begin);
        const auto last = std::min(page + PageBytes, end);
        const bool accessible = first < last && Accessible(first, last - first);
        pages.emplace(page, accessible);
        return accessible;
    }

    std::uint32_t Read(std::uint64_t address) const {
        std::uint32_t word = 0;
        if (!runtime.readMemory(runtime.userContext, address, &word)) {
            throw std::runtime_error("image table: failed to read guest memory at 0x" + hexText(address));
        }
        return word;
    }

    ShaderBufferResource EvaluateBuffer(std::uint32_t source) const {
        DescriptorValue value;
        walker.EvaluateDescriptorSource(plan, source, runtime, value);
        return decodeBufferDescriptor(value);
    }

    void Narrow(const TableColumn& column, ColumnRecords& records) {
        const auto& domain = *column.keyDomain;
        const auto skip = [&] {
            tableCounters().narrowSkipped.fetch_add(1, std::memory_order_relaxed);
        };
        const auto keyBuffer = EvaluateBuffer(domain.source);
        if (keyBuffer.Type() != 0u) {
            skip();
            return;
        }
        const auto base = keyBuffer.Base48();
        const auto size = keyBuffer.GetSize();
        const std::uint64_t first = domain.offset & ~3u;
        const std::uint64_t positions = size >= first + 4u ? (size - first - 4u) / domain.stride + 1u : 0u;
        if (positions > TableKeyScanLimit() || first > 0xffffffffull) {
            skip();
            return;
        }
        if (positions != 0u && runtime.pendingWrite != nullptr && runtime.pendingWrite(runtime.userContext, base + first, size - first)) {
            skip();
            return;
        }
        std::vector<std::uint32_t> values{0u};
        for (std::uint64_t index = 0; index < positions; index++) {
            const auto address = base + first + index * domain.stride;
            if (!PageAccessible(address, base, base + size)) {
                skip();
                return;
            }
            values.push_back(Read(address));
        }
        records.states.assign(records.keys, RecordState::OutsideDomain);
        records.outside = false;
        for (const auto value : values) {
            const std::uint32_t relative = value * column.stride;
            const bool aligned = relative % column.stride == 0u;
            const auto record = relative / column.stride;
            if (aligned && record < records.keys) {
                records.states[record] = RecordState::Read;
                continue;
            }
            if (ImageTableAbi::ScalarBufferDword(relative + column.addend, column.offset, records.size).has_value()) records.outside = true;
        }
        records.narrowed = true;
        if (positions != 0u) tables.ranges.emplace_back(base, size);
        tableCounters().narrowed.fetch_add(1, std::memory_order_relaxed);
    }

    std::uint32_t WordsIndex(const DescriptorValue& value) {
        const auto [found, inserted] = wordsIndex.try_emplace({value.dwords, value.dwordCount}, static_cast<std::uint32_t>(tables.words.size()));
        if (inserted) tables.words.push_back(value);
        return found->second;
    }

    ColumnRecords& Column(std::uint32_t source) {
        if (const auto found = columns.find(source); found != columns.end()) {
            return found->second;
        }
        auto& records = columns[source];
        const auto& column = *plan.descriptorSources.at(source).tableColumn;
        const bool active = source >= activeSources.size() || activeSources[source] != 0u;
        if (!active) {
            records.fault = ImageTableAbi::PoisonCode(static_cast<std::uint32_t>(PoisonReason::Inactive));
            return records;
        }
        const auto heap = EvaluateBuffer(column.heapSource);
        if (heap.Type() != 0u) {
            records.fault = ImageTableAbi::PoisonCode(static_cast<std::uint32_t>(PoisonReason::NotBuffer));
            return records;
        }
        records.base = heap.Base48();
        records.size = heap.GetSize();
        records.records = ImageTableAbi::ScalarBufferRecords(column.addend, column.stride, column.offset, records.size);
        records.keys = std::min(records.records, ImageTableAbi::MaxKeys);
        records.outside = records.keys < records.records;
        if (records.size != 0u) tables.ranges.emplace_back(records.base, records.size);
        records.states.assign(records.keys, RecordState::Read);
        if (column.keyDomain.has_value() && records.keys != 0u) {
            Narrow(column, records);
        }
        records.words.assign(records.keys, 0u);
        const auto end = records.base + records.size;
        for (std::uint32_t record = 0; record < records.keys; record++) {
            if (records.states[record] != RecordState::Read) continue;
            DescriptorValue value;
            value.dwordCount = column.sampler ? 4u : 8u;
            const auto offset = column.addend + record * column.stride;
            for (std::uint32_t dword = 0; dword < column.dwordCount; dword++) {
                const auto position = ImageTableAbi::ScalarBufferDword(offset, column.offset + dword * 4u, records.size);
                if (!position.has_value()) continue;
                const auto address = records.base + *position;
                if (!PageAccessible(address, records.base, end)) {
                    records.states[record] = RecordState::Unmapped;
                    break;
                }
                value.dwords[dword] = Read(address);
            }
            if (records.states[record] == RecordState::Read) records.words[record] = WordsIndex(value);
        }
        return records;
    }

    void Snapshot(std::uint32_t source, std::uint32_t table) {
        const auto& column = *plan.descriptorSources.at(source).tableColumn;
        const auto& records = Column(source);
        auto& out = tables.tables.at(table);
        out.base = records.base;
        out.size = records.size;
        out.records = records.records;
        out.keys = records.keys;
        out.fault = records.fault;
        out.outside = records.outside;
        out.codes.assign(records.keys, 0u);
        TableTrace trace{records.base, records.size, records.records, records.keys, 0u, records.narrowed};
        auto& counters = tableCounters();
        if (records.fault != 0u) counters.poison[records.fault & ~ImageTableAbi::PoisonFlag].fetch_add(1, std::memory_order_relaxed);
        for (std::uint32_t key = 0; key < records.keys; key++) {
            switch (records.states[key]) {
                case RecordState::Read:
                    out.codes[key] = records.words[key];
                    continue;
                case RecordState::OutsideDomain:
                    out.codes[key] = ImageTableAbi::PoisonCode(static_cast<std::uint32_t>(PoisonReason::OutsideDomain));
                    break;
                case RecordState::Unmapped:
                    out.codes[key] = ImageTableAbi::PoisonCode(static_cast<std::uint32_t>(PoisonReason::Unmapped));
                    counters.poison[static_cast<std::size_t>(PoisonReason::Unmapped)].fetch_add(1, std::memory_order_relaxed);
                    break;
            }
            trace.poison++;
        }
        counters.keys.fetch_add(records.keys, std::memory_order_relaxed);
        if (TableTraced()) traceTable(plan, table, column, trace);
    }

    const IrResourcePlan& plan;
    const SrtRuntime& runtime;
    SrtWalker& walker;
    const std::vector<std::uint8_t>& activeSources;
    ImageTableSnapshot& tables;
    std::map<std::uint32_t, ColumnRecords> columns;
    std::map<std::pair<std::array<std::uint32_t, 8>, std::uint32_t>, std::uint32_t> wordsIndex;
    std::unordered_map<std::uint64_t, bool> pages;
};

void materializeSnapshot(const IrResourcePlan& plan, const SrtRuntime& runtime, SrtWalker& walker, ResourceSnapshot& snapshot, std::vector<std::uint8_t>& activeSources) {
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
    walker.EvaluateRuntimeSources(plan, plan.materializationSources, runtime, values, snapshot.flattenedSrt, plan.cleanFlatSlots, activeSources, &snapshot.srtPoison);

    std::size_t cursor = 0;
    if (values.size() < plan.info.buffers.size()) {
        throw std::runtime_error("materialization sources are missing buffer descriptors");
    }
    snapshot.buffers.assign(values.begin(), values.begin() + plan.info.buffers.size());
    cursor += plan.info.buffers.size();

    snapshot.images.resize(plan.info.images.size());
    for (std::uint32_t i = 0; i < plan.info.images.size(); i++) {
        const auto& image = plan.info.images[i];
        if (image.source >= plan.descriptorSources.size()) {
            throw std::runtime_error("image resource references an unknown descriptor source");
        }
        if (image.table != NoTable) {
            snapshot.images[i].dwordCount = 8u;
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

    snapshot.samplers.resize(plan.info.samplers.size());
    for (std::uint32_t i = 0; i < plan.info.samplers.size(); i++) {
        if (plan.info.samplers[i].table != NoTable) {
            snapshot.samplers[i].dwordCount = 4u;
            continue;
        }
        if (cursor >= values.size()) {
            throw std::runtime_error("materialization sources are missing sampler descriptors");
        }
        snapshot.samplers[i] = values[cursor];
        cursor++;
    }
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
    if (image.table != NoTable || (image.emulatedCompare & EmulatedCompare::Unsupported) != 0u) throw std::runtime_error("unsupported color comparison image instructions");
    if ((image.emulatedCompare & EmulatedCompare::RequiresSingleLevel) != 0u && ((descriptor.dwords[3] >> 12u) & 0xfu) != ((descriptor.dwords[3] >> 16u) & 0xfu)) throw std::runtime_error("color comparison requires a single mip level");
    const auto reference = colorCompareReference(format);
    const auto type = rawImageType(descriptor);
    if (type != ImageType::Color2D && type != ImageType::Color2DArray && type != ImageType::Cube) throw std::runtime_error("comparison sampling of a color texture is implemented only for 2D, 2D array and cube views");
    if ((descriptorImageSwizzle(descriptor) & 0x7u) != 4u) throw std::runtime_error("comparison sampling of a color texture is implemented only when the view's X channel is red");
    std::optional<std::uint32_t> samplerState;
    for (const auto& pair : info.sampledPairs) {
        if (pair.image != index) continue;
        if (pair.sampler >= info.samplers.size() || info.samplers[pair.sampler].table != NoTable) throw std::runtime_error("comparison sampling of a color texture through a sampler table is not implemented");
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

void materializeTables(const IrResourcePlan& plan, ResourceSnapshot& snapshot) {
    for (std::uint32_t i = 0u; i < plan.info.buffers.size(); ++i) {
        const auto decoded = decodeBufferDescriptor(snapshot.buffers.at(i));
        if (decoded.Type() != 0u) throw std::runtime_error("buffer descriptor uses an unsupported type");
        if (plan.stage != IrShaderStage::Compute && decoded.AddTid()) throw std::runtime_error("buffer ADD_TID is only valid for compute shaders");
    }
    if (snapshot.flattenedSrt.size() != plan.srtReads.size()) throw std::runtime_error("runtime SRT size differs from the static interface");
    for (std::uint32_t i = 0u; i < plan.info.images.size(); ++i) {
        const auto& image = plan.info.images[i];
        if (image.table != NoTable) continue;
        static_cast<void>(ResourceMaterializer::RuntimeImageMode(image, snapshot.images.at(i), plan.info.runtimeImageModes.at(i)));
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

struct DecodeMemoKey {
    std::uint64_t artifact = 0;
    std::uint32_t resource = 0;
    std::array<std::uint32_t, 8> words{};

    bool operator==(const DecodeMemoKey& other) const = default;
};

struct DecodeMemoHash {
    std::size_t operator()(const DecodeMemoKey& key) const {
        std::uint64_t hash = 0xcbf29ce484222325ull;
        const auto mix = [&](std::uint64_t value) {
            hash ^= value;
            hash *= 0x100000001b3ull;
        };
        mix(key.artifact);
        mix(key.resource);
        for (const auto word : key.words) mix(word);
        return static_cast<std::size_t>(hash);
    }
};

struct DecodeMemo {
    std::mutex mutex;
    std::unordered_map<DecodeMemoKey, std::uint32_t, DecodeMemoHash> entries;
};

DecodeMemo& decodeMemo() {
    static DecodeMemo memo;
    return memo;
}

constexpr std::uint32_t DecodeMemoEntries = 1u << 20u;
constexpr std::uint32_t DecodeFailed = 0x80000000u;

std::uint32_t decodeTableEntry(const ImageResource& image, std::span<const ImageResource> modes, const DescriptorValue& words, std::uint64_t memoKey, std::uint32_t resource) {
    DecodeMemoKey key{memoKey, resource, words.dwords};
    if (memoKey != 0u) {
        auto& memo = decodeMemo();
        std::lock_guard lock(memo.mutex);
        if (const auto found = memo.entries.find(key); found != memo.entries.end()) return found->second;
    }
    const auto match = ResourceMaterializer::TryRuntimeImageMode(image, words, modes);
    const auto value = match.mode.has_value() ? *match.mode : DecodeFailed | static_cast<std::uint32_t>(match.reason);
    if (memoKey != 0u) {
        auto& memo = decodeMemo();
        std::lock_guard lock(memo.mutex);
        if (memo.entries.size() >= DecodeMemoEntries) memo.entries.clear();
        memo.entries.emplace(key, value);
    }
    return value;
}

}

ResolvedImageTables ResourceMaterializer::ResolveImageTables(const ShaderInfo& info, const ResourceSnapshot& snapshot, std::uint64_t memoKey) {
    namespace Abi = ImageTableAbi;
    ResolvedImageTables out;
    std::uint32_t count = 0;
    for (const auto& image : info.images) {
        if (image.table != NoTable) count = std::max(count, image.table + 1u);
    }
    for (const auto& sampler : info.samplers) {
        if (sampler.table != NoTable) count = std::max(count, sampler.table + 1u);
    }
    if (count == 0u) return out;
    const auto& tables = snapshot.tables;
    if (tables.tables.size() != count) throw std::runtime_error("image table snapshot disagrees with the static tables");
    if (info.runtimeImageModes.size() != info.images.size()) throw std::runtime_error("prepared runtime image modes are missing");
    out.map.assign(Abi::HeaderWords + Abi::TableWords * count, 0u);
    out.map[Abi::HeaderVersion] = Abi::Version;
    out.poison.push_back({{}, 0u, NoTable, PoisonReason::OutsideSnapshot});
    out.poison.push_back({{}, 0u, NoTable, PoisonReason::OutsideDomain});
    std::map<std::tuple<std::uint32_t, PoisonReason, std::array<std::uint32_t, 8>, std::uint32_t>, std::uint32_t> poisonIndex;
    std::set<std::uint32_t> expected;
    const auto poison = [&](std::uint32_t resource, PoisonReason reason, const DescriptorValue* words) {
        const auto value = words != nullptr ? *words : DescriptorValue{};
        const auto [found, inserted] = poisonIndex.try_emplace({resource, reason, value.dwords, value.dwordCount}, static_cast<std::uint32_t>(out.poison.size()));
        if (inserted) out.poison.push_back({value.dwords, value.dwordCount, resource, reason});
        expected.insert(found->second);
        return Abi::PoisonCode(found->second);
    };
    std::map<std::tuple<std::array<std::uint32_t, 8>, RdnaImageDimension, bool, DescriptorBindingKind>, std::uint32_t> elementIndex;
    std::vector<std::uint32_t> directSamplers(info.images.size(), 0u);
    std::vector<bool> reducingSampler(info.images.size(), false);
    for (const auto& pair : info.sampledPairs) {
        const auto& sampler = info.samplers.at(pair.sampler);
        if (sampler.table != NoTable) continue;
        directSamplers.at(pair.image) |= 1u << pair.sampler;
        const auto& words = snapshot.samplers.at(pair.sampler).dwords;
        if (reducesBetweenTexels(words[0], words[2])) reducingSampler.at(pair.image) = true;
    }
    const auto imageRecord = [&](std::uint32_t resource, const DescriptorValue& words) {
        const auto& image = info.images[resource];
        const auto& modes = info.runtimeImageModes[resource];
        std::array<std::uint32_t, Abi::ImageRecordWords> record{0u, words.dwords[3], 0u};
        const auto decoded = decodeTableEntry(image, modes, words, memoKey, resource);
        if ((decoded & DecodeFailed) != 0u) {
            record[Abi::ImageRecordFlags] = poison(resource, static_cast<PoisonReason>(decoded & ~DecodeFailed), &words);
        } else {
            const auto& mode = modes.at(decoded);
            if (mode.dimension == RdnaImageDimension::Dim2DMsaa || mode.dimension == RdnaImageDimension::Dim2DMsaaArray) {
                record[Abi::ImageRecordFlags] = poison(resource, PoisonReason::Multisampled, &words);
            } else if (requiresPointSampler(mode) && reducingSampler[resource]) {
                record[Abi::ImageRecordFlags] = poison(resource, PoisonReason::Reduction, &words);
            } else {
                auto element = words.dwords;
                if (mode.conversionFormat != IrBufferFormat::Invalid || mode.depthBits) element[3] = (element[3] & ~0xfffu) | ShaderImageIdentitySwizzle;
                const auto kind = DescriptorBindingForImage(mode);
                const auto [found, inserted] = elementIndex.try_emplace({element, mode.dimension, mode.depthCompare, kind}, static_cast<std::uint32_t>(out.elements.size()));
                if (inserted && out.elements.size() >= Abi::ImageElementBudget) {
                    elementIndex.erase(found);
                    record[Abi::ImageRecordFlags] = poison(resource, PoisonReason::Budget, &words);
                } else {
                    if (inserted) out.elements.push_back({element, mode.dimension, mode.depthCompare, kind, 0u});
                    out.elements[found->second].samplers |= directSamplers[resource];
                    record[Abi::ImageRecordFlags] = decoded << Abi::ModeShift;
                    record[Abi::ImageRecordElement] = found->second;
                }
            }
        }
        const auto offset = static_cast<std::uint32_t>(out.map.size());
        out.map.insert(out.map.end(), record.begin(), record.end());
        return offset;
    };
    const auto samplerRecord = [&](std::uint32_t resource, const DescriptorValue& words) {
        std::array<std::uint32_t, Abi::SamplerRecordWords> record{0u, 0u};
        if ((words.dwords[0] & (1u << 15u)) != 0u) {
            record[Abi::SamplerRecordFlags] = poison(resource, PoisonReason::Unnormalized, &words);
        } else if (((words.dwords[0] >> 29u) & 3u) != 0u) {
            record[Abi::SamplerRecordFlags] = poison(resource, PoisonReason::Reduction, &words);
        } else if (out.samplers.size() + Abi::SamplerEntryElements > Abi::SamplerElementBudget) {
            record[Abi::SamplerRecordFlags] = poison(resource, PoisonReason::Budget, &words);
        } else {
            record[Abi::SamplerRecordElement] = static_cast<std::uint32_t>(out.samplers.size());
            for (std::uint32_t variant = 0; variant < 2u; variant++) {
                const bool compare = variant == 1u;
                for (std::uint32_t point = 0; point < 2u; point++) {
                    ImageTableSamplerElement element;
                    std::copy_n(words.dwords.begin(), 4u, element.words.begin());
                    element.compare = compare;
                    element.point = point != 0u;
                    out.samplers.push_back(element);
                }
            }
        }
        const auto offset = static_cast<std::uint32_t>(out.map.size());
        out.map.insert(out.map.end(), record.begin(), record.end());
        return offset;
    };
    std::vector<bool> outside(count, false);
    const auto fill = [&](std::uint32_t resource, std::uint32_t table, bool sampler) {
        const auto& column = tables.tables.at(table);
        const auto header = Abi::TableHeader(table);
        out.map[header + Abi::TableKeyCount] = column.keys;
        out.map[header + Abi::TableSizeLow] = static_cast<std::uint32_t>(column.size);
        out.map[header + Abi::TableSizeHigh] = static_cast<std::uint32_t>(column.size >> 32u);
        out.map[header + Abi::TableBaseLow] = static_cast<std::uint32_t>(column.base);
        out.map[header + Abi::TableBaseHigh] = static_cast<std::uint32_t>(column.base >> 32u);
        if (column.codes.size() != column.keys) throw std::runtime_error("image table snapshot has a truncated map");
        if (column.fault != 0u) {
            out.map[header + Abi::TableFault] = poison(resource, static_cast<PoisonReason>(column.fault & ~Abi::PoisonFlag), nullptr);
        } else if (!sampler && (info.images[resource].tableOperation == TableOperation::Unsupported || info.runtimeImageModes[resource].empty())) {
            out.map[header + Abi::TableFault] = poison(resource, PoisonReason::UnsupportedOperation, nullptr);
        }
        outside[table] = column.outside;
        const auto mapStart = static_cast<std::uint32_t>(out.map.size());
        out.map[header + Abi::TableMapStart] = mapStart;
        out.map.resize(out.map.size() + column.keys, 0u);
        std::unordered_map<std::uint32_t, std::uint32_t> records;
        for (std::uint32_t key = 0; key < column.keys; key++) {
            const auto code = column.codes[key];
            std::uint32_t mapped = Abi::NullCode;
            if ((code & Abi::PoisonFlag) != 0u) {
                const auto reason = static_cast<PoisonReason>(code & ~Abi::PoisonFlag);
                mapped = reason == PoisonReason::OutsideDomain ? Abi::PoisonCode(Abi::OutsideDomainPoison) : poison(resource, reason, nullptr);
            } else if (out.map[header + Abi::TableFault] == 0u) {
                const auto& words = tables.words.at(code);
                if (sampler || !nullImageDescriptor(words)) {
                    const auto [found, inserted] = records.try_emplace(code, 0u);
                    if (inserted) found->second = sampler ? samplerRecord(resource, words) : imageRecord(resource, words);
                    mapped = found->second;
                }
            }
            out.map[mapStart + key] = mapped;
        }
        if (sampler && out.map[header + Abi::TableFault] == 0u) {
            DescriptorValue zero;
            zero.dwordCount = 4u;
            out.map[header + Abi::TableOutsideCode] = samplerRecord(resource, zero);
        }
    };
    for (std::uint32_t resource = 0; resource < info.images.size(); resource++) {
        if (info.images[resource].table != NoTable) fill(resource, info.images[resource].table, false);
    }
    for (std::uint32_t resource = 0; resource < info.samplers.size(); resource++) {
        if (info.samplers[resource].table != NoTable) fill(resource, info.samplers[resource].table, true);
    }
    out.map[Abi::HeaderElementCount] = static_cast<std::uint32_t>(out.elements.size());
    out.map[Abi::HeaderSamplerCount] = static_cast<std::uint32_t>(out.samplers.size());
    out.map[Abi::HeaderPoisonCount] = static_cast<std::uint32_t>(out.poison.size());
    out.map[Abi::HeaderTableCount] = count;
    out.map[Abi::HeaderElementFaults] = static_cast<std::uint32_t>(out.map.size());
    out.map.resize(out.map.size() + out.elements.size(), 0u);
    out.map[Abi::HeaderSamplerFaults] = static_cast<std::uint32_t>(out.map.size());
    out.map.resize(out.map.size() + out.samplers.size() / Abi::SamplerEntryElements, 0u);
    out.faults = static_cast<std::uint32_t>(expected.size() + static_cast<std::size_t>(std::count(outside.begin(), outside.end(), true)));
    return out;
}

std::vector<ImageResource> ResourceMaterializer::RuntimeImageModes(const ImageResource& image) {
    const bool table = image.table != NoTable;
    if (table && image.tableOperation == TableOperation::Unsupported) return {};
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
                if (image.fmaskCompatible && !table) append(IrTextureNumericClass::Float, IrBufferFormat::Invalid, IrBufferFormat::Fmask8_S2_F1, false, false);
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
    if (image.depthCompare && !table && (image.emulatedCompare & EmulatedCompare::Unsupported) == 0u && (image.dimension == RdnaImageDimension::Dim2D || image.dimension == RdnaImageDimension::Dim2DArray)) {
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
    if (image.constantSwizzleCompatible && !storage && !image.depthCompare && !image.packed && !table) {
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
    if (modes.empty() && !table) throw std::runtime_error("image instruction has no supported runtime modes");
    return modes;
}

RuntimeImageModeMatch ResourceMaterializer::TryRuntimeImageMode(const ImageResource& image, const DescriptorValue& descriptor, std::span<const ImageResource> modes) {
    RuntimeImageModeMatch result;
    const auto reject = [&](PoisonReason reason, std::string detail) {
        result.reason = reason;
        result.detail = std::move(detail);
        return result;
    };
    if (descriptor.dwordCount != 8u) throw std::runtime_error("runtime image descriptor must contain eight dwords");
    if (modes.empty()) throw std::runtime_error("prepared runtime image modes are missing");
    if (nullImageDescriptor(descriptor)) {
        result.mode = 0u;
        return result;
    }
    const auto inspected = inspectImageDescriptor(descriptor, image, image.srgbDecodeFormats);
    if (inspected.reason.has_value()) return reject(*inspected.reason, inspected.detail);
    const auto& decoded = inspected.decoded;
    const auto format = rawImageFormat(descriptor);
    const bool emulated = image.depthCompare && format != IrBufferFormat::Format32Float && format != IrBufferFormat::Format16UNorm && !IsDepthBitsTexture(descriptor.dwords[1], descriptor.dwords[3]);
    if (!emulated && (image.emulatedCompare & EmulatedCompare::NativeOffsetUnsupported) != 0u) return reject(PoisonReason::UnsupportedOperation, "native comparison with a nonconstant texel offset requires VK_KHR_maintenance8 and shaderImageGatherExtended");
    if (image.packed && decoded.packedFormat != IrBufferFormat::Invalid) {
        const auto packedFormat = GetFormatInfo(decoded.packedFormat);
        if (packedFormat.packedBitfield) return reject(PoisonReason::InvalidFormat, "runtime packed image accesses a bitfield format");
        const bool storage = image.resourceClass == ImageResourceClass::Storage;
        if (storage && packedFormat.byteSize == 12u) return reject(PoisonReason::InvalidFormat, "runtime packed image uses a format the hardware does not write");
        for (std::uint32_t component = 0u; component < packedFormat.componentCount; ++component) {
            const auto bits = packedFormat.componentBits[component];
            const bool exact = storage ? packedFormat.type == SpirvFormatComponentType::Uint || (bits == 32u && (packedFormat.type == SpirvFormatComponentType::Sint || packedFormat.type == SpirvFormatComponentType::Float)) : packedFormat.type == SpirvFormatComponentType::Uint || packedFormat.type == SpirvFormatComponentType::Sint || (packedFormat.type == SpirvFormatComponentType::Unorm && bits <= 16u) || (packedFormat.type == SpirvFormatComponentType::Float && bits == 32u);
            if (!exact) return reject(PoisonReason::InvalidFormat, storage ? "runtime packed image bits are not reproducible through the view" : "runtime packed image bits are not recoverable from the view");
        }
    }
    if (decoded.mipCount > (image.mipMode == ImageMipMode::DynamicStorage ? RuntimeAbi::StorageMipSlots : 1u)) return reject(PoisonReason::InvalidFormat, "runtime storage image mip capacity exceeded");
    if ((descriptorImageSwizzle(descriptor) & 06666u) == 0u && !decoded.fmask && !decoded.depthBits && decoded.conversionFormat == IrBufferFormat::Invalid && !decoded.srgbDecode) {
        for (std::uint32_t index = 0u; index < modes.size(); ++index) {
            if (modes[index].constantSwizzle && modes[index].numericClass == decoded.numericClass) {
                result.mode = index;
                return result;
            }
        }
    }
    for (std::uint32_t index = 0u; index < modes.size(); ++index) {
        const auto& mode = modes[index];
        if (mode.constantSwizzle) continue;
        if (((mode.emulatedCompare & EmulatedCompare::Enabled) != 0u) != emulated) continue;
        if (decoded.fmask) {
            if (mode.packedFormat == IrBufferFormat::Fmask8_S2_F1) {
                result.mode = index;
                return result;
            }
            continue;
        }
        if (mode.numericClass == decoded.numericClass && mode.dimension == decoded.dimension && mode.conversionFormat == decoded.conversionFormat && mode.packedFormat == decoded.packedFormat && mode.cube == decoded.cube && mode.depthBits == decoded.depthBits && mode.depthUnorm16 == decoded.depthUnorm16 && mode.srgbDecode == decoded.srgbDecode) {
            result.mode = index;
            return result;
        }
    }
    if (image.dimension == RdnaImageDimension::Dim1D && decoded.dimension != RdnaImageDimension::Dim1D) return reject(PoisonReason::Dimension, "image address has too few coordinate components");
    const auto reason = emulated ? PoisonReason::ColorCompare
        : decoded.fmask ? PoisonReason::Fmask
        : decoded.depthBits ? PoisonReason::DepthBits16
        : decoded.srgbDecode ? PoisonReason::SrgbUnsupported
        : decoded.conversionFormat != IrBufferFormat::Invalid ? PoisonReason::Conversion
        : decoded.dimension == RdnaImageDimension::Dim2DMsaa || decoded.dimension == RdnaImageDimension::Dim2DMsaaArray ? PoisonReason::Multisampled
        : std::none_of(modes.begin(), modes.end(), [&](const ImageResource& mode) { return mode.dimension == decoded.dimension && mode.cube == decoded.cube; }) ? PoisonReason::Dimension
        : PoisonReason::NumericClass;
    return reject(reason, "image descriptor is incompatible with the static runtime image interface");
}

std::uint32_t ResourceMaterializer::RuntimeImageMode(const ImageResource& image, const DescriptorValue& descriptor, std::span<const ImageResource> modes) {
    const auto match = TryRuntimeImageMode(image, descriptor, modes);
    if (!match.mode.has_value()) throw std::runtime_error(match.detail);
    return *match.mode;
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
    bool tables = false;
    for (auto& image : images) {
        image.srgbDecodeFormats = resources.srgbDecodeFormats;
        image.numericClass = image.atomic ? IrTextureNumericClass::Uint : IrTextureNumericClass::Float;
        image.mipCount = image.mipMode == ImageMipMode::DynamicStorage ? RuntimeAbi::StorageMipSlots : 1u;
        if (image.table == NoTable) continue;
        tables = true;
        const auto modes = RuntimeImageModes(image);
        if (modes.empty() || modes.size() > ImageTableAbi::MaxModes) image.tableOperation = TableOperation::Unsupported;
    }
    for (const auto& pair : resources.info.sampledPairs) {
        if (pair.image >= images.size() || pair.sampler >= resources.info.samplers.size()) throw std::runtime_error("static sampled pair is out of range");
        auto& sampler = resources.info.samplers[pair.sampler];
        sampler.depthCompare = sampler.depthCompare || images[pair.image].depthCompare;
        tables = tables || sampler.table != NoTable;
    }
    resources.info.images = std::move(images);
    resources.guardedSrtSlots = Detail::ComputeGuardedFlatSlots(resources);
    resources.srtGuardOffset = static_cast<std::uint32_t>(resources.srtReads.size());
    if (tables || !resources.guardedSrtSlots.empty()) resources.info.usesFaultBuffer = true;
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
        if (image.table != NoTable) {
            plan.requiresSpecializationMemory = true;
        } else {
            addSource(image.source);
        }
    }
    for (const auto& sampler : plan.info.samplers) {
        if (sampler.table != NoTable) {
            plan.requiresSpecializationMemory = true;
        } else {
            addSource(sampler.source);
        }
    }
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
        throw std::runtime_error("ResourceMaterializer::Materialize requires runtime memory access for image tables");
    }
    SrtWalker walker;
    ResourceSnapshot nextSnapshot;
    std::vector<std::uint8_t> activeSources;
    try {
        materializeSnapshot(plan, runtime, walker, nextSnapshot, activeSources);
        TableSnapshotter(plan, runtime, walker, activeSources, nextSnapshot.tables).Run();
    } catch (...) {
        reportTables();
        throw;
    }
    const auto started = MaterializeProfiled() ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
    materializeTables(plan, nextSnapshot);
    materializeSrtGuards(plan, nextSnapshot);
    if (MaterializeProfiled()) specializationNanoseconds.fetch_add(static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - started).count()), std::memory_order_relaxed);
    snapshot = std::move(nextSnapshot);
    reportTables();
}

std::uint64_t ResourceMaterializer::SpecializationNanoseconds() {
    return specializationNanoseconds.load(std::memory_order_relaxed);
}

}
