#include "Optimization/ResourceMaterializer.hpp"
#include "Optimization/SrtWalker/SrtFlatSlotClasses.hpp"
#include "Optimization/SrtWalker/SrtEvaluator.hpp"
#include "Optimization/ShaderStageInputInfo.hpp"
#include "RdnaDecoder/RdnaDescriptorFormat.hpp"
#include "RdnaDecoder/RdnaImageOpDecoder.hpp"
#include "RdnaDecoder/RdnaInstructionDecoder.hpp"
#include "SpirvBackend/SpirvBufferFormat.hpp"
#include "ThreadOwned.hpp"
#include "ImageTableAbi.hpp"
#include "prx/libc/include/HostThreadLocal.hpp"
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
#include <memory>
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

using ImageTableAbi::PoisonReason;

std::atomic<std::uint64_t> specializationNanoseconds{0};
std::atomic<std::uint64_t> evaluateNanoseconds{0};
std::atomic<std::uint64_t> materializeNanoseconds{0};

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

bool nullImageDescriptor(const ImageResource& image, const DescriptorValue& descriptor) {
    if (nullImageDescriptor(descriptor)) return true;
    return static_cast<std::uint32_t>(rawImageType(descriptor)) <= 1u && image.table == NoTable && image.read && !image.written && !image.atomic;
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
            return wantArray || requested == RdnaImageDimension::Dim3D ? RdnaImageDimension::Dim2DArray : RdnaImageDimension::Dim2D;
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

std::uint32_t imageCoordinates(RdnaImageDimension dimension) {
    switch (dimension) {
        case RdnaImageDimension::Dim1D: return 1u;
        case RdnaImageDimension::Dim1DArray: case RdnaImageDimension::Dim2D: return 2u;
        case RdnaImageDimension::Dim2DMsaaArray: return 4u;
        default: return 3u;
    }
}

bool multisamplePlaneRead(const ImageResource& image) {
    return image.dimension == RdnaImageDimension::Dim2D && image.resourceClass == ImageResourceClass::Sampled && image.table == NoTable && !image.depthCompare && !image.r128 && image.cubeFaceCompatible;
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
    if (base.mipMode != ImageMipMode::DynamicStorage || nullImageDescriptor(base, descriptor)) {
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
    if (nullImageDescriptor(base, descriptor)) {
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
    if (base.depthCompare) {
        if (decoded.dimension == RdnaImageDimension::Dim1D) {
            decoded.dimension = RdnaImageDimension::Dim2D;
        } else if (decoded.dimension == RdnaImageDimension::Dim1DArray) {
            decoded.dimension = RdnaImageDimension::Dim2DArray;
        }
    }
    if (GcnGuestShaders() && imageCoordinates(decoded.dimension) != imageCoordinates(base.dimension)) {
        throw std::runtime_error("GCN image instruction on a descriptor of type " + std::to_string(static_cast<std::uint32_t>(rawImageType(descriptor))) + " needs a different address count than its inferred dimension; not implemented");
    }
    decoded.cube = descriptorIsCube(descriptor);
    const auto format = rawImageFormat(descriptor);
    if (decoded.dimension == RdnaImageDimension::Dim2DMsaa && rawImageType(descriptor) == ImageType::Color2DMsaa && multisamplePlaneRead(base) && !IsFmaskTextureFormat(format) && !IsDepthBitsTexture(descriptor.dwords[1], descriptor.dwords[3])) decoded.dimension = RdnaImageDimension::Dim2D;
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
    if (decoded.fmask && !FmaskLayoutOf(format).has_value()) return rejectImage(PoisonReason::Fmask, "FMASK format is not implemented");
    if (decoded.fmask && ((descriptor.dwords[6] & (1u << 20u)) != 0u || ((descriptor.dwords[6] & (1u << 21u)) != 0u && (descriptor.dwords[6] >> 24u) == 0u && descriptor.dwords[7] == 0u))) return rejectImage(PoisonReason::Fmask, "FMASK CMASK compression is not implemented without readable metadata");
    if (decoded.fmask && (descriptor.dwords[6] & (1u << 21u)) != 0u && format != IrBufferFormat::Fmask32_S8_F8) return rejectImage(PoisonReason::Fmask, "FMASK CMASK compression is implemented only for 8-sample, 8-fragment FMASK");
    if (decoded.fmask && (base.packed || base.byElements != 0u)) return rejectImage(PoisonReason::Fmask, "packed FMASK image reads are not implemented");
    for (std::uint32_t component = 0; decoded.fmask && component < 4u; ++component) {
        const auto selector = (descriptorImageSwizzle(descriptor) >> (component * 3u)) & 7u;
        if (selector != 0u && selector != 1u && selector != 4u) return rejectImage(PoisonReason::Fmask, "FMASK destination channel selector is not implemented");
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
    const bool nativeRepacked = format == IrBufferFormat::Format10_11_11Float && !storage && !base.packed;
    const auto remappedFormat = storage ? RemapStorageTextureFormat(format) : RemapTextureFormat(format);
    decoded.conversionFormat = remappedFormat != format && !nativeRepacked ? format : IrBufferFormat::Invalid;
    if (format == IrBufferFormat::Format11_11_10UNorm || (format == IrBufferFormat::Format10_11_11Float && !nativeRepacked)) {
        const bool floating = format == IrBufferFormat::Format10_11_11Float;
        if (!base.srgbDecodeCompatible) {
            if (floating) return rejectImage(PoisonReason::Conversion, "samples or gathers a converted float image, or queries its LOD, which is not implemented");
            if (storage || base.depthCompare || (base.emulatedCompare & EmulatedCompare::ConvertedSampleUnsupported) != 0u) return rejectImage(PoisonReason::Conversion, "sampling, gathering or querying LOD of a converted unorm image is not implemented");
            if (base.table != NoTable) return rejectImage(PoisonReason::Conversion, "samples a converted unorm image selected through an image table, which is not implemented");
            if (decoded.cube || (decoded.dimension != RdnaImageDimension::Dim2D && decoded.dimension != RdnaImageDimension::Dim2DArray)) return rejectImage(PoisonReason::Conversion, "samples a converted unorm image that is not a 2D or 2D array view, which is not implemented");
            if ((base.emulatedCompare & EmulatedCompare::ConvertedSampleRequiresSingleLevel) != 0u && ((descriptor.dwords[3] >> 12u) & 0xfu) != ((descriptor.dwords[3] >> 16u) & 0xfu)) return rejectImage(PoisonReason::Conversion, "samples a converted unorm image with several mip levels at a LOD that is not 0, which is not implemented");
        }
        if (!base.depthBitsCompatible) return rejectImage(PoisonReason::Conversion, floating ? "reads or writes a converted float image with 16-bit data, which is not implemented" : "reads or writes a converted unorm image with 16-bit data, which is not implemented");
        for (std::uint32_t component = 0; component < 4u; ++component) {
            if (((descriptorImageSwizzle(descriptor) >> (component * 3u)) & 7u) == 7u) return rejectImage(PoisonReason::Conversion, "selects a channel the converted image format does not have");
        }
    }
    if (decoded.fmask && format != IrBufferFormat::Fmask32_S8_F8) decoded.conversionFormat = FmaskExpansion;
    decoded.srgbDecode = !storage && !base.depthCompare && (srgbDecodeFormats & SrgbDecodeBit(format)) != 0u;
    if (decoded.srgbDecode && !base.srgbDecodeCompatible) {
        return rejectImage(PoisonReason::SrgbUnsupported, "samples or gathers an sRGB image the device cannot sample, which is not implemented");
    }
    if (storage || decoded.conversionFormat != IrBufferFormat::Invalid) {
        decoded.shaderSwizzle = descriptorImageSwizzle(descriptor);
    }
    const bool wideSint = format == IrBufferFormat::Format32SInt || format == IrBufferFormat::Format32_32SInt || format == IrBufferFormat::Format32_32_32_32SInt;
    const bool narrowSint = format == IrBufferFormat::Format16SInt || format == IrBufferFormat::Format8_8SInt || format == IrBufferFormat::Format16_16SInt || format == IrBufferFormat::Format8_8_8_8SInt || format == IrBufferFormat::Format16_16_16_16SInt;
    const bool rawSintStorage = storage && (wideSint || (narrowSint && !base.packed)) && base.written && !base.read && !base.atomic;
    decoded.numericClass = base.atomic || decoded.fmask ? IrTextureNumericClass::Uint : nativeRepacked ? IrTextureNumericClass::Float : SampledTextureNumericClass(remappedFormat);
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
            decoded.conversionFormat = format;
        }
    } else if (decoded.numericClass == IrTextureNumericClass::Unsupported || (base.depthCompare && decoded.numericClass != IrTextureNumericClass::Float)) {
        char words[96];
        std::snprintf(words, sizeof(words), " (T# %08x %08x %08x %08x %08x %08x %08x %08x)", descriptor.dwords[0], descriptor.dwords[1], descriptor.dwords[2], descriptor.dwords[3], descriptor.dwords[4], descriptor.dwords[5], descriptor.dwords[6], descriptor.dwords[7]);
        return rejectImage(PoisonReason::NumericClass, std::string("sampled image descriptor uses an unsupported format") + words);
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

enum class NarrowSkip : std::size_t { Value, NotBuffer, KeyScan, Pending, Unmapped, Count };
constexpr std::array<const char*, static_cast<std::size_t>(NarrowSkip::Count)> NarrowSkipNames{"value", "not-buffer", "key-scan", "pending-write", "unmapped"};

struct TableCounters {
    std::atomic<std::uint64_t> snapshots{0};
    std::atomic<std::uint64_t> tables{0};
    std::atomic<std::uint64_t> keys{0};
    std::array<std::atomic<std::uint64_t>, PoisonReasonCount> poison{};
    std::atomic<std::uint64_t> narrowed{0};
    std::atomic<std::uint64_t> narrowSkipped{0};
    std::array<std::atomic<std::uint64_t>, static_cast<std::size_t>(NarrowSkip::Count)> narrowSkips{};
    std::atomic<std::uint64_t> narrowPositions{0};
    std::atomic<std::uint64_t> selected{0};
    std::atomic<std::uint64_t> reusedUnwritten{0};
    std::atomic<std::uint64_t> reusedCompared{0};
    std::atomic<std::uint64_t> nanoseconds{0};
    std::atomic<long long> lastReport{0};
};

TableCounters& tableCounters() {
    static TableCounters counters;
    return counters;
}

struct TableReuseTotals {
    std::atomic<std::uint64_t> unwritten{0};
    std::atomic<std::uint64_t> compared{0};
    std::atomic<std::uint64_t> wordsRead{0};
    std::atomic<std::uint64_t> shared{0};
    std::atomic<std::uint64_t> columns{0};
    std::atomic<std::uint64_t> resolutions{0};
    std::atomic<std::uint64_t> resolutionsShared{0};
};

TableReuseTotals& tableReuseTotals() {
    static TableReuseTotals totals;
    return totals;
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
    std::string skips;
    for (std::size_t reason = 0; reason < counters.narrowSkips.size(); reason++) {
        const auto count = counters.narrowSkips[reason].exchange(0, std::memory_order_relaxed);
        if (count == 0) continue;
        skips += " " + std::string(NarrowSkipNames[reason]) + "=" + std::to_string(count);
        if (reason == static_cast<std::size_t>(NarrowSkip::KeyScan)) skips += " (up to " + std::to_string(counters.narrowPositions.exchange(0, std::memory_order_relaxed)) + " positions, APS5_IMAGE_TABLE_KEY_SCAN " + std::to_string(TableKeyScanLimit()) + ")";
    }
    std::fprintf(stderr, "[image-table] (10 s): %llu snapshots, %llu tables, %llu keys, %llu selected records, structural poison:%s, narrowing %llu applied / %llu skipped%s%s, reused %llu unwritten / %llu compared, %.1f ms\n",
        static_cast<unsigned long long>(snapshots), static_cast<unsigned long long>(counters.tables.exchange(0, std::memory_order_relaxed)), static_cast<unsigned long long>(counters.keys.exchange(0, std::memory_order_relaxed)), static_cast<unsigned long long>(counters.selected.exchange(0, std::memory_order_relaxed)), poison.empty() ? " none" : poison.c_str(),
        static_cast<unsigned long long>(counters.narrowed.exchange(0, std::memory_order_relaxed)), static_cast<unsigned long long>(counters.narrowSkipped.exchange(0, std::memory_order_relaxed)), skips.empty() ? "" : ":", skips.c_str(), static_cast<unsigned long long>(counters.reusedUnwritten.exchange(0, std::memory_order_relaxed)), static_cast<unsigned long long>(counters.reusedCompared.exchange(0, std::memory_order_relaxed)), static_cast<double>(counters.nanoseconds.exchange(0, std::memory_order_relaxed)) / 1e6);
}

enum class RecordState : std::uint8_t { OutsideDomain, Read, Unmapped, Carried };

struct ColumnContents {
    SharedValues<std::uint32_t> codes;
    SharedValues<DescriptorValue> words;
    SharedValues<std::pair<std::uint64_t, std::uint64_t>> readRanges;
    std::uint32_t poisoned = 0;
    std::uint32_t unmapped = 0;
    std::uint32_t carried = 0;
};

struct ColumnRecords {
    std::uint64_t base = 0;
    std::uint64_t size = 0;
    std::uint32_t records = 0;
    std::uint32_t keys = 0;
    std::uint32_t fault = 0;
    bool narrowed = false;
    bool outside = false;
    bool sealed = false;
    std::vector<RecordState> states;
    std::vector<std::uint32_t> words;
    std::vector<DescriptorValue> distinct;
    std::vector<std::pair<std::uint64_t, std::uint64_t>> readRanges;
    ColumnContents contents;
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

bool TableSnapshotReuse() {
    static const bool enabled = std::getenv("APS5_NO_TABLE_SNAPSHOT_REUSE") == nullptr;
    return enabled;
}

struct ColumnKey {
    std::uint64_t base = 0;
    std::uint64_t size = 0;
    std::uint32_t records = 0;
    std::uint32_t keys = 0;
    std::uint32_t stride = 0;
    std::uint32_t addend = 0;
    std::uint32_t offset = 0;
    std::uint32_t dwordCount = 0;
    std::uint32_t maxKey = 0;
    std::uint32_t foldedAddend = 0;
    std::uint32_t foldedCarry = 0;
    bool address = false;
    bool sampler = false;

    bool operator==(const ColumnKey& other) const = default;
};

struct ColumnMemo {
    ColumnKey key;
    std::shared_ptr<const SrtGuestWords> read;
    ColumnContents contents;
    std::uint64_t low = 0;
    std::uint64_t high = 0;

    std::size_t Footprint() const {
        return read->words.size() + read->runs.size() * 3u + contents.codes.size() + contents.words.size() * 9u + contents.readRanges.size() * 4u;
    }
};

constexpr std::size_t ColumnMemoEntries = 16;
constexpr std::size_t ColumnMemoWords = std::size_t{1} << 22u;

std::vector<ColumnMemo>& ColumnMemos() {
    thread_local std::vector<ColumnMemo>* slot = nullptr;
    return ThreadOwned(slot);
}

template <typename TSlot>
struct SlotScratch {
    std::vector<TSlot> slots;
    std::uint32_t stamp = 0;
    bool leased = false;
};

template <typename TSlot, typename TTag>
SlotScratch<TSlot>& threadScratch() {
    return HostThreadLocal<SlotScratch<TSlot>, TTag>();
}

constexpr std::size_t KeptScratchBytes = std::size_t{2} << 20u;

template <typename TSlot>
class SlotLease {
public:
    explicit SlotLease(SlotScratch<TSlot>& shared) : scratch(shared.leased ? own : shared) {
        scratch.leased = true;
        Clear();
    }

    SlotLease(const SlotLease&) = delete;
    SlotLease& operator=(const SlotLease&) = delete;

    ~SlotLease() {
        if (scratch.slots.size() * sizeof(TSlot) > KeptScratchBytes) std::vector<TSlot>().swap(scratch.slots);
        scratch.leased = false;
    }

    void Clear() {
        if (++scratch.stamp != 0u) return;
        std::fill(scratch.slots.begin(), scratch.slots.end(), TSlot{});
        scratch.stamp = 1u;
    }

    std::vector<TSlot>& Slots() {
        return scratch.slots;
    }

    const std::vector<TSlot>& Slots() const {
        return scratch.slots;
    }

    std::uint32_t Stamp() const {
        return scratch.stamp;
    }

private:
    SlotScratch<TSlot> own;
    SlotScratch<TSlot>& scratch;
};

struct DenseSlot {
    std::uint32_t stamp = 0;
    std::uint32_t value = 0;
};

class DenseScratchMap {
public:
    DenseScratchMap(SlotScratch<DenseSlot>& shared, std::size_t count) : lease(shared), keys(count) {
        if (lease.Slots().size() < keys) lease.Slots().resize(keys);
    }

    void Clear() {
        lease.Clear();
    }

    std::optional<std::uint32_t> Find(std::size_t key) const {
        if (key >= keys) throw std::out_of_range("dense scratch map key is out of range");
        const auto& slot = lease.Slots()[key];
        return slot.stamp == lease.Stamp() ? std::optional<std::uint32_t>(slot.value) : std::nullopt;
    }

    void Set(std::size_t key, std::uint32_t value) {
        if (key >= keys) throw std::out_of_range("dense scratch map key is out of range");
        lease.Slots()[key] = {lease.Stamp(), value};
    }

private:
    SlotLease<DenseSlot> lease;
    std::size_t keys;
};

struct HashSlot {
    std::uint64_t hash = 0;
    std::uint32_t stamp = 0;
    std::uint32_t value = 0;
};

std::uint64_t hashWords(std::span<const std::uint32_t> words, std::uint64_t seed) {
    std::uint64_t hash = 0xcbf29ce484222325ull ^ seed;
    for (const auto word : words) hash = (hash ^ word) * 0x100000001b3ull;
    hash ^= hash >> 33u;
    hash *= 0xff51afd7ed558ccdull;
    return hash ^ (hash >> 33u);
}

class HashScratchIndex {
public:
    explicit HashScratchIndex(SlotScratch<HashSlot>& shared) : lease(shared) {}

    template <typename TEqual>
    std::optional<std::uint32_t> Find(std::uint64_t hash, TEqual&& equal) const {
        const auto& slots = lease.Slots();
        if (slots.empty()) return std::nullopt;
        const auto mask = slots.size() - 1u;
        for (auto at = static_cast<std::size_t>(hash) & mask;; at = (at + 1u) & mask) {
            const auto& slot = slots[at];
            if (slot.stamp != lease.Stamp()) return std::nullopt;
            if (slot.hash == hash && equal(slot.value)) return slot.value;
        }
    }

    void Clear() {
        lease.Clear();
        entries = 0;
    }

    void Insert(std::uint64_t hash, std::uint32_t value) {
        if ((entries + 1u) * 2u > lease.Slots().size()) grow();
        place(lease.Slots(), hash, value);
        entries++;
    }

private:
    static constexpr std::size_t MinimumSlots = 64;

    void place(std::vector<HashSlot>& slots, std::uint64_t hash, std::uint32_t value) const {
        const auto mask = slots.size() - 1u;
        auto at = static_cast<std::size_t>(hash) & mask;
        while (slots[at].stamp == lease.Stamp()) at = (at + 1u) & mask;
        slots[at] = {hash, lease.Stamp(), value};
    }

    void grow() {
        std::vector<HashSlot> grown(std::max(MinimumSlots, lease.Slots().size() * 2u));
        for (const auto& slot : lease.Slots()) {
            if (slot.stamp == lease.Stamp()) place(grown, slot.hash, slot.value);
        }
        lease.Slots().swap(grown);
    }

    SlotLease<HashSlot> lease;
    std::size_t entries = 0;
};

struct WordsIndexTag {};
struct ElementIndexTag {};
struct SamplerIndexTag {};
struct PoisonIndexTag {};
struct TableRecordsTag {};

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

    bool AddressAccessible(std::uint64_t address) {
        const auto page = address & ~(PageBytes - 1u);
        const auto [found, inserted] = wholePages.try_emplace(page, false);
        if (inserted) found->second = Accessible(page, PageBytes);
        return found->second || Accessible(address, sizeof(std::uint32_t));
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
        const auto skip = [&](NarrowSkip reason) {
            auto& counters = tableCounters();
            counters.narrowSkipped.fetch_add(1, std::memory_order_relaxed);
            counters.narrowSkips[static_cast<std::size_t>(reason)].fetch_add(1, std::memory_order_relaxed);
        };
        std::vector<std::uint32_t> values{0u};
        std::vector<std::pair<std::uint64_t, std::uint64_t>> scanned;
        for (const auto& domain : column.keyDomains) {
            if (domain.value) {
                DescriptorValue key;
                try {
                    walker.EvaluateDescriptorSource(plan, domain.source, runtime, key);
                } catch (const std::exception&) {
                    skip(NarrowSkip::Value);
                    return;
                }
                values.push_back(key.dwords[0]);
                continue;
            }
            const auto keyBuffer = EvaluateBuffer(domain.source);
            if (keyBuffer.Type() != 0u) {
                skip(NarrowSkip::NotBuffer);
                return;
            }
            const auto base = keyBuffer.Base48();
            const auto size = keyBuffer.GetSize();
            const std::uint64_t first = domain.offset & ~3u;
            const std::uint64_t positions = size >= first + 4u ? (size - first - 4u) / domain.stride + 1u : 0u;
            if (positions > TableKeyScanLimit() || first > 0xffffffffull) {
                auto& widest = tableCounters().narrowPositions;
                auto seen = widest.load(std::memory_order_relaxed);
                while (seen < positions && !widest.compare_exchange_weak(seen, positions, std::memory_order_relaxed)) {}
                skip(NarrowSkip::KeyScan);
                return;
            }
            if (positions != 0u && runtime.pendingWrite != nullptr && runtime.pendingWrite(runtime.userContext, base + first, size - first)) {
                skip(NarrowSkip::Pending);
                return;
            }
            if (runtime.readWords != nullptr && positions != 0u && domain.stride % 4u == 0u && domain.stride <= 64u) {
                const auto words = static_cast<std::size_t>(((positions - 1u) * domain.stride) / 4u + 1u);
                for (std::uint64_t page = (base + first) & ~(PageBytes - 1u); page < base + first + words * 4u; page += PageBytes) {
                    if (!PageAccessible(std::max(page, base + first), base, base + size)) {
                        skip(NarrowSkip::Unmapped);
                        return;
                    }
                }
                if (keyWords.size() < words) keyWords.resize(words);
                if (!runtime.readWords(runtime.userContext, base + first, keyWords.data(), words)) throw std::runtime_error("image table: failed to read guest memory at 0x" + hexText(base + first));
                const auto step = static_cast<std::size_t>(domain.stride / 4u);
                values.reserve(values.size() + positions);
                for (std::size_t index = 0; index < words; index += step) values.push_back(keyWords[index]);
            } else {
                for (std::uint64_t index = 0; index < positions; index++) {
                    const auto address = base + first + index * domain.stride;
                    if (!PageAccessible(address, base, base + size)) {
                        skip(NarrowSkip::Unmapped);
                        return;
                    }
                    values.push_back(Read(address));
                }
            }
            if (positions != 0u) scanned.emplace_back(base, size);
        }
        records.states.assign(records.keys, RecordState::OutsideDomain);
        records.outside = false;
        const std::uint32_t unwrapped = std::numeric_limits<std::uint32_t>::max() / column.stride;
        for (const auto value : values) {
            const std::uint32_t relative = value * column.stride;
            const auto record = value <= unwrapped ? value : relative % column.stride == 0u ? relative / column.stride : records.keys;
            if (record < records.keys) {
                records.states[record] = RecordState::Read;
                continue;
            }
            if (!records.outside && (column.address || ImageTableAbi::ScalarBufferDword(relative + column.addend, column.offset, records.size).has_value())) records.outside = true;
        }
        records.narrowed = true;
        for (const auto& [base, size] : scanned) tables.ranges.emplace_back(base, size);
        tableCounters().narrowed.fetch_add(1, std::memory_order_relaxed);
    }

    void Select(const TableColumn& column, ColumnRecords& records) {
        const auto base = records.base;
        const auto size = records.size;
        DescriptorValue selection;
        try {
            walker.EvaluateDescriptorSource(plan, column.selection, runtime, selection);
        } catch (const std::exception&) {
            records.fault = ImageTableAbi::PoisonCode(static_cast<std::uint32_t>(PoisonReason::Unmapped));
            return;
        }
        tableCounters().selected.fetch_add(1, std::memory_order_relaxed);
        const auto offset = selection.dwords[0];
        const auto entry = (column.addend + column.offset) & ~3u;
        records.base = base + ((offset + column.offset) & ~3u) - entry;
        records.size = entry + static_cast<std::uint64_t>(column.dwordCount) * sizeof(std::uint32_t);
        records.records = 1u;
        records.keys = 1u;
        records.states.assign(1u, RecordState::Read);
        records.words.assign(1u, 0u);
        if (offset >= column.foldedAddend && offset > 0xffffffffu - column.foldedCarry) {
            records.states.front() = RecordState::Carried;
            return;
        }
        DescriptorValue value;
        value.dwordCount = 8u;
        std::uint64_t low = std::numeric_limits<std::uint64_t>::max();
        std::uint64_t high = 0;
        for (std::uint32_t dword = 0; dword < column.dwordCount; dword++) {
            const auto position = ImageTableAbi::ScalarBufferDword(offset, column.offset + dword * 4u, size);
            if (!position.has_value()) {
                value.availableDwordMask |= static_cast<std::uint8_t>(1u << dword);
                continue;
            }
            const auto address = base + *position;
            if (!PageAccessible(address, base, base + size)) {
                records.states.front() = RecordState::Unmapped;
                return;
            }
            value.dwords[dword] = Read(address);
            value.availableDwordMask |= static_cast<std::uint8_t>(1u << dword);
            low = std::min(low, address);
            high = std::max(high, address + sizeof(std::uint32_t));
        }
        records.words.front() = WordsIndex(records, value);
        if (low < high) tables.ranges.emplace_back(low, high - low);
    }

    std::uint32_t WordsIndex(ColumnRecords& records, const DescriptorValue& value) {
        const auto hash = hashWords(value.dwords, (static_cast<std::uint64_t>(value.dwordCount) << 8u) | value.availableDwordMask);
        if (const auto found = wordsIndex.Find(hash, [&](std::uint32_t index) { return records.distinct[index] == value; })) return *found;
        const auto index = static_cast<std::uint32_t>(records.distinct.size());
        records.distinct.push_back(value);
        wordsIndex.Insert(hash, index);
        return index;
    }

    void Seal(ColumnRecords& records) {
        auto& contents = records.contents;
        std::vector<std::uint32_t> codes(records.keys, 0u);
        for (std::uint32_t key = 0; key < records.keys; key++) {
            switch (records.states[key]) {
                case RecordState::Read:
                    codes[key] = records.words[key];
                    continue;
                case RecordState::OutsideDomain:
                    codes[key] = ImageTableAbi::PoisonCode(static_cast<std::uint32_t>(PoisonReason::OutsideDomain));
                    break;
                case RecordState::Unmapped:
                    codes[key] = ImageTableAbi::PoisonCode(static_cast<std::uint32_t>(PoisonReason::Unmapped));
                    contents.unmapped++;
                    break;
                case RecordState::Carried:
                    codes[key] = ImageTableAbi::PoisonCode(static_cast<std::uint32_t>(PoisonReason::OffsetCarry));
                    contents.carried++;
                    break;
            }
            contents.poisoned++;
        }
        contents.codes = SharedValues<std::uint32_t>(std::move(codes));
        contents.words = SharedValues<DescriptorValue>(std::move(records.distinct));
        contents.readRanges = SharedValues<std::pair<std::uint64_t, std::uint64_t>>(std::move(records.readRanges));
        records.sealed = true;
        tableReuseTotals().columns.fetch_add(1, std::memory_order_relaxed);
    }

    ColumnRecords& Column(std::uint32_t source) {
        if (const auto found = columns.find(source); found != columns.end()) {
            return found->second;
        }
        auto& records = columns[source];
        ReadColumn(*plan.descriptorSources.at(source).tableColumn, source, records);
        if (!records.sealed) Seal(records);
        return records;
    }

    void ReadColumn(const TableColumn& column, std::uint32_t source, ColumnRecords& records) {
        stamp = {};
        comparedRuns.clear();
        comparedWords.clear();
        wordsIndex.Clear();
        const bool active = source >= activeSources.size() || activeSources[source] != 0u;
        if (!active) {
            records.fault = ImageTableAbi::PoisonCode(static_cast<std::uint32_t>(PoisonReason::Inactive));
            return;
        }
        if (column.address) {
            DescriptorValue pointer;
            walker.EvaluateDescriptorSource(plan, column.heapSource, runtime, pointer);
            records.base = (static_cast<std::uint64_t>(pointer.dwords[1]) << 32u) | pointer.dwords[0];
            records.records = ImageTableAbi::ScalarAddressRecords(column.stride);
        } else {
            const auto heap = EvaluateBuffer(column.heapSource);
            if (heap.Type() != 0u) {
                records.fault = ImageTableAbi::PoisonCode(static_cast<std::uint32_t>(PoisonReason::NotBuffer));
                return;
            }
            records.base = heap.Base48();
            records.size = heap.GetSize();
            if (column.selected) {
                Select(column, records);
                return;
            }
            records.records = ImageTableAbi::ScalarBufferRecords(column.addend, column.stride, column.offset + (column.indirect ? 4u : 0u), records.size);
            if (records.size != 0u) tables.ranges.emplace_back(records.base, records.size);
        }
        records.records = std::min(records.records, ImageTableAbi::KeyRecords(column.maxKey, column.stride));
        records.keys = std::min(records.records, ImageTableAbi::MaxKeys);
        records.outside = records.keys < records.records;
        if (!column.keyDomains.empty() && records.keys != 0u) {
            Narrow(column, records);
        }
        const auto unread = [&] {
            if (!records.narrowed) records.states.assign(records.keys, RecordState::Read);
            records.words.assign(records.keys, 0u);
        };
        const auto end = records.base + records.size;
        if (column.indirect) {
            unread();
            for (std::uint32_t record = 0; record < records.keys; record++) {
                if (records.states[record] != RecordState::Read) continue;
                const auto offset = column.addend + record * column.stride;
                const auto lowWord = ImageTableAbi::ScalarBufferDword(offset, column.offset, records.size);
                const auto highWord = ImageTableAbi::ScalarBufferDword(offset, column.offset + 4u, records.size);
                if (!lowWord.has_value() || !highWord.has_value() || !PageAccessible(records.base + *lowWord, records.base, end) || !PageAccessible(records.base + *highWord, records.base, end)) {
                    records.states[record] = RecordState::Unmapped;
                    continue;
                }
                std::uint64_t pointer = (static_cast<std::uint64_t>(Read(records.base + *highWord)) << 32u) | Read(records.base + *lowWord);
                for (const auto& hop : column.pointerHops) {
                    const auto low = ImageTableAbi::ScalarAddressDword(pointer, hop.offset, hop.immediate);
                    const auto high = ImageTableAbi::ScalarAddressDword(pointer, hop.offset, hop.immediate + 4u);
                    if (!low.has_value() || !high.has_value() || (*low & 3u) != 0u || *high != *low + sizeof(std::uint32_t) || !AddressAccessible(*low) || !AddressAccessible(*high)) {
                        records.states[record] = RecordState::Unmapped;
                        break;
                    }
                    pointer = (static_cast<std::uint64_t>(Read(*high)) << 32u) | Read(*low);
                    tables.ranges.emplace_back(*low, 2u * sizeof(std::uint32_t));
                }
                if (records.states[record] != RecordState::Read) continue;
                DescriptorValue value;
                value.dwordCount = column.sampler ? 4u : 8u;
                std::uint64_t first = std::numeric_limits<std::uint64_t>::max();
                std::uint64_t past = 0;
                for (std::uint32_t dword = 0; dword < column.dwordCount; dword++) {
                    auto address = ImageTableAbi::ScalarAddressDword(pointer, column.indirectOffset, column.indirectImmediate + dword * 4u);
                    if (address.has_value() && (*address & 3u) != 0u) address.reset();
                    if (!address.has_value() || !AddressAccessible(*address)) {
                        records.states[record] = RecordState::Unmapped;
                        break;
                    }
                    value.dwords[dword] = Read(*address);
                    value.availableDwordMask |= static_cast<std::uint8_t>(1u << dword);
                    first = std::min(first, *address);
                    past = std::max(past, *address + sizeof(std::uint32_t));
                }
                if (records.states[record] != RecordState::Read) continue;
                records.words[record] = WordsIndex(records, value);
                tables.ranges.emplace_back(first, past - first);
            }
            return;
        }
        const bool memoized = TableSnapshotReuse() && !records.narrowed && records.keys != 0u;
        const ColumnKey key{records.base, records.size, records.records, records.keys, column.stride, column.addend, column.offset, column.dwordCount, column.maxKey, column.foldedAddend, column.foldedCarry, column.address, column.sampler};
        if (memoized && Reuse(key, column, records, false)) return;
        if (memoized && !column.address && records.size != 0u && runtime.stampWrites != nullptr) {
            const auto span = std::min<std::uint64_t>(records.size, static_cast<std::uint64_t>(column.addend) + column.offset + static_cast<std::uint64_t>(records.keys) * column.stride + column.dwordCount * 4u);
            stamp = runtime.stampWrites(runtime.userContext, records.base, span);
        }
        if (memoized && Reuse(key, column, records, true)) return;
        unread();
        constexpr auto Unread = std::numeric_limits<std::uint32_t>::max();
        std::vector<std::uint32_t> whole(records.keys, Unread);
        std::vector<std::pair<std::uint32_t, std::array<std::uint32_t, 8>>> split;
        std::vector<std::pair<std::uint64_t, std::uint32_t>> runs;
        std::uint32_t total = 0;
        std::uint64_t low = std::numeric_limits<std::uint64_t>::max();
        std::uint64_t high = 0;
        std::uint64_t checkedPage = std::numeric_limits<std::uint64_t>::max();
        bool checkedAccessible = false;
        const auto accessible = [&](std::uint64_t address) {
            if (column.address) return AddressAccessible(address);
            if ((address & ~(PageBytes - 1u)) != checkedPage) {
                checkedPage = address & ~(PageBytes - 1u);
                checkedAccessible = PageAccessible(address, records.base, end);
            }
            return checkedAccessible;
        };
        const auto wholeAccessible = [&](std::uint64_t address) {
            if (!column.address) return accessible(address);
            const auto [found, inserted] = wholePages.try_emplace(address & ~(PageBytes - 1u), false);
            if (inserted) found->second = Accessible(address & ~(PageBytes - 1u), PageBytes);
            return found->second;
        };
        const auto locate = [&](std::uint32_t offset, std::uint32_t immediate) -> std::optional<std::uint64_t> {
            if (column.address) return ImageTableAbi::ScalarAddressDword(records.base, offset, immediate);
            const auto position = ImageTableAbi::ScalarBufferDword(offset, immediate, records.size);
            return position.has_value() ? std::optional<std::uint64_t>(records.base + *position) : std::nullopt;
        };
        const auto take = [&](std::uint64_t address, std::uint32_t words) {
            if (!runs.empty() && address > runs.back().first && address - runs.back().first == static_cast<std::uint64_t>(runs.back().second) * 4u) {
                runs.back().second += words;
            } else {
                runs.emplace_back(address, words);
            }
            if (column.address) records.readRanges.emplace_back(address, static_cast<std::uint64_t>(words) * sizeof(std::uint32_t));
            low = std::min(low, address);
            high = std::max(high, address + static_cast<std::uint64_t>(words) * sizeof(std::uint32_t));
            total += words;
            return total - words;
        };
        const auto last = (column.dwordCount - 1u) * 4u;
        for (std::uint32_t record = 0; record < records.keys; record++) {
            if (records.states[record] != RecordState::Read) continue;
            const auto offset = column.addend + record * column.stride;
            if (offset >= column.foldedAddend && offset > 0xffffffffu - column.foldedCarry) {
                records.states[record] = RecordState::Carried;
                continue;
            }
            const auto head = locate(offset, column.offset);
            const auto tail = locate(offset, column.offset + last);
            if (head.has_value() && tail.has_value() && (*head & 3u) == 0u && *tail > *head && *tail - *head == last && wholeAccessible(*head) && wholeAccessible(*tail)) {
                whole[record] = take(*head, column.dwordCount);
                continue;
            }
            std::array<std::uint32_t, 8> slots;
            slots.fill(Unread);
            for (std::uint32_t dword = 0; dword < column.dwordCount; dword++) {
                auto address = locate(offset, column.offset + dword * 4u);
                if (column.address && address.has_value() && (*address & 3u) != 0u) address.reset();
                if (!address.has_value() && !column.address) continue;
                if (!address.has_value() || !accessible(*address)) {
                    records.states[record] = RecordState::Unmapped;
                    break;
                }
                slots[dword] = take(*address, 1u);
            }
            if (records.states[record] == RecordState::Read) split.emplace_back(record, slots);
        }
        std::vector<std::uint32_t> read;
        if (!comparedWords.empty() && runs == comparedRuns) {
            read = std::move(comparedWords);
        } else {
            read.resize(total);
            ReadRuns(runs, read);
        }
        auto next = split.begin();
        for (std::uint32_t record = 0; record < records.keys; record++) {
            if (records.states[record] != RecordState::Read) continue;
            DescriptorValue value;
            value.dwordCount = column.sampler ? 4u : 8u;
            if (whole[record] != Unread) {
                std::copy_n(read.begin() + whole[record], column.dwordCount, value.dwords.begin());
                value.availableDwordMask = static_cast<std::uint8_t>((1u << column.dwordCount) - 1u);
            } else {
                for (std::uint32_t dword = 0; dword < column.dwordCount; dword++) {
                    if (next->second[dword] != Unread) {
                        value.dwords[dword] = read[next->second[dword]];
                        value.availableDwordMask |= static_cast<std::uint8_t>(1u << dword);
                    }
                }
                ++next;
            }
            records.words[record] = WordsIndex(records, value);
        }
        if (column.address && low < high) tables.ranges.emplace_back(low, high - low);
        std::sort(records.readRanges.begin(), records.readRanges.end());
        std::size_t ranges = 0;
        for (const auto& range : records.readRanges) {
            if (ranges != 0u && range.first - records.readRanges[ranges - 1u].first <= records.readRanges[ranges - 1u].second) {
                auto& previous = records.readRanges[ranges - 1u];
                previous.second = std::max(previous.second, range.first - previous.first + range.second);
            } else {
                records.readRanges[ranges++] = range;
            }
        }
        records.readRanges.resize(ranges);
        Seal(records);
        if (memoized) Remember(key, records, std::move(runs), std::move(read), column.address && low < high ? low : 0u, column.address && low < high ? high : 0u);
    }

    void ReadRuns(const std::vector<std::pair<std::uint64_t, std::uint32_t>>& runs, std::vector<std::uint32_t>& read) const {
        std::size_t at = 0;
        for (const auto& [address, count] : runs) {
            if (runtime.readWords != nullptr) {
                if (!runtime.readWords(runtime.userContext, address, read.data() + at, count)) throw std::runtime_error("image table: failed to read guest memory at 0x" + hexText(address));
            } else {
                for (std::uint32_t word = 0; word < count; word++) read[at + word] = Read(address + word * 4u);
            }
            at += count;
        }
        tableReuseTotals().wordsRead.fetch_add(read.size(), std::memory_order_relaxed);
    }

    bool WholePageAccessible(std::uint64_t address) {
        const auto [found, inserted] = wholePages.try_emplace(address & ~(PageBytes - 1u), false);
        if (inserted) found->second = Accessible(address & ~(PageBytes - 1u), PageBytes);
        return found->second;
    }

    bool Unwritten(const ColumnMemo& memo) const {
        if (runtime.unchangedSince == nullptr || runtime.supplyWords == nullptr || memo.read->low >= memo.read->high) return false;
        const auto vouched = LoadStamp(*memo.read);
        if (vouched.generation == 0u || !runtime.unchangedSince(runtime.userContext, memo.read->low, memo.read->high - memo.read->low, vouched)) return false;
        return runtime.supplyWords(runtime.userContext, memo.read);
    }

    bool Compared(const TableColumn& column, const ColumnRecords& records, ColumnMemo& memo) {
        const auto end = records.base + records.size;
        for (const auto& [address, count] : memo.read->runs) {
            const auto past = address + static_cast<std::uint64_t>(count) * sizeof(std::uint32_t);
            for (auto page = address & ~(PageBytes - 1u); page < past; page += PageBytes) {
                const auto first = std::max(page, address);
                if (column.address ? !WholePageAccessible(first) : !PageAccessible(first, records.base, end)) return false;
            }
        }
        std::vector<std::uint32_t> read(memo.read->words.size());
        ReadRuns(memo.read->runs, read);
        if (read != memo.read->words) {
            comparedRuns = memo.read->runs;
            comparedWords = std::move(read);
            return false;
        }
        StoreStamp(*memo.read, stamp);
        return true;
    }

    void Apply(const TableColumn& column, ColumnRecords& records, const ColumnMemo& memo) {
        records.contents = memo.contents;
        records.sealed = true;
        if (column.address && memo.low < memo.high) tables.ranges.emplace_back(memo.low, memo.high - memo.low);
    }

    bool Reuse(const ColumnKey& key, const TableColumn& column, ColumnRecords& records, bool compare) {
        auto& memos = ColumnMemos();
        const auto found = std::find_if(memos.begin(), memos.end(), [&](const ColumnMemo& entry) { return entry.key == key; });
        if (found == memos.end()) return false;
        auto& counters = tableCounters();
        auto& totals = tableReuseTotals();
        if (!compare && Unwritten(*found)) {
            counters.reusedUnwritten.fetch_add(1, std::memory_order_relaxed);
            totals.unwritten.fetch_add(1, std::memory_order_relaxed);
        } else if (compare && Compared(column, records, *found)) {
            counters.reusedCompared.fetch_add(1, std::memory_order_relaxed);
            totals.compared.fetch_add(1, std::memory_order_relaxed);
        } else {
            return false;
        }
        Apply(column, records, *found);
        totals.shared.fetch_add(1, std::memory_order_relaxed);
        std::rotate(memos.begin(), found, found + 1);
        return true;
    }

    void Remember(const ColumnKey& key, const ColumnRecords& records, std::vector<std::pair<std::uint64_t, std::uint32_t>>&& runs, std::vector<std::uint32_t>&& words, std::uint64_t low, std::uint64_t high) {
        if (records.contents.poisoned != records.contents.carried) return;
        auto& memos = ColumnMemos();
        auto found = std::find_if(memos.begin(), memos.end(), [&](const ColumnMemo& entry) { return entry.key == key; });
        if (found == memos.end()) found = memos.emplace(memos.end());
        auto& memo = *found;
        memo.key = key;
        std::uint64_t first = std::numeric_limits<std::uint64_t>::max();
        std::uint64_t past = 0;
        std::uint64_t total = 0;
        bool ordered = !runs.empty();
        for (const auto& [address, count] : runs) {
            const auto bytes = static_cast<std::uint64_t>(count) * sizeof(std::uint32_t);
            ordered = ordered && count != 0u && address % sizeof(std::uint32_t) == 0u && address >= past && address <= std::numeric_limits<std::uint64_t>::max() - bytes;
            first = std::min(first, address);
            past = std::max(past, address + bytes);
            total += count;
        }
        ordered = ordered && total == words.size();
        memo.read = std::make_shared<const SrtGuestWords>(SrtGuestWords{std::move(runs), std::move(words), std::make_shared<SrtStampCell>(), first < past ? first : 0u, first < past ? past : 0u, ordered});
        StoreStamp(*memo.read, stamp);
        memo.contents = records.contents;
        memo.low = low;
        memo.high = high;
        if (memo.Footprint() > ColumnMemoWords) {
            memos.erase(found);
            return;
        }
        std::rotate(memos.begin(), found, found + 1);
        std::size_t kept = 0;
        std::size_t footprint = 0;
        while (kept < memos.size() && kept < ColumnMemoEntries && footprint + memos[kept].Footprint() <= ColumnMemoWords) footprint += memos[kept++].Footprint();
        memos.resize(kept);
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
        out.codes = records.contents.codes;
        out.words = records.contents.words;
        out.readRanges = records.contents.readRanges;
        auto& counters = tableCounters();
        if (records.fault != 0u) counters.poison[records.fault & ~ImageTableAbi::PoisonFlag].fetch_add(1, std::memory_order_relaxed);
        if (records.contents.unmapped != 0u) counters.poison[static_cast<std::size_t>(PoisonReason::Unmapped)].fetch_add(records.contents.unmapped, std::memory_order_relaxed);
        if (records.contents.carried != 0u) counters.poison[static_cast<std::size_t>(PoisonReason::OffsetCarry)].fetch_add(records.contents.carried, std::memory_order_relaxed);
        counters.keys.fetch_add(records.keys, std::memory_order_relaxed);
        if (TableTraced()) traceTable(plan, table, column, {records.base, records.size, records.records, records.keys, records.contents.poisoned, records.narrowed});
    }

    const IrResourcePlan& plan;
    const SrtRuntime& runtime;
    SrtWalker& walker;
    const std::vector<std::uint8_t>& activeSources;
    ImageTableSnapshot& tables;
    std::map<std::uint32_t, ColumnRecords> columns;
    HashScratchIndex wordsIndex{threadScratch<HashSlot, WordsIndexTag>()};
    std::unordered_map<std::uint64_t, bool> pages;
    std::unordered_map<std::uint64_t, bool> wholePages;
    SrtWriteStamp stamp;
    std::vector<std::pair<std::uint64_t, std::uint32_t>> comparedRuns;
    std::vector<std::uint32_t> comparedWords;
    std::vector<std::uint32_t> keyWords;
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

    struct ValuesStorage {};
    auto& values = HostThreadLocal<std::vector<DescriptorValue>, ValuesStorage>();
    values.clear();
    const auto evaluateStarted = MaterializeProfiled() ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
    walker.EvaluateRuntimeSources(plan, plan.materializationSources, runtime, values, snapshot.flattenedSrt, plan.cleanFlatSlots, activeSources, &snapshot.srtPoison, &snapshot.nullRootReads);
    if (MaterializeProfiled()) evaluateNanoseconds.fetch_add(static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - evaluateStarted).count()), std::memory_order_relaxed);

    std::size_t cursor = 0;
    if (values.size() < plan.info.buffers.size()) {
        throw std::runtime_error("materialization sources are missing buffer descriptors");
    }
    snapshot.buffers.assign(values.begin(), values.begin() + plan.info.buffers.size());
    if (GcnGuestShaders()) {
        for (auto& buffer : snapshot.buffers) {
            buffer.dwords[1] &= GcnBufferWord1Mask;
            buffer.dwords[3] = GcnBufferWord3(buffer.dwords[1], buffer.dwords[3]);
        }
    }
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
        if (GcnGuestShaders()) {
            const auto words = GcnImageWords(std::span<const std::uint32_t, 8>(descriptor.dwords.data(), 8u));
            std::copy(words.begin(), words.end(), descriptor.dwords.begin());
        }
        const auto activity = plan.descriptorSources.size() + i;
        if (activity < activeSources.size() && activeSources[activity] == 0u) {
            descriptor = DescriptorValue{};
            descriptor.dwordCount = 8u;
        }
        if (!validImageDescriptor(descriptor, image.r128) && !nullImageDescriptor(image, descriptor)) {
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

void materializeForcedDegamma(const IrResourcePlan& plan, ResourceSnapshot& snapshot) {
    for (std::uint32_t index = 0; index < plan.info.images.size(); ++index) {
        auto& descriptor = snapshot.images.at(index);
        if (((descriptor.dwords[1] >> 20u) & 0x1ffu) != static_cast<std::uint32_t>(IrBufferFormat::Format8_8_8_8UInt)) continue;
        bool forced = false;
        bool plain = false;
        for (const auto& pair : plan.info.sampledPairs) {
            if (pair.image != index) continue;
            const auto& sampler = snapshot.samplers.at(pair.sampler);
            if (sampler.dwordCount != 4u) throw std::runtime_error("FORCE_DEGAMMA requires four sampler descriptor words");
            const bool enabled = (sampler.dwords[0] & (1u << 20u)) != 0u;
            forced = forced || enabled;
            plain = plain || !enabled;
        }
        if (!forced) continue;
        const auto& image = plan.info.images.at(index);
        if (!image.singleMipPlaneBiasCompatible || image.dimension != RdnaImageDimension::Dim2D || image.table != NoTable || image.resourceClass != ImageResourceClass::Sampled || image.depthCompare || image.packed) throw std::runtime_error("FORCE_DEGAMMA on RGBA8_UINT requires a direct plain biased pixel sample");
        if (plain) throw std::runtime_error("FORCE_DEGAMMA on RGBA8_UINT with mixed sampler states is not implemented");
        if ((descriptor.dwords[3] >> 28u) != 10u || (descriptor.dwords[4] & 0x1fffu) != 0u || (descriptor.dwords[3] & 0xff000u) != 0u || (descriptor.dwords[5] & 0xf0u) != 0u) throw std::runtime_error("FORCE_DEGAMMA on RGBA8_UINT requires a one-slice volume at mip zero");
        descriptor.dwords[1] = (descriptor.dwords[1] & ~(0x1ffu << 20u)) | (static_cast<std::uint32_t>(IrBufferFormat::Format8_8_8_8Srgb) << 20u);
    }
}

std::uint32_t colorCompareReference(IrBufferFormat format) {
    const auto value = static_cast<std::uint32_t>(format);
    if (((value >= 1u && value <= 77u) || (value >= 128u && value <= 130u)) && SampledTextureNumericClass(format) == IrTextureNumericClass::Float && format != IrBufferFormat::Format11_11_10UNorm && format != IrBufferFormat::Format10_11_11Float) return EmulatedCompare::ReferenceZero;
    throw std::runtime_error("comparison sampling of a color texture is implemented only for floating-point, UNORM and SNORM formats (format " + std::to_string(value) + ")");
}

bool convertedSampleCompatible(const ImageResource& image) {
    return image.resourceClass != ImageResourceClass::Storage && !image.depthCompare && !image.atomic && !image.packed && image.table == NoTable && image.depthBitsCompatible && (image.emulatedCompare & EmulatedCompare::ConvertedSampleUnsupported) == 0u && (image.dimension == RdnaImageDimension::Dim2D || image.dimension == RdnaImageDimension::Dim2DArray);
}

std::uint32_t convertedSampleState(const ShaderInfo& info, const ResourceSnapshot& snapshot, std::uint32_t index) {
    const auto& image = info.images[index];
    if (image.srgbDecodeCompatible || image.depthCompare || image.resourceClass == ImageResourceClass::Storage || index >= snapshot.images.size()) return 0u;
    const auto& descriptor = snapshot.images[index];
    if (descriptor.dwordCount != 8u || nullImageDescriptor(image, descriptor)) return 0u;
    const auto format = rawImageFormat(descriptor);
    if (format != IrBufferFormat::Format11_11_10UNorm) return 0u;
    std::optional<std::uint32_t> samplerState;
    for (const auto& pair : info.sampledPairs) {
        if (pair.image != index) continue;
        if (pair.sampler >= snapshot.samplers.size() || snapshot.samplers[pair.sampler].dwordCount != 4u) throw std::runtime_error("sampling of a converted image has no sampler descriptor");
        const auto& words = snapshot.samplers[pair.sampler].dwords;
        if (((words[0] >> 29u) & 0x3u) != 0u) throw std::runtime_error("sampling of a converted image through a min or max reduction sampler is not implemented");
        if (((words[0] >> 15u) & 0x1u) != 0u) throw std::runtime_error("sampling of a converted image does not implement unnormalized coordinates");
        const auto magFilter = (words[2] >> 20u) & 0x3u;
        const auto minFilter = (words[2] >> 22u) & 0x3u;
        if (magFilter != minFilter || magFilter > 1u) throw std::runtime_error("sampling of a converted image is implemented only with equal point or bilinear minification and magnification filters");
        const auto addressMode = [](std::uint32_t clamp) {
            if (clamp == 0u) return EmulatedCompare::AddressWrap;
            if (clamp == 2u) return EmulatedCompare::AddressEdge;
            throw std::runtime_error("sampling of a converted image is implemented only with wrap or clamp-to-edge addressing");
        };
        const auto state = EmulatedCompare::Enabled | (magFilter == 1u ? EmulatedCompare::Linear : 0u) | (addressMode(words[0] & 0x7u) << EmulatedCompare::ClampXShift) | (addressMode((words[0] >> 3u) & 0x7u) << EmulatedCompare::ClampYShift);
        if (samplerState.has_value() && *samplerState != state) throw std::runtime_error("sampling of a converted image through samplers that disagree is not implemented");
        samplerState = state;
    }
    if (!samplerState.has_value()) throw std::runtime_error("sampling of a converted image has no paired sampler");
    return *samplerState;
}

std::uint32_t emulatedCompareState(const ShaderInfo& info, const ResourceSnapshot& snapshot, std::uint32_t index) {
    const auto& image = info.images[index];
    const auto& descriptor = snapshot.images[index];
    if (!image.depthCompare) return convertedSampleState(info, snapshot, index);
    if (descriptor.dwordCount != 8u || nullImageDescriptor(image, descriptor)) return 0u;
    const auto format = rawImageFormat(descriptor);
    if (format == IrBufferFormat::Format32Float || format == IrBufferFormat::Format16UNorm || IsDepthBitsTexture(descriptor.dwords[1], descriptor.dwords[3])) return 0u;
    if (image.table != NoTable || (image.emulatedCompare & EmulatedCompare::Unsupported) != 0u) throw std::runtime_error("unsupported color comparison image instructions");
    if ((image.emulatedCompare & EmulatedCompare::RequiresSingleLevel) != 0u && ((descriptor.dwords[3] >> 12u) & 0xfu) != ((descriptor.dwords[3] >> 16u) & 0xfu)) throw std::runtime_error("color comparison requires a single mip level");
    const auto reference = colorCompareReference(format);
    const auto type = rawImageType(descriptor);
    if (type != ImageType::Color2D && type != ImageType::Color2DArray && type != ImageType::Cube) throw std::runtime_error("comparison sampling of a color texture is implemented only for 2D, 2D array and cube views");
    bool redChannel = false;
    bool compareChannel = false;
    for (std::uint32_t channel = 0u; channel < 4u; ++channel) {
        const auto select = (descriptorImageSwizzle(descriptor) >> (channel * 3u)) & 0x7u;
        if (select == 2u || select == 3u) throw std::runtime_error("comparison sampling of a color texture is implemented only when every view channel is a constant or a color channel");
        redChannel = redChannel || select == 4u;
        compareChannel = compareChannel || select >= 4u;
    }
    if (compareChannel && !redChannel) throw std::runtime_error("comparison sampling of a color texture is implemented only when a view channel selects red");
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
        const bool constantZero = reference == EmulatedCompare::ReferenceZero;
        if (type == ImageType::Cube && magFilter == 1u && ((words[0] >> 28u) & 1u) == 0u && !constantZero) {
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
        if (!constantZero && (magFilter != minFilter || magFilter > 1u)) throw std::runtime_error("comparison sampling of a color texture is implemented only with equal point or bilinear minification and magnification filters");
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
        const auto& buffer = plan.info.buffers[i];
        const bool scalarOnly = buffer.scalar && !buffer.written && !buffer.atomic && !buffer.formatted && buffer.imageAlias == BufferResource::NoImageAlias
            && std::none_of(plan.memoryInfo.begin(), plan.memoryInfo.end(), [i](const MemoryInfo& memory) { return memory.kind == ResourceKind::Buffer && memory.resource == i; });
        if (scalarOnly) {
            snapshot.buffers.at(i).dwords[1] &= ~0x40000000u;
            snapshot.buffers.at(i).dwords[3] = 0u;
        }
        const auto decoded = decodeBufferDescriptor(snapshot.buffers.at(i));
        if (decoded.Type() != 0u) {
            const auto& words = snapshot.buffers.at(i).dwords;
            char detail[64];
            std::snprintf(detail, sizeof(detail), " (buffer %u V# %08x %08x %08x %08x)", i, words[0], words[1], words[2], words[3]);
            throw std::runtime_error(std::string("buffer descriptor uses an unsupported type") + detail);
        }
        if (plan.stage != IrShaderStage::Compute && decoded.AddTid()) throw std::runtime_error("buffer ADD_TID is only valid for compute shaders");
    }
    if (snapshot.flattenedSrt.size() != plan.srtReads.size()) throw std::runtime_error("runtime SRT size differs from the static interface");
    for (std::uint32_t i = 0u; i < plan.info.images.size(); ++i) {
        const auto& image = plan.info.images[i];
        if (image.table != NoTable) continue;
        const auto& descriptor = snapshot.images.at(i);
        if (!image.convertedSamples && !image.srgbDecodeCompatible && convertedSampleCompatible(image) && descriptor.dwordCount == 8u && !nullImageDescriptor(image, descriptor)) {
            if (rawImageFormat(descriptor) == IrBufferFormat::Format11_11_10UNorm) throw ConvertedSampleRequired("samples a converted image through a shader prepared without converted-sample modes");
        }
        static_cast<void>(ResourceMaterializer::RuntimeImageMode(image, descriptor, plan.info.runtimeImageModes.at(i)));
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

std::vector<bool> reducingSamplers(const ShaderInfo& info, const ResourceSnapshot& snapshot) {
    std::vector<bool> reducing(info.images.size(), false);
    for (const auto& pair : info.sampledPairs) {
        if (info.samplers.at(pair.sampler).table != NoTable) continue;
        const auto& words = snapshot.samplers.at(pair.sampler).dwords;
        if (reducesBetweenTexels(words[0], words[2])) reducing.at(pair.image) = true;
    }
    return reducing;
}

bool TableResolutionReuse() {
    static const bool enabled = std::getenv("APS5_NO_TABLE_RESOLUTION_REUSE") == nullptr;
    return enabled;
}

struct ResolutionMemoEntry {
    std::uint64_t memoKey = 0;
    std::vector<ImageTableColumnSnapshot> tables;
    std::vector<bool> reducing;
    std::shared_ptr<const ResolvedImageTables> resolved;
    std::size_t footprint = 0;
};

struct ResolutionMemo {
    std::mutex mutex;
    std::vector<ResolutionMemoEntry> entries;
};

ResolutionMemo& resolutionMemo() {
    static ResolutionMemo memo;
    return memo;
}

constexpr std::size_t ResolutionMemoEntries = 256;
constexpr std::size_t ResolutionMemoVariantEntries = 4;
constexpr std::size_t ResolutionMemoWords = std::size_t{1} << 23u;

bool sameColumns(const std::vector<ImageTableColumnSnapshot>& left, const std::vector<ImageTableColumnSnapshot>& right) {
    if (left.size() != right.size()) return false;
    for (std::size_t index = 0; index < left.size(); index++) {
        const auto& a = left[index];
        const auto& b = right[index];
        if (a.base != b.base || a.size != b.size || a.records != b.records || a.keys != b.keys || a.fault != b.fault || a.outside != b.outside || !a.codes.Shares(b.codes) || !a.words.Shares(b.words)) return false;
    }
    return true;
}

std::size_t resolutionFootprint(const ResolvedImageTables& resolved, const std::vector<ImageTableColumnSnapshot>& tables) {
    std::size_t footprint = resolved.map.size() + (resolved.elements.size() * sizeof(ImageTableElement) + resolved.samplers.size() * sizeof(ImageTableSamplerElement) + resolved.poison.size() * sizeof(ImageTablePoisonRecord)) / sizeof(std::uint32_t);
    for (const auto& table : tables) footprint += 16u + table.codes.size() + table.words.size() * 9u;
    return footprint;
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
    HashScratchIndex poisonIndex{threadScratch<HashSlot, PoisonIndexTag>()};
    std::vector<bool> expected;
    std::size_t expectedCount = 0;
    const auto poison = [&](std::uint32_t resource, PoisonReason reason, const DescriptorValue* words) {
        const auto value = words != nullptr ? *words : DescriptorValue{};
        const auto hash = hashWords(value.dwords, (static_cast<std::uint64_t>(resource) << 40u) ^ (static_cast<std::uint64_t>(reason) << 8u) ^ value.dwordCount);
        auto index = poisonIndex.Find(hash, [&](std::uint32_t known) {
            const auto& record = out.poison[known];
            return record.resource == resource && record.reason == reason && record.words == value.dwords && record.dwordCount == value.dwordCount;
        });
        if (!index.has_value()) {
            index = static_cast<std::uint32_t>(out.poison.size());
            out.poison.push_back({value.dwords, value.dwordCount, resource, reason});
            poisonIndex.Insert(hash, *index);
        }
        if (*index >= expected.size()) expected.resize(out.poison.size(), false);
        if (!expected[*index]) {
            expected[*index] = true;
            expectedCount++;
        }
        return Abi::PoisonCode(*index);
    };
    HashScratchIndex elementIndex{threadScratch<HashSlot, ElementIndexTag>()};
    HashScratchIndex samplerIndex{threadScratch<HashSlot, SamplerIndexTag>()};
    std::vector<std::uint32_t> directSamplers(info.images.size(), 0u);
    for (const auto& pair : info.sampledPairs) {
        if (info.samplers.at(pair.sampler).table == NoTable) directSamplers.at(pair.image) |= 1u << pair.sampler;
    }
    const auto reducingSampler = reducingSamplers(info, snapshot);
    const auto imageRecord = [&](std::uint32_t resource, const DescriptorValue& words) {
        const auto& image = info.images[resource];
        const auto& modes = info.runtimeImageModes[resource];
        std::array<std::uint32_t, Abi::ImageRecordWords> record{0u, words.dwords[3], 0u};
        if ((words.availableDwordMask & (1u << 3u)) == 0u) {
            record[Abi::ImageRecordFlags] = poison(resource, PoisonReason::OutsideSnapshot, &words);
            const auto offset = static_cast<std::uint32_t>(out.map.size());
            out.map.insert(out.map.end(), record.begin(), record.end());
            return offset;
        }
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
                const auto hash = hashWords(element, (static_cast<std::uint64_t>(mode.dimension) << 16u) ^ (static_cast<std::uint64_t>(mode.depthCompare) << 8u) ^ static_cast<std::uint64_t>(kind));
                auto index = elementIndex.Find(hash, [&](std::uint32_t known) {
                    const auto& entry = out.elements[known];
                    return entry.words == element && entry.dimension == mode.dimension && entry.depthCompare == mode.depthCompare && entry.kind == kind;
                });
                if (!index.has_value() && out.elements.size() >= Abi::ImageElementBudget) {
                    record[Abi::ImageRecordFlags] = poison(resource, PoisonReason::Budget, &words);
                } else {
                    if (!index.has_value()) {
                        index = static_cast<std::uint32_t>(out.elements.size());
                        out.elements.push_back({element, mode.dimension, mode.depthCompare, kind, 0u});
                        elementIndex.Insert(hash, *index);
                    }
                    out.elements[*index].samplers |= directSamplers[resource];
                    record[Abi::ImageRecordFlags] = decoded << Abi::ModeShift;
                    record[Abi::ImageRecordElement] = *index;
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
        } else {
            const auto hash = hashWords(std::span(words.dwords.data(), 4u), 0);
            auto index = samplerIndex.Find(hash, [&](std::uint32_t known) {
                return std::equal(words.dwords.begin(), words.dwords.begin() + 4u, out.samplers[known].words.begin());
            });
            if (!index.has_value() && out.samplers.size() + Abi::SamplerEntryElements > Abi::SamplerElementBudget) {
                record[Abi::SamplerRecordFlags] = poison(resource, PoisonReason::Budget, &words);
            } else {
                if (!index.has_value()) {
                    index = static_cast<std::uint32_t>(out.samplers.size());
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
                    samplerIndex.Insert(hash, *index);
                }
                record[Abi::SamplerRecordElement] = *index;
            }
        }
        const auto offset = static_cast<std::uint32_t>(out.map.size());
        out.map.insert(out.map.end(), record.begin(), record.end());
        return offset;
    };
    std::vector<bool> outside(count, false);
    std::size_t recordCodes = 0;
    for (const auto& column : tables.tables) recordCodes = std::max(recordCodes, column.words.size());
    DenseScratchMap records(threadScratch<DenseSlot, TableRecordsTag>(), recordCodes);
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
        records.Clear();
        for (std::uint32_t key = 0; key < column.keys; key++) {
            const auto code = column.codes[key];
            std::uint32_t mapped = Abi::NullCode;
            if ((code & Abi::PoisonFlag) != 0u) {
                const auto reason = static_cast<PoisonReason>(code & ~Abi::PoisonFlag);
                mapped = reason == PoisonReason::OutsideDomain ? Abi::PoisonCode(Abi::OutsideDomainPoison) : poison(resource, reason, nullptr);
            } else if (out.map[header + Abi::TableFault] == 0u) {
                if (const auto known = records.Find(code)) {
                    mapped = *known;
                } else {
                    const auto& words = column.words.at(code);
                    if (sampler || (words.availableDwordMask & (1u << 3u)) == 0u || !nullImageDescriptor(info.images[resource], words)) mapped = sampler ? samplerRecord(resource, words) : imageRecord(resource, words);
                    records.Set(code, mapped);
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
    out.faults = static_cast<std::uint32_t>(expectedCount + static_cast<std::size_t>(std::count(outside.begin(), outside.end(), true)));
    return out;
}

std::shared_ptr<const ResolvedImageTables> ResourceMaterializer::ResolveImageTablesShared(const ShaderInfo& info, const ResourceSnapshot& snapshot, std::uint64_t memoKey) {
    if (memoKey == 0u || snapshot.tables.tables.empty() || !TableResolutionReuse()) return std::make_shared<const ResolvedImageTables>(ResolveImageTables(info, snapshot, memoKey));
    auto reducing = reducingSamplers(info, snapshot);
    auto& memo = resolutionMemo();
    auto& totals = tableReuseTotals();
    {
        std::lock_guard lock(memo.mutex);
        const auto found = std::find_if(memo.entries.begin(), memo.entries.end(), [&](const ResolutionMemoEntry& entry) { return entry.memoKey == memoKey && entry.reducing == reducing && sameColumns(entry.tables, snapshot.tables.tables); });
        if (found != memo.entries.end()) {
            std::rotate(memo.entries.begin(), found, found + 1);
            totals.resolutionsShared.fetch_add(1, std::memory_order_relaxed);
            return memo.entries.front().resolved;
        }
    }
    auto resolved = std::make_shared<const ResolvedImageTables>(ResolveImageTables(info, snapshot, memoKey));
    totals.resolutions.fetch_add(1, std::memory_order_relaxed);
    const auto footprint = resolutionFootprint(*resolved, snapshot.tables.tables);
    if (footprint > ResolutionMemoWords) return resolved;
    std::lock_guard lock(memo.mutex);
    memo.entries.insert(memo.entries.begin(), ResolutionMemoEntry{memoKey, snapshot.tables.tables, std::move(reducing), resolved, footprint});
    std::size_t kept = 0;
    std::size_t variantEntries = 0;
    std::size_t total = 0;
    for (std::size_t index = 0; index < memo.entries.size(); index++) {
        auto& entry = memo.entries[index];
        const bool sameVariant = entry.memoKey == memoKey;
        if ((sameVariant && variantEntries == ResolutionMemoVariantEntries) || kept == ResolutionMemoEntries || total + entry.footprint > ResolutionMemoWords) continue;
        if (sameVariant) variantEntries++;
        total += entry.footprint;
        if (kept != index) memo.entries[kept] = std::move(entry);
        kept++;
    }
    memo.entries.resize(kept);
    return resolved;
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
        const bool volumeLoad = (image.dimension == RdnaImageDimension::Dim2D || image.dimension == RdnaImageDimension::Dim2DArray) && image.fmaskCompatible && conversion != FmaskExpansion;
        const bool volumeStore = image.dimension == RdnaImageDimension::Dim2DArray && storage && image.written && !image.read && !image.atomic && image.mipMode != ImageMipMode::DynamicStorage;
        const bool volumePlaneBias = image.dimension == RdnaImageDimension::Dim2D && image.singleMipPlaneBiasCompatible && !storage && image.table == NoTable && conversion == IrBufferFormat::Invalid;
        const bool volumeArraySample = image.dimension == RdnaImageDimension::Dim2DArray && image.volumeArraySampleCompatible && !storage && image.table == NoTable && conversion == IrBufferFormat::Invalid;
        if ((volumeLoad || volumeStore || volumePlaneBias || volumeArraySample) && !depth && packed == IrBufferFormat::Invalid && image.byElements == 0u) {
            auto volume = mode;
            volume.dimension = RdnaImageDimension::Dim3D;
            modes.push_back(volume);
        }
        const bool arrayStore = image.dimension == RdnaImageDimension::Dim3D && storage && image.written && !image.read && !image.atomic && image.mipMode != ImageMipMode::DynamicStorage;
        if (arrayStore && !depth && packed == IrBufferFormat::Invalid && image.byElements == 0u) {
            auto array = mode;
            array.dimension = RdnaImageDimension::Dim2DArray;
            modes.push_back(array);
        }
        if (image.dimension == RdnaImageDimension::Dim2DMsaa && image.fmaskCompatible && image.cubeFaceCompatible && !table && !storage && !depth && conversion == IrBufferFormat::Invalid && packed == IrBufferFormat::Invalid && image.byElements == 0u) {
            auto plane = mode;
            plane.dimension = RdnaImageDimension::Dim2D;
            modes.push_back(plane);
        }
        if (image.dimension == RdnaImageDimension::Dim3D && image.flatVolumeCompatible && !storage && !depth && conversion == IrBufferFormat::Invalid && packed == IrBufferFormat::Invalid) {
            for (const auto dimension : {RdnaImageDimension::Dim2D, RdnaImageDimension::Dim2DArray}) {
                auto plane = mode;
                plane.dimension = dimension;
                modes.push_back(plane);
            }
        }
        if (image.dimension == RdnaImageDimension::Dim3D && image.flatVolumeLineCompatible && !storage && !depth && conversion == IrBufferFormat::Invalid && packed == IrBufferFormat::Invalid) {
            auto line = mode;
            line.dimension = RdnaImageDimension::Dim1D;
            modes.push_back(line);
        }
        if (image.dimension == RdnaImageDimension::Dim2D && storage && image.written && !image.read && !image.atomic && image.mipMode != ImageMipMode::DynamicStorage && !depth && packed == IrBufferFormat::Invalid && image.byElements == 0u) {
            auto line = mode;
            line.dimension = RdnaImageDimension::Dim1D;
            modes.push_back(line);
        }
        const bool cubeFaceStorage = image.written && !image.read && !image.atomic && image.mipMode != ImageMipMode::DynamicStorage;
        if (image.dimension == RdnaImageDimension::Dim2D && image.cubeFaceCompatible && (!storage || cubeFaceStorage) && !depth && conversion == IrBufferFormat::Invalid && packed == IrBufferFormat::Invalid) {
            auto face = mode;
            face.dimension = RdnaImageDimension::Dim2DArray;
            face.cube = true;
            modes.push_back(face);
        }
        if ((image.dimension == RdnaImageDimension::Dim2D || image.dimension == RdnaImageDimension::Dim2DArray) && image.flatLineCompatible && !storage && !depth && conversion == IrBufferFormat::Invalid && packed == IrBufferFormat::Invalid) {
            auto line = mode;
            line.dimension = RdnaImageDimension::Dim1D;
            modes.push_back(line);
        }
        if (image.dimension == RdnaImageDimension::Dim1D && !storage && !depth && conversion == IrBufferFormat::Invalid && packed == IrBufferFormat::Invalid) {
            auto plane = mode;
            plane.dimension = RdnaImageDimension::Dim2D;
            modes.push_back(plane);
        }
        if (image.dimension == RdnaImageDimension::Dim1DArray || image.dimension == RdnaImageDimension::Dim2DArray || image.dimension == RdnaImageDimension::Dim2DMsaaArray) {
            auto plain = mode;
            plain.dimension = image.dimension == RdnaImageDimension::Dim1DArray ? RdnaImageDimension::Dim1D : image.dimension == RdnaImageDimension::Dim2DArray ? RdnaImageDimension::Dim2D : RdnaImageDimension::Dim2DMsaa;
            modes.push_back(plain);
        }
        if (image.dimension == RdnaImageDimension::Dim2DArray && !image.remappedDepth1D) {
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
            if ((image.srgbDecodeCompatible || (image.convertedSamples && convertedSampleCompatible(image))) && image.depthBitsCompatible) append(IrTextureNumericClass::Uint, IrBufferFormat::Format11_11_10UNorm, IrBufferFormat::Invalid, false, false);
            if (image.srgbDecodeCompatible && image.depthBitsCompatible) append(IrTextureNumericClass::Uint, IrBufferFormat::Format10_11_11Float, IrBufferFormat::Invalid, false, false);
            if (storage) {
                for (const auto format : {IrBufferFormat::Format5_6_5UNorm, IrBufferFormat::Format5_5_5_1UNorm, IrBufferFormat::Format4_4_4_4UNorm}) {
                    append(IrTextureNumericClass::Uint, format, IrBufferFormat::Invalid, false, false);
                }
            }
            if (!storage) {
                if (image.depthBitsCompatible) {
                    append(IrTextureNumericClass::Float, IrBufferFormat::Invalid, IrBufferFormat::Invalid, true, false);
                    append(IrTextureNumericClass::Float, IrBufferFormat::Invalid, IrBufferFormat::Invalid, true, true);
                }
                if (image.fmaskCompatible && !table) append(IrTextureNumericClass::Uint, FmaskExpansion, IrBufferFormat::Invalid, false, false);
            }
        }
    }
    if (storage && !image.packed && !image.atomic64 && ((image.written && !image.read) || image.atomic)) {
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
        if (image.dimension == RdnaImageDimension::Dim2DArray && !image.remappedDepth1D) {
            auto plain = mode;
            plain.dimension = RdnaImageDimension::Dim2D;
            modes.push_back(plain);
            mode.cube = true;
            modes.push_back(mode);
        }
    }
    if (image.srgbDecodeFormats != 0u && image.srgbDecodeCompatible && !image.depthCompare && !storage && !image.packed) {
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
    if (image.foldTexelOffsetsOnly && std::ranges::any_of(modes, [](const ImageResource& mode) { return UnnormalizedSampleMismatch(mode) == nullptr; })) {
        std::erase_if(modes, [](const ImageResource& mode) { return UnnormalizedSampleMismatch(mode) != nullptr; });
    }
    return modes;
}

bool ResourceMaterializer::NullImageDescriptor(const ImageResource& image, const DescriptorValue& descriptor) {
    return nullImageDescriptor(image, descriptor);
}

const char* ResourceMaterializer::UnnormalizedSampleMismatch(const ImageResource& image) {
    if (image.table != NoTable) return "samples an image selected at run time";
    if (image.constantSwizzle) return nullptr;
    if ((image.dimension != RdnaImageDimension::Dim1D && image.dimension != RdnaImageDimension::Dim2D) || image.cube) return "samples a 1D-array, 2D-array, 3D, cube or multisampled image";
    if (image.depthCompare) return "is used with depth comparison";
    if (image.conversionFormat != IrBufferFormat::Invalid || image.packed) return "samples an image that needs a format conversion or packed access";
    return nullptr;
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
    if (nullImageDescriptor(image, descriptor)) {
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
    if (image.singleMipPlaneBiasCompatible && image.dimension == RdnaImageDimension::Dim2D && decoded.dimension == RdnaImageDimension::Dim3D) {
        if ((descriptor.dwords[4] & 0x1fffu) != 0u || (descriptor.dwords[3] & 0xff000u) != 0u || (descriptor.dwords[5] & 0xf0u) != 0u) return reject(PoisonReason::Dimension, "2D biased sampling of a volume requires one slice and mip level zero only");
    }
    for (std::uint32_t index = 0u; index < modes.size(); ++index) {
        const auto& mode = modes[index];
        if (mode.constantSwizzle) continue;
        if (((mode.emulatedCompare & EmulatedCompare::Enabled) != 0u) != emulated) continue;
        if (mode.numericClass == decoded.numericClass && mode.dimension == decoded.dimension && mode.conversionFormat == decoded.conversionFormat && mode.packedFormat == decoded.packedFormat && mode.cube == decoded.cube && mode.depthBits == decoded.depthBits && mode.depthUnorm16 == decoded.depthUnorm16 && mode.srgbDecode == decoded.srgbDecode) {
            result.mode = index;
            return result;
        }
    }
    if (image.dimension == RdnaImageDimension::Dim1D && decoded.dimension != RdnaImageDimension::Dim1D) return reject(PoisonReason::Dimension, "image address has too few coordinate components");
    if (image.foldTexelOffsetsOnly) return reject(PoisonReason::UnsupportedOperation, "image sample with a texel offset that is not a constant takes only images an unnormalized S# can sample without VK_KHR_maintenance8 and shaderImageGatherExtended");
    const auto describe = [](IrTextureNumericClass numeric, RdnaImageDimension dimension, IrBufferFormat conversion, IrBufferFormat packed, bool cube, bool depthBits, bool depthUnorm16, bool srgb) {
        return "{class " + std::to_string(static_cast<int>(numeric)) + ", dim " + std::to_string(static_cast<int>(dimension)) + ", conversion " + std::to_string(static_cast<int>(conversion)) + ", packed " + std::to_string(static_cast<int>(packed)) + ", cube " + std::to_string(cube) + ", depthBits " + std::to_string(depthBits) + ", unorm16 " + std::to_string(depthUnorm16) + ", srgb " + std::to_string(srgb) + "}";
    };
    std::string message = "image descriptor is incompatible with the static runtime image interface: descriptor format " + std::to_string(static_cast<int>(format)) + " decodes to " + describe(decoded.numericClass, decoded.dimension, decoded.conversionFormat, decoded.packedFormat, decoded.cube, decoded.depthBits, decoded.depthUnorm16, decoded.srgbDecode) + (emulated ? " (emulated compare)" : "") + "; modes:";
    for (const auto& mode : modes) message += " " + describe(mode.numericClass, mode.dimension, mode.conversionFormat, mode.packedFormat, mode.cube, mode.depthBits, mode.depthUnorm16, mode.srgbDecode);
    char words[96];
    std::snprintf(words, sizeof(words), "; T# %08x %08x %08x %08x %08x %08x %08x %08x", descriptor.dwords[0], descriptor.dwords[1], descriptor.dwords[2], descriptor.dwords[3], descriptor.dwords[4], descriptor.dwords[5], descriptor.dwords[6], descriptor.dwords[7]);
    const auto reason = emulated ? PoisonReason::ColorCompare
        : decoded.fmask ? PoisonReason::Fmask
        : decoded.depthBits ? PoisonReason::DepthBits16
        : decoded.srgbDecode ? PoisonReason::SrgbUnsupported
        : decoded.conversionFormat != IrBufferFormat::Invalid ? PoisonReason::Conversion
        : decoded.dimension == RdnaImageDimension::Dim2DMsaa || decoded.dimension == RdnaImageDimension::Dim2DMsaaArray ? PoisonReason::Multisampled
        : std::none_of(modes.begin(), modes.end(), [&](const ImageResource& mode) { return mode.dimension == decoded.dimension && mode.cube == decoded.cube; }) ? PoisonReason::Dimension
        : PoisonReason::NumericClass;
    return reject(reason, message + words);
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
                if ((memory.imageSampleFlags & RdnaImageSampleFlagOffset) == 0u) continue;
                const auto* address = inst->Argument(2)->Resolve();
                const auto component = GetRdnaImageAddressComponentLayout(memory.imageSampleFlags, 0u);
                const auto argument = component.bitOffset / 32u;
                if (component.bitWidth == 32u && argument < address->ArgumentCount() && address->Argument(argument)->Resolve()->HasImmediate()) continue;
                if ((memory.imageSampleFlags & RdnaImageSampleFlagCompare) != 0u) {
                    images.at(memory.resource).emulatedCompare |= EmulatedCompare::NativeOffsetUnsupported;
                } else if (resources.info.samplers.at(memory.sampler).table == NoTable) {
                    images.at(memory.resource).foldTexelOffsetsOnly = true;
                    resources.info.samplers.at(memory.sampler).foldTexelOffsetsOnly = true;
                }
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
        const bool storage = image.resourceClass == ImageResourceClass::Storage;
        if (modes.empty() || modes.size() > ImageTableAbi::MaxModes || (storage && image.mipMode == ImageMipMode::DynamicStorage)) image.tableOperation = TableOperation::Unsupported;
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
    plan.valueStorage.Reserve(order.size());
    for (const auto* value : order) {
        auto* clone = plan.valueStorage.Create(value->Opcode(), value->Type(), static_cast<std::uint32_t>(plan.valueStorage.Size()));
        clone->SetFlags(value->Flags<std::uint64_t>());
        if (value->HasImmediate()) clone->SetImmediateU64(value->ImmediateU64());
        clone->SetRegister(value->Register());
        clones[value] = clone;
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
        std::vector<IrValue*> arguments;
        std::vector<IrBlock*> phiBlocks;
        arguments.reserve(value->ArgumentCount());
        if (value->IsPhi()) phiBlocks.reserve(value->ArgumentCount());
        for (std::size_t index = 0; index < value->ArgumentCount(); index++) {
            const auto* argument = value->Argument(index);
            arguments.push_back(argument == nullptr ? nullptr : clones.at(argument));
            if (value->IsPhi()) phiBlocks.push_back(blockFor(value->PhiBlock(index)));
        }
        clones.at(value)->SetDetachedOperands(std::move(arguments), std::move(phiBlocks));
    }
    for (auto* root : roots) {
        if (*root != nullptr) *root = clones.at(*root);
    }
    plan.denseValueIds = true;
}

void buildCompactPlan(IrResourcePlan& plan) {
    if (!plan.denseValueIds || plan.valueStorage.Size() >= CompactResourcePlan::NoValue) return;
    CompactResourcePlan compact;
    const auto idOf = [&](const IrValue* value, std::uint32_t& id) {
        if (value == nullptr) return false;
        const auto* resolved = value->Resolve();
        id = resolved->Id();
        return id < plan.valueStorage.Size() && &plan.valueStorage[id] == resolved;
    };
    try {
        compact.values.reserve(plan.valueStorage.Size());
        for (const auto& stored : plan.valueStorage) {
            const auto* value = &stored;
            if (value->ArgumentCount() > 0xffu) return;
            CompactPlanValue entry;
            entry.immediate = value->ImmediateU64();
            entry.flags = value->Flags<std::uint64_t>();
            entry.firstArgument = static_cast<std::uint32_t>(compact.arguments.size());
            entry.registerIndex = value->Register().index;
            entry.type = value->Type();
            entry.opcode = value->Opcode();
            entry.argumentCount = static_cast<std::uint8_t>(value->ArgumentCount());
            entry.hasImmediate = value->HasImmediate();
            for (std::size_t index = 0; index < value->ArgumentCount(); index++) {
                std::uint32_t id = 0;
                if (!idOf(value->Argument(index), id)) return;
                compact.arguments.push_back(id);
            }
            compact.values.push_back(entry);
        }
        compact.sourceDwords.assign(plan.descriptorSources.size() * 8u, CompactResourcePlan::NoValue);
        for (std::size_t source = 0; source < plan.descriptorSources.size(); source++) {
            const auto& descriptor = plan.descriptorSources[source];
            if (descriptor.dwordCount > 8u) return;
            for (std::uint32_t dword = 0; dword < descriptor.dwordCount; dword++) {
                if (!idOf(descriptor.dwords[dword], compact.sourceDwords[source * 8u + dword])) return;
            }
        }
        compact.srtReads.resize(plan.srtReads.size());
        for (std::size_t slot = 0; slot < plan.srtReads.size(); slot++) {
            if (!idOf(plan.srtReads[slot].value, compact.srtReads[slot])) return;
        }
        compact.conditions.assign(plan.controlFlow.size(), CompactResourcePlan::NoValue);
        for (std::size_t block = 0; block < plan.controlFlow.size(); block++) {
            if (plan.controlFlow[block].condition != nullptr && !idOf(plan.controlFlow[block].condition, compact.conditions[block])) return;
        }
    } catch (const std::exception&) {
        return;
    }
    plan.compact = std::move(compact);
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
    for (auto& block : plan.controlFlow) {
        if (block.condition != nullptr && !Detail::Evaluator::MayDecide(source, block.condition)) block.condition = nullptr;
    }
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
    buildCompactPlan(plan);
    return plan;
}

void ResourceMaterializer::CompactPlan(IrResourcePlan& plan) {
    plan.compact = {};
    buildCompactPlan(plan);
}

void ResourceMaterializer::Materialize(const IrResourcePlan& program, const SrtRuntime& runtime, ResourceSnapshot& snapshot) const {
    const IrResourcePlan& plan = program;
    if (!plan.resourceTrackingComplete) {
        throw std::runtime_error("ResourceMaterializer::Materialize requires a completed resource plan");
    }
    if (plan.requiresSpecializationMemory && runtime.readMemory == nullptr) {
        throw std::runtime_error("ResourceMaterializer::Materialize requires runtime memory access for image tables");
    }
    const auto materializeStarted = MaterializeProfiled() ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
    SrtWalker walker;
    ResourceSnapshot nextSnapshot;
    struct ActiveSourcesStorage {};
    auto& activeSources = HostThreadLocal<std::vector<std::uint8_t>, ActiveSourcesStorage>();
    activeSources.clear();
    try {
        materializeSnapshot(plan, runtime, walker, nextSnapshot, activeSources);
        TableSnapshotter(plan, runtime, walker, activeSources, nextSnapshot.tables).Run();
    } catch (...) {
        reportTables();
        throw;
    }
    const auto started = MaterializeProfiled() ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
    materializeForcedDegamma(plan, nextSnapshot);
    materializeTables(plan, nextSnapshot);
    materializeSrtGuards(plan, nextSnapshot);
    if (MaterializeProfiled()) specializationNanoseconds.fetch_add(static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - started).count()), std::memory_order_relaxed);
    snapshot = std::move(nextSnapshot);
    reportTables();
    if (MaterializeProfiled()) materializeNanoseconds.fetch_add(static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - materializeStarted).count()), std::memory_order_relaxed);
}

std::uint64_t ResourceMaterializer::SpecializationNanoseconds() {
    return specializationNanoseconds.load(std::memory_order_relaxed);
}

ImageTableReuseCounts ResourceMaterializer::TableReuse() {
    auto& totals = tableReuseTotals();
    return {totals.unwritten.load(std::memory_order_relaxed), totals.compared.load(std::memory_order_relaxed), totals.wordsRead.load(std::memory_order_relaxed), totals.shared.load(std::memory_order_relaxed), totals.columns.load(std::memory_order_relaxed), totals.resolutions.load(std::memory_order_relaxed), totals.resolutionsShared.load(std::memory_order_relaxed)};
}

std::uint64_t ResourceMaterializer::EvaluateNanoseconds() {
    return evaluateNanoseconds.load(std::memory_order_relaxed);
}

std::uint64_t ResourceMaterializer::MaterializeNanoseconds() {
    return materializeNanoseconds.load(std::memory_order_relaxed);
}

}
