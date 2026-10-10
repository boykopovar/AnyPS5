#ifndef CORE_SHADER_RECOMPILER_IMAGETABLEABI_HPP
#define CORE_SHADER_RECOMPILER_IMAGETABLEABI_HPP

#include <cstdint>
#include <optional>
#include <string_view>

namespace ShaderRecompiler::ImageTableAbi {

inline constexpr std::uint32_t Version = 2;

inline constexpr std::uint32_t HeaderVersion = 0;
inline constexpr std::uint32_t HeaderElementBase = 1;
inline constexpr std::uint32_t HeaderElementCount = 2;
inline constexpr std::uint32_t HeaderSamplerCount = 3;
inline constexpr std::uint32_t HeaderPoisonCount = 4;
inline constexpr std::uint32_t HeaderElementFaults = 5;
inline constexpr std::uint32_t HeaderSamplerFaults = 6;
inline constexpr std::uint32_t HeaderTableCount = 7;
inline constexpr std::uint32_t HeaderWords = 8;

inline constexpr std::uint32_t TableMapStart = 0;
inline constexpr std::uint32_t TableKeyCount = 1;
inline constexpr std::uint32_t TableSizeLow = 2;
inline constexpr std::uint32_t TableSizeHigh = 3;
inline constexpr std::uint32_t TableBaseLow = 4;
inline constexpr std::uint32_t TableBaseHigh = 5;
inline constexpr std::uint32_t TableOutsideCode = 6;
inline constexpr std::uint32_t TableFault = 7;
inline constexpr std::uint32_t TableWords = 8;

inline constexpr std::uint32_t ImageRecordFlags = 0;
inline constexpr std::uint32_t ImageRecordDword3 = 1;
inline constexpr std::uint32_t ImageRecordElement = 2;
inline constexpr std::uint32_t ImageRecordWords = 3;

inline constexpr std::uint32_t SamplerRecordFlags = 0;
inline constexpr std::uint32_t SamplerRecordElement = 1;
inline constexpr std::uint32_t SamplerRecordWords = 2;
inline constexpr std::uint32_t SamplerEntryElements = 4;

inline constexpr std::uint32_t NullCode = 0;
inline constexpr std::uint32_t PoisonFlag = 0x80000000u;
inline constexpr std::uint32_t ModeShift = 1;
inline constexpr std::uint32_t ModeMask = 0xffu;

inline constexpr std::uint32_t OutsideSnapshotPoison = 0;
inline constexpr std::uint32_t OutsideDomainPoison = 1;
inline constexpr std::uint32_t FirstEntryPoison = 2;

inline constexpr std::uint32_t MaxKeys = 65536;
inline constexpr std::uint32_t MaxModes = 256;
inline constexpr std::uint32_t ImageElementBudget = 65536;
inline constexpr std::uint32_t SamplerElementBudget = 1024;

enum class PoisonReason : std::uint32_t {
    OutsideSnapshot,
    OutsideDomain,
    Unmapped,
    Inactive,
    NotBuffer,
    NotImage,
    InvalidFormat,
    Dimension,
    Multisampled,
    Fmask,
    ColorCompare,
    DepthBits16,
    Conversion,
    SrgbUnsupported,
    NumericClass,
    Budget,
    Reduction,
    Unnormalized,
    HeapWritten,
    DriverRejected,
    UnsupportedOperation,
    Count
};

constexpr std::string_view PoisonReasonName(PoisonReason reason) {
    switch (reason) {
        case PoisonReason::OutsideSnapshot: return "outside the snapshot";
        case PoisonReason::OutsideDomain: return "outside the key domain";
        case PoisonReason::Unmapped: return "unmapped";
        case PoisonReason::Inactive: return "inactive source";
        case PoisonReason::NotBuffer: return "table V# is not a buffer";
        case PoisonReason::NotImage: return "not an image";
        case PoisonReason::InvalidFormat: return "invalid format";
        case PoisonReason::Dimension: return "dimension";
        case PoisonReason::Multisampled: return "multisampled";
        case PoisonReason::Fmask: return "FMASK";
        case PoisonReason::ColorCompare: return "color texture under comparison";
        case PoisonReason::DepthBits16: return "depth bits with 16-bit results";
        case PoisonReason::Conversion: return "format conversion";
        case PoisonReason::SrgbUnsupported: return "sRGB decode";
        case PoisonReason::NumericClass: return "numeric class";
        case PoisonReason::Budget: return "over budget";
        case PoisonReason::Reduction: return "min or max reduction";
        case PoisonReason::Unnormalized: return "unnormalized coordinates";
        case PoisonReason::HeapWritten: return "table written by its dispatch";
        case PoisonReason::DriverRejected: return "rejected by the driver";
        case PoisonReason::UnsupportedOperation: return "unsupported operation";
        case PoisonReason::Count: break;
    }
    return "unknown";
}

constexpr std::uint32_t PoisonCode(std::uint32_t poison) {
    return PoisonFlag | poison;
}

constexpr std::uint32_t TableHeader(std::uint32_t table) {
    return HeaderWords + TableWords * table;
}

constexpr std::optional<std::uint32_t> ScalarBufferDword(std::uint32_t offset, std::uint32_t immediate, std::uint64_t size) {
    const std::uint64_t address = static_cast<std::uint64_t>(offset) + immediate;
    if (address > 0xffffffffull) return std::nullopt;
    const std::uint32_t dword = static_cast<std::uint32_t>(address) & ~3u;
    if (static_cast<std::uint64_t>(dword) + 4u > size) return std::nullopt;
    return dword;
}

constexpr std::optional<std::uint64_t> ScalarAddressDword(std::uint64_t base, std::uint32_t offset, std::uint32_t immediate) {
    const std::uint64_t first = base + (offset & ~3u);
    if (first < base) return std::nullopt;
    const std::int64_t delta = static_cast<std::int32_t>(immediate & ~3u);
    const std::uint64_t magnitude = delta < 0 ? static_cast<std::uint64_t>(-delta) : static_cast<std::uint64_t>(delta);
    if (delta < 0) return magnitude > first ? std::nullopt : std::optional<std::uint64_t>(first - magnitude);
    return first + magnitude < first ? std::nullopt : std::optional<std::uint64_t>(first + magnitude);
}

constexpr std::uint32_t ScalarAddressRecords(std::uint32_t stride) {
    return stride <= 1u ? 0xffffffffu * stride : 0xffffffffu / stride + 1u;
}

constexpr std::uint32_t KeyRecords(std::uint32_t maxKey, std::uint32_t stride) {
    return maxKey != 0xffffffffu && static_cast<std::uint64_t>(maxKey) * stride <= 0xffffffffull ? maxKey + 1u : 0xffffffffu;
}

constexpr std::uint32_t ScalarBufferRecords(std::uint32_t addend, std::uint32_t stride, std::uint32_t immediate, std::uint64_t size) {
    const std::uint64_t start = static_cast<std::uint64_t>(addend) + immediate;
    const std::uint64_t rounded = size & ~std::uint64_t{3u};
    const std::uint64_t limit = rounded < 0x100000000ull ? rounded : 0x100000000ull;
    if (stride == 0u || start >= limit) return 0u;
    const std::uint64_t records = (limit - start + stride - 1u) / stride;
    return records > 0xffffffffull ? 0xffffffffu : static_cast<std::uint32_t>(records);
}

}

#endif
